#include "FileTransferActivity.h"

#include <GfxRenderer.h>
#include <HardwareSerial.h>
#include <qrcode.h>
#include <WiFi.h>

#include "CalibreWirelessActivity.h"
#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "WifiSelectionActivity.h"
#include "fontIds.h"
#include "util/UrlUtils.h"

namespace {
constexpr unsigned long CLIENT_HANDLE_INTERVAL_MS = 50;
constexpr const char* AP_HOSTNAME = "crosspoint";
}

void FileTransferActivity::taskTrampoline(void* param) {
  auto* self = static_cast<FileTransferActivity*>(param);
  self->displayTaskLoop();
}

void FileTransferActivity::onEnter() {
  ActivityWithSubactivity::onEnter();

  renderingMutex = xSemaphoreCreateMutex();
  updateRequired = true;
  state = FileTransferActivityState::PROTOCOL_SELECTION;

  // Enter the protocol selection subactivity
  enterNewActivity(new ProtocolSelectionActivity(
      renderer, mappedInput, [this](FileTransferProtocol protocol) { onProtocolSelected(protocol); }));

  xTaskCreate(&FileTransferActivity::taskTrampoline, "FileTransferTask",
              4096,  // Stack size
              this,  // Parameters
              1,     // Priority
              &displayTaskHandle  // Task handle
  );
}

void FileTransferActivity::onExit() {
  stopServer();

  ActivityWithSubactivity::onExit();

  // Wait until not rendering to delete task
  if (renderingMutex) {
    xSemaphoreTake(renderingMutex, portMAX_DELAY);
    if (displayTaskHandle) {
      vTaskDelete(displayTaskHandle);
      displayTaskHandle = nullptr;
    }
    vSemaphoreDelete(renderingMutex);
    renderingMutex = nullptr;
  }
}

void FileTransferActivity::loop() {
  // Forward loop to subactivity if active
  if (subActivity) {
    subActivity->loop();
    return;
  }

  // Handle server clients
  if (state == FileTransferActivityState::SERVER_RUNNING) {
    unsigned long now = millis();
    if (now - lastHandleClientTime >= CLIENT_HANDLE_INTERVAL_MS) {
      lastHandleClientTime = now;
      if (webServer && webServer->isRunning()) {
        webServer->handleClient();
      }
      if (ftpServer && ftpServer->running()) {
        ftpServer->handleClient();
      }
    }
  }

  // Handle back button to stop server and return
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (state == FileTransferActivityState::SERVER_RUNNING) {
      state = FileTransferActivityState::SHUTTING_DOWN;
      stopServer();
      updateRequired = true;
    } else {
      onGoBack();
    }
    return;
  }

  // Handle activity state transitions
  switch (state) {
    case FileTransferActivityState::PROTOCOL_SELECTION: {
      // Protocol selection is handled via subactivity
      break;
    }
    case FileTransferActivityState::MODE_SELECTION: {
      // Mode selection is handled via subactivity
      break;
    }
    case FileTransferActivityState::WIFI_SELECTION: {
      // WiFi selection is handled via subactivity
      break;
    }
    case FileTransferActivityState::AP_STARTING: {
      // AP startup is handled by apStartupTask
      break;
    }
    case FileTransferActivityState::SERVER_RUNNING: {
      // Server is running, handled in client handling above
      break;
    }
    case FileTransferActivityState::SHUTTING_DOWN: {
      onGoBack();
      break;
    }
  }
}

void FileTransferActivity::onProtocolSelected(FileTransferProtocol protocol) {
  // Exit the ProtocolSelectionActivity subactivity
  exitActivity();

  selectedProtocol = protocol;
  state = FileTransferActivityState::MODE_SELECTION;
  updateRequired = true;

  // Enter network mode selection subactivity
  enterNewActivity(new NetworkModeSelectionActivity(
      renderer, mappedInput, [this](NetworkMode mode) { onNetworkModeSelected(mode); }, onGoBack));
}

