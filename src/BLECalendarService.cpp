#include "BLECalendarService.h"

#include <HardwareSerial.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>

static BLECalendarService* bleCalendarServiceInstance = nullptr;
static BLEServer* bleServer = nullptr;
static BLECharacteristic* calendarWriteCharacteristic = nullptr;
static BLECharacteristic* calendarNotifyCharacteristic = nullptr;
static bool clientConnected = false;

class CalendarServiceCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    Serial.printf("[%lu] [BLE-CAL] Client connected\n", millis());
    clientConnected = true;
    if (bleCalendarServiceInstance) {
      // Trigger auto-sync or notification
    }
  }

  void onDisconnect(BLEServer* pServer) override {
    Serial.printf("[%lu] [BLE-CAL] Client disconnected\n", millis());
    clientConnected = false;
    // Restart advertising to allow new connections
    BLEDevice::startAdvertising();
  }
};

class CalendarCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    if (!bleCalendarServiceInstance) return;

    std::string receivedData = pCharacteristic->getValue();
    Serial.printf("[%lu] [BLE-CAL] Received calendar data: %u bytes\n", millis(), receivedData.length());

    // Parse and handle the calendar data
    auto* calData = parseCalendarJSON(receivedData);
    if (calData) {
      // Save to file
      calData->saveToFile();

      // Call user callback
      if (bleCalendarServiceInstance) {
        bleCalendarServiceInstance->invokeCalendarReceivedCallback(*calData);
      }

      delete calData;
    } else {
      Serial.printf("[%lu] [BLE-CAL] Failed to parse calendar JSON\n", millis());
    }
  }

  void onRead(BLECharacteristic* pCharacteristic) override {
    Serial.printf("[%lu] [BLE-CAL] Calendar data read\n", millis());
  }
};

BLECalendarService::BLECalendarService() {
  bleCalendarServiceInstance = this;
}

BLECalendarService::~BLECalendarService() {
  stop();
  bleCalendarServiceInstance = nullptr;
}

bool BLECalendarService::begin(const char* deviceName) {
  if (isRunning_) {
    Serial.printf("[%lu] [BLE-CAL] Service already running\n", millis());
    return true;
  }

  try {
    Serial.printf("[%lu] [BLE-CAL] Initializing BLE Calendar Service...\n", millis());

    // Initialize BLE device
    BLEDevice::init(deviceName);

    // Create server
    bleServer = BLEDevice::createServer();
    if (!bleServer) {
      Serial.printf("[%lu] [BLE-CAL] Failed to create BLE server\n", millis());
      return false;
    }

    bleServer->setCallbacks(new CalendarServiceCallbacks());

    // Create calendar service
    BLEService* calendarService = bleServer->createService(CALENDAR_SERVICE_UUID);
    if (!calendarService) {
      Serial.printf("[%lu] [BLE-CAL] Failed to create calendar service\n", millis());
      return false;
    }

    // Create write characteristic (for receiving calendar data)
    calendarWriteCharacteristic = calendarService->createCharacteristic(
        CALENDAR_WRITE_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);

    if (calendarWriteCharacteristic) {
      calendarWriteCharacteristic->setCallbacks(new CalendarCharacteristicCallbacks());
    }

    // Create notify characteristic (for sending status/confirmation)
    calendarNotifyCharacteristic = calendarService->createCharacteristic(
        CALENDAR_NOTIFY_UUID,
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);

    if (calendarNotifyCharacteristic) {
      calendarNotifyCharacteristic->addDescriptor(new BLE2902());
    }

    // Start service
    calendarService->start();

    // Start advertising
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(CALENDAR_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // helps with iPhone connections
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    isRunning_ = true;
    Serial.printf("[%lu] [BLE-CAL] BLE Calendar Service started, advertising as '%s'\n", millis(), deviceName);
    return true;

  } catch (const std::exception& e) {
    Serial.printf("[%lu] [BLE-CAL] Exception during initialization: %s\n", millis(), e.what());
    return false;
  }
}

void BLECalendarService::stop() {
  if (!isRunning_) return;

  try {
    if (bleServer) {
      BLEDevice::getAdvertising()->stop();
      bleServer = nullptr;
    }
    BLEDevice::deinit(true);
    isRunning_ = false;
    clientConnected = false;
    Serial.printf("[%lu] [BLE-CAL] BLE Calendar Service stopped\n", millis());
  } catch (const std::exception& e) {
    Serial.printf("[%lu] [BLE-CAL] Exception during stop: %s\n", millis(), e.what());
  }
}

bool BLECalendarService::sendCalendarData(const CalendarData& data) {
  if (!isRunning_ || !calendarNotifyCharacteristic) {
    return false;
  }

  try {
    JsonDocument doc;

    // Serialize calendar data
    JsonObject todayObj = doc["today"].to<JsonObject>();
    todayObj["date"] = data.today.date;
    JsonArray todayEvents = todayObj["events"].to<JsonArray>();
    for (const auto& event : data.today.events) {
      JsonObject eventObj = todayEvents.add<JsonObject>();
      eventObj["title"] = event.title;
      eventObj["time"] = event.time;
    }

    std::string jsonStr;
    serializeJson(doc, jsonStr);

    calendarNotifyCharacteristic->setValue(jsonStr);
    calendarNotifyCharacteristic->notify();

    return true;
  } catch (const std::exception& e) {
    Serial.printf("[%lu] [BLE-CAL] Exception sending data: %s\n", millis(), e.what());
    return false;
  }
}

void BLECalendarService::handleTasks() {
  // Periodic maintenance tasks
  // Could be used for auto-sync, connection checks, etc.
  isClientConnected_ = clientConnected;
}

int32_t BLECalendarService::getClientRSSI() const {
  if (!bleServer || !clientConnected) {
    return -128;  // No connection
  }

  // Note: Standard ESP32 BLE library doesn't provide easy RSSI access for connected clients
  // Return a placeholder value. Could be enhanced with custom RSSI tracking if needed
  return -60;
}