void FileTransferActivity::onNetworkModeSelected(NetworkMode mode) {
  // Exit the NetworkModeSelectionActivity subactivity
  exitActivity();

  networkMode = mode;
  isApMode = (mode == NetworkMode::CREATE_HOTSPOT);

  if (isApMode) {
    state = FileTransferActivityState::AP_STARTING;
    
    // Start AP in a separate task to avoid blocking the main loop
    xTaskCreate(&FileTransferActivity::apStartupTaskTrampoline, "APStartupTask",
                4096,  // Stack size
                this,  // Parameters
                1,     // Priority
                &apStartupTaskHandle);
  } else {
    state = FileTransferActivityState::WIFI_SELECTION;
    enterNewActivity(new WifiSelectionActivity(
        renderer, mappedInput, [this](bool connected) { onWifiSelectionComplete(connected); }));
  }
  updateRequired = true;
}

void FileTransferActivity::onWifiSelectionComplete(bool connected) {
  // Exit the WifiSelectionActivity subactivity
  exitActivity();

  if (connected) {
    connectedSSID = WiFi.SSID().c_str();
    connectedIP = WiFi.localIP().toString().c_str();
    startServer();
    state = FileTransferActivityState::SERVER_RUNNING;
  } else {
    state = FileTransferActivityState::MODE_SELECTION;
  }
  updateRequired = true;
}

void FileTransferActivity::startAccessPoint() {
  // Set up AP with name based on device
  const char* apName = "Crosspoint Reader";
  const char* apPassword = "crosspoint";

  Serial.printf("[%lu] Starting Access Point: %s\n", millis(), apName);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName, apPassword);

  connectedSSID = apName;
  connectedIP = WiFi.softAPIP().toString().c_str();

  Serial.printf("[%lu] AP started. IP: %s\n", millis(), connectedIP.c_str());

  startServer();
  state = FileTransferActivityState::SERVER_RUNNING;
  updateRequired = true;
}

void FileTransferActivity::startServer() {
  if (selectedProtocol == FileTransferProtocol::HTTP) {
    webServer.reset(new CrossPointWebServer());
    webServer->begin();
    Serial.printf("[%lu] HTTP server started\n", millis());
  } else {  // FTP
    ftpServer.reset(new CrossPointFtpServer());
    if (!ftpServer->begin()) {
      Serial.printf("[%lu] Failed to start FTP server\n", millis());
      stopServer();
      return;
    }
  }

  Serial.printf("[%lu] File transfer server started\n", millis());
  lastHandleClientTime = millis();
}

void FileTransferActivity::stopServer() {
  if (webServer) {
    webServer->stop();
    webServer.reset();
  }
  if (ftpServer) {
    ftpServer->stop();
    ftpServer.reset();
  }

  if (isApMode && WiFi.getMode() == WIFI_AP) {
    WiFi.softAPdisconnect(true);
    Serial.printf("[%lu] AP stopped\n", millis());
  }
}

void FileTransferActivity::render() const {
  // Don't render if a subactivity is active - let the subactivity handle rendering
  if (subActivity) {
    return;
  }

  renderer.clearScreen();

  switch (state) {
    case FileTransferActivityState::PROTOCOL_SELECTION:
    case FileTransferActivityState::MODE_SELECTION:
    case FileTransferActivityState::WIFI_SELECTION:
      // These states use subactivities, no need to render here
      break;

    case FileTransferActivityState::AP_STARTING:
      renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2 - 20, "Starting Hotspot...", true,
                                EpdFontFamily::BOLD);
      break;

    case FileTransferActivityState::SERVER_RUNNING:
      renderServerRunning();
      break;

    case FileTransferActivityState::SHUTTING_DOWN:
      renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2 - 20, "Shutting down...", true,
                                EpdFontFamily::BOLD);
      break;
  }

  renderer.displayBuffer();
}

namespace {
void drawQRCode(const GfxRenderer& renderer, const int x, const int y, const std::string& data) {
  // Structure to manage the QR code
  QRCode qrcode;
  uint8_t qrcodeBytes[qrcode_getBufferSize(4)];
  
  qrcode_initText(&qrcode, qrcodeBytes, 4, ECC_LOW, data.c_str());
  constexpr uint8_t px = 6;  // pixels per module
  
  for (uint8_t cy = 0; cy < qrcode.size; cy++) {
    for (uint8_t cx = 0; cx < qrcode.size; cx++) {
      if (qrcode_getModule(&qrcode, cx, cy)) {
        renderer.fillRect(x + px * cx, y + px * cy, px, px, true);
      }
    }
  }
}
}  // namespace

void FileTransferActivity::renderServerRunning() const {
  // Use consistent line spacing
  constexpr int LINE_SPACING = 28;  // Space between lines

  const char* protocolName = selectedProtocol == FileTransferProtocol::HTTP ? "HTTP" : "FTP";
  const std::string title = std::string("File Transfer (") + protocolName + ")";
  renderer.drawCenteredText(UI_12_FONT_ID, 15, title.c_str(), true, EpdFontFamily::BOLD);

  if (isApMode) {
    // AP mode display - center the content block
    int startY = 55;

    renderer.drawCenteredText(UI_10_FONT_ID, startY, "Hotspot Mode", true, EpdFontFamily::BOLD);

    std::string ssidInfo = "Network: " + connectedSSID;
    renderer.drawCenteredText(UI_10_FONT_ID, startY + LINE_SPACING, ssidInfo.c_str());

    renderer.drawCenteredText(SMALL_FONT_ID, startY + LINE_SPACING * 2, 
                             "Connect your device to this WiFi network");

    renderer.drawCenteredText(SMALL_FONT_ID, startY + LINE_SPACING * 3,
                             "or scan QR code with your phone to connect to WiFi:");
    
    // Show QR code for WiFi
    const std::string wifiConfig = std::string("WIFI:S:") + connectedSSID + ";;";
    drawQRCode(renderer, (480 - 6 * 33) / 2, startY + LINE_SPACING * 4, wifiConfig);

    startY += 6 * 29 + 3 * LINE_SPACING;

    // Show URL based on protocol
    std::string serverUrl;
    if (selectedProtocol == FileTransferProtocol::HTTP) {
      serverUrl = std::string("http://") + AP_HOSTNAME + ".local/";
      renderer.drawCenteredText(UI_10_FONT_ID, startY + LINE_SPACING * 3, 
                               serverUrl.c_str(), true, EpdFontFamily::BOLD);

      std::string ipUrl = "or http://" + connectedIP + "/";
      renderer.drawCenteredText(SMALL_FONT_ID, startY + LINE_SPACING * 4, ipUrl.c_str());
      renderer.drawCenteredText(SMALL_FONT_ID, startY + LINE_SPACING * 5, 
                               "Open this URL in your browser");
    } else {
      // FTP URL with credentials
      serverUrl = std::string("ftp://") + SETTINGS.ftpUsername + ":" + 
                  SETTINGS.ftpPassword + "@" + connectedIP + "/";
      renderer.drawCenteredText(UI_10_FONT_ID, startY + LINE_SPACING * 3, 
                               serverUrl.c_str(), true, EpdFontFamily::BOLD);

      std::string ftpInfo = "User: " + SETTINGS.ftpUsername + " | Pass: " + 
                           SETTINGS.ftpPassword;
      renderer.drawCenteredText(SMALL_FONT_ID, startY + LINE_SPACING * 4, ftpInfo.c_str());
      renderer.drawCenteredText(SMALL_FONT_ID, startY + LINE_SPACING * 5, 
                               "Use FTP client or scan QR code:");
    }

    // Show QR code for server URL
    drawQRCode(renderer, (480 - 6 * 33) / 2, startY + LINE_SPACING * 6, serverUrl);
  } else {
    // STA mode display
    const int startY = 65;

    std::string ssidInfo = "Network: " + connectedSSID;
    if (ssidInfo.length() > 28) {
      ssidInfo.replace(25, ssidInfo.length() - 25, "...");
    }
    renderer.drawCenteredText(UI_10_FONT_ID, startY, ssidInfo.c_str());

    std::string ipInfo = "IP Address: " + connectedIP;
    renderer.drawCenteredText(UI_10_FONT_ID, startY + LINE_SPACING, ipInfo.c_str());

    // Show server URL based on protocol
    std::string serverUrl;
    if (selectedProtocol == FileTransferProtocol::HTTP) {
      serverUrl = "http://" + connectedIP + "/";
      renderer.drawCenteredText(UI_10_FONT_ID, startY + LINE_SPACING * 2, 
                               serverUrl.c_str(), true, EpdFontFamily::BOLD);

      std::string hostnameUrl = std::string("or http://") + AP_HOSTNAME + ".local/";
      renderer.drawCenteredText(SMALL_FONT_ID, startY + LINE_SPACING * 3, hostnameUrl.c_str());

      renderer.drawCenteredText(SMALL_FONT_ID, startY + LINE_SPACING * 4, 
                               "Open this URL in your browser");
    } else {
      // FTP URL with credentials
      serverUrl = std::string("ftp://") + SETTINGS.ftpUsername + ":" + 
                  SETTINGS.ftpPassword + "@" + connectedIP + "/";
      renderer.drawCenteredText(UI_10_FONT_ID, startY + LINE_SPACING * 2, 
                               serverUrl.c_str(), true, EpdFontFamily::BOLD);

      std::string ftpInfo = "User: " + SETTINGS.ftpUsername + " | Pass: " + 
                           SETTINGS.ftpPassword;
      renderer.drawCenteredText(SMALL_FONT_ID, startY + LINE_SPACING * 3, ftpInfo.c_str());

      renderer.drawCenteredText(SMALL_FONT_ID, startY + LINE_SPACING * 4, 
                               "Use FTP client or scan QR code:");
    }

    // Show QR code for server URL
    renderer.drawCenteredText(SMALL_FONT_ID, startY + LINE_SPACING * 5, 
                             "or scan QR code with your phone:");
    drawQRCode(renderer, (480 - 6 * 33) / 2, startY + LINE_SPACING * 6, serverUrl);
  }

  const auto labels = mappedInput.mapLabels("« Exit", "", "", "");
  renderer.drawButtonHints(UI_10_FONT_ID, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

[[noreturn]] void FileTransferActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      render();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void FileTransferActivity::apStartupTaskTrampoline(void* param) {
  auto* self = static_cast<FileTransferActivity*>(param);
  self->apStartupTask();
}

void FileTransferActivity::apStartupTask() {
  // This runs in a separate task so the blocking WiFi.softAP() call doesn't hang the main loop
  Serial.printf("[%lu] [FileTransfer] AP startup task starting\n", millis());
  
  const char* apName = SETTINGS.hotspotSSID.c_str();
  const char* apPassword = "crosspoint";
  
  Serial.printf("[%lu] [FileTransfer] Calling WiFi.mode(WIFI_AP)\n", millis());
  WiFi.mode(WIFI_AP);
  
  Serial.printf("[%lu] [FileTransfer] Calling WiFi.softAP - this may block for several seconds\n", millis());
  WiFi.softAP(apName, apPassword);
  
  connectedSSID = apName;
  connectedIP = WiFi.softAPIP().toString().c_str();
  
  Serial.printf("[%lu] [FileTransfer] AP started. IP: %s\n", millis(), connectedIP.c_str());
  
  // Start the server now that AP is ready
  startServer();
  state = FileTransferActivityState::SERVER_RUNNING;
  updateRequired = true;
  
  // Delete this task
  vTaskDelete(nullptr);
}
