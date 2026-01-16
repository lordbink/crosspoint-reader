#include "SleepActivity.h"

#include <Epub.h>
#include <GfxRenderer.h>
#include <SDCardManager.h>
#include <Xtc.h>

#include "CalendarData.h"
#include "CalendarEventState.h"
#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "fontIds.h"
#include "images/CrossLarge.h"
#include "util/StringUtils.h"

void SleepActivity::onEnter() {
  Activity::onEnter();
  renderPopup("Entering Sleep...");

  if (SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::BLANK) {
    return renderBlankSleepScreen();
  }

  if (SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM) {
    return renderCustomSleepScreen();
  }

  if (SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::COVER) {
    return renderCoverSleepScreen();
  }

  if (SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::CALENDAR) {
    return renderCalendarSleepScreen();
  }

  renderDefaultSleepScreen();
}

void SleepActivity::renderPopup(const char* message) const {
  const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, message, EpdFontFamily::BOLD);
  constexpr int margin = 20;
  const int x = (renderer.getScreenWidth() - textWidth - margin * 2) / 2;
  constexpr int y = 117;
  const int w = textWidth + margin * 2;
  const int h = renderer.getLineHeight(UI_12_FONT_ID) + margin * 2;
  // renderer.clearScreen();
  renderer.fillRect(x - 5, y - 5, w + 10, h + 10, true);
  renderer.fillRect(x + 5, y + 5, w - 10, h - 10, false);
  renderer.drawText(UI_12_FONT_ID, x + margin, y + margin, message, true, EpdFontFamily::BOLD);
  renderer.displayBuffer();
}

void SleepActivity::renderCustomSleepScreen() const {
  // Check if we have a /sleep directory
  auto dir = SdMan.open("/sleep");
  if (dir && dir.isDirectory()) {
    std::vector<std::string> files;
    char name[500];
    // collect all valid BMP files
    for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
      if (file.isDirectory()) {
        file.close();
        continue;
      }
      file.getName(name, sizeof(name));
      auto filename = std::string(name);
      if (filename[0] == '.') {
        file.close();
        continue;
      }

      if (filename.substr(filename.length() - 4) != ".bmp") {
        Serial.printf("[%lu] [SLP] Skipping non-.bmp file name: %s\n", millis(), name);
        file.close();
        continue;
      }
      Bitmap bitmap(file);
      if (bitmap.parseHeaders() != BmpReaderError::Ok) {
        Serial.printf("[%lu] [SLP] Skipping invalid BMP file: %s\n", millis(), name);
        file.close();
        continue;
      }
      files.emplace_back(filename);
      file.close();
    }
    const auto numFiles = files.size();
    if (numFiles > 0) {
      // Generate a random number between 1 and numFiles
      const auto randomFileIndex = random(numFiles);
      const auto filename = "/sleep/" + files[randomFileIndex];
      FsFile file;
      if (SdMan.openFileForRead("SLP", filename, file)) {
        Serial.printf("[%lu] [SLP] Randomly loading: /sleep/%s\n", millis(), files[randomFileIndex].c_str());
        delay(100);
        Bitmap bitmap(file);
        if (bitmap.parseHeaders() == BmpReaderError::Ok) {
          renderBitmapSleepScreen(bitmap);
          dir.close();
          return;
        }
      }
    }
  }
  if (dir) dir.close();

  // Look for sleep.bmp on the root of the sd card to determine if we should
  // render a custom sleep screen instead of the default.
  FsFile file;
  if (SdMan.openFileForRead("SLP", "/sleep.bmp", file)) {
    Bitmap bitmap(file);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      Serial.printf("[%lu] [SLP] Loading: /sleep.bmp\n", millis());
      renderBitmapSleepScreen(bitmap);
      return;
    }
  }

  renderDefaultSleepScreen();
}

void SleepActivity::renderDefaultSleepScreen() const {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  renderer.drawImage(CrossLarge, (pageWidth + 128) / 2, (pageHeight - 128) / 2, 128, 128);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 70, "CrossPoint", true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 95, "SLEEPING");

  // Make sleep screen dark unless light is selected in settings
  if (SETTINGS.sleepScreen != CrossPointSettings::SLEEP_SCREEN_MODE::LIGHT) {
    renderer.invertScreen();
  }

  renderer.displayBuffer(EInkDisplay::HALF_REFRESH);
}

void SleepActivity::renderBitmapSleepScreen(const Bitmap& bitmap) const {
  int x, y;
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  float cropX = 0, cropY = 0;

  Serial.printf("[%lu] [SLP] bitmap %d x %d, screen %d x %d\n", millis(), bitmap.getWidth(), bitmap.getHeight(),
                pageWidth, pageHeight);
  if (bitmap.getWidth() > pageWidth || bitmap.getHeight() > pageHeight) {
    // image will scale, make sure placement is right
    float ratio = static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
    const float screenRatio = static_cast<float>(pageWidth) / static_cast<float>(pageHeight);

    Serial.printf("[%lu] [SLP] bitmap ratio: %f, screen ratio: %f\n", millis(), ratio, screenRatio);
    if (ratio > screenRatio) {
      // image wider than viewport ratio, scaled down image needs to be centered vertically
      if (SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP) {
        cropX = 1.0f - (screenRatio / ratio);
        Serial.printf("[%lu] [SLP] Cropping bitmap x: %f\n", millis(), cropX);
        ratio = (1.0f - cropX) * static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
      }
      x = 0;
      y = std::round((static_cast<float>(pageHeight) - static_cast<float>(pageWidth) / ratio) / 2);
      Serial.printf("[%lu] [SLP] Centering with ratio %f to y=%d\n", millis(), ratio, y);
    } else {
      // image taller than viewport ratio, scaled down image needs to be centered horizontally
      if (SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP) {
        cropY = 1.0f - (ratio / screenRatio);
        Serial.printf("[%lu] [SLP] Cropping bitmap y: %f\n", millis(), cropY);
        ratio = static_cast<float>(bitmap.getWidth()) / ((1.0f - cropY) * static_cast<float>(bitmap.getHeight()));
      }
      x = std::round((pageWidth - pageHeight * ratio) / 2);
      y = 0;
      Serial.printf("[%lu] [SLP] Centering with ratio %f to x=%d\n", millis(), ratio, x);
    }
  } else {
    // center the image
    x = (pageWidth - bitmap.getWidth()) / 2;
    y = (pageHeight - bitmap.getHeight()) / 2;
  }

  Serial.printf("[%lu] [SLP] drawing to %d x %d\n", millis(), x, y);
  renderer.clearScreen();
  renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
  renderer.displayBuffer(EInkDisplay::HALF_REFRESH);

  if (bitmap.hasGreyscale()) {
    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
    renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
    renderer.copyGrayscaleLsbBuffers();

    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
    renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
    renderer.copyGrayscaleMsbBuffers();

    renderer.displayGrayBuffer();
    renderer.setRenderMode(GfxRenderer::BW);
  }
}

void SleepActivity::renderCoverSleepScreen() const {
  if (APP_STATE.openEpubPath.empty()) {
    return renderDefaultSleepScreen();
  }

  std::string coverBmpPath;

  if (StringUtils::checkFileExtension(APP_STATE.openEpubPath, ".xtc") ||
      StringUtils::checkFileExtension(APP_STATE.openEpubPath, ".xtch")) {
    // Handle XTC file
    Xtc lastXtc(APP_STATE.openEpubPath, "/.crosspoint");
    if (!lastXtc.load()) {
      Serial.println("[SLP] Failed to load last XTC");
      return renderDefaultSleepScreen();
    }

    if (!lastXtc.generateCoverBmp()) {
      Serial.println("[SLP] Failed to generate XTC cover bmp");
      return renderDefaultSleepScreen();
    }

    coverBmpPath = lastXtc.getCoverBmpPath();
  } else if (StringUtils::checkFileExtension(APP_STATE.openEpubPath, ".epub")) {
    // Handle EPUB file
    Epub lastEpub(APP_STATE.openEpubPath, "/.crosspoint");
    if (!lastEpub.load()) {
      Serial.println("[SLP] Failed to load last epub");
      return renderDefaultSleepScreen();
    }

    if (!lastEpub.generateCoverBmp()) {
      Serial.println("[SLP] Failed to generate cover bmp");
      return renderDefaultSleepScreen();
    }

    coverBmpPath = lastEpub.getCoverBmpPath();
  } else {
    return renderDefaultSleepScreen();
  }

  FsFile file;
  if (SdMan.openFileForRead("SLP", coverBmpPath, file)) {
    Bitmap bitmap(file);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      renderBitmapSleepScreen(bitmap);
      return;
    }
  }

  renderDefaultSleepScreen();
}

void SleepActivity::renderBlankSleepScreen() const {
  renderer.clearScreen();
  renderer.displayBuffer(EInkDisplay::HALF_REFRESH);
}

void SleepActivity::renderCalendarSleepScreen() const {
  // Try to load calendar data from file
  CalendarData calendarData;
  if (!calendarData.loadFromFile()) {
    Serial.printf("[%lu] [SLP] No calendar data found, showing default screen\n", millis());
    return renderDefaultSleepScreen();
  }

  // Load event state (completed/hidden status)
  CalendarEventState eventState;
  eventState.loadFromFile();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  // Title and date at the top
  int y = 10;
  renderer.drawText(UI_12_FONT_ID, 10, y, "Today's Agenda", true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 5;

  // Date
  if (!calendarData.today.date.empty()) {
    renderer.drawText(UI_10_FONT_ID, 10, y, calendarData.today.date.c_str(), true, EpdFontFamily::REGULAR);
    y += renderer.getLineHeight(UI_10_FONT_ID) + 2;
  }

  // Weather info
  if (!calendarData.today.weather.empty() || calendarData.today.temperature != 0) {
    char weatherStr[64];
    if (calendarData.today.temperature != 0) {
      snprintf(weatherStr, sizeof(weatherStr), "%s %d°C", calendarData.today.weather.c_str(), calendarData.today.temperature);
    } else {
      snprintf(weatherStr, sizeof(weatherStr), "%s", calendarData.today.weather.c_str());
    }
    renderer.drawText(SMALL_FONT_ID, 10, y, weatherStr, true, EpdFontFamily::REGULAR);
    y += renderer.getLineHeight(SMALL_FONT_ID) + 8;
  }

  // Draw a separator line
  renderer.drawLine(10, y, pageWidth - 10, y, true);
  y += 8;

  // Display today's events (limit to 5 visible events to fit on screen)
  const size_t maxVisibleEvents = 5;
  size_t visibleEventCount = 0;
  bool hasEvents = false;

  renderer.drawText(UI_10_FONT_ID, 10, y, "Events:", true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_10_FONT_ID) + 5;

  for (size_t i = 0; i < calendarData.today.events.size() && visibleEventCount < maxVisibleEvents; i++) {
    const auto& event = calendarData.today.events[i];
    std::string eventKey = CalendarEventState::makeEventKey(calendarData.today.date, event.time, event.title);

    // Skip hidden events
    if (eventState.isHidden(eventKey)) {
      continue;
    }

    hasEvents = true;
    bool isCompleted = eventState.isCompleted(eventKey);

    // Completion indicator
    const char* checkbox = isCompleted ? "[X]" : "   ";
    renderer.drawText(SMALL_FONT_ID, 15, y, checkbox, true, EpdFontFamily::REGULAR);

    // Time
    renderer.drawText(UI_10_FONT_ID, 45, y, event.time.c_str(), true, EpdFontFamily::REGULAR);

    // Title (truncate if too long)
    std::string title = event.title;
    const int maxTitleWidth = pageWidth - 130;
    const int titleWidth = renderer.getTextWidth(UI_10_FONT_ID, title.c_str(), EpdFontFamily::REGULAR);
    if (titleWidth > maxTitleWidth) {
      // Truncate title to fit
      while (title.length() > 3 && renderer.getTextWidth(UI_10_FONT_ID, (title + "...").c_str(), EpdFontFamily::REGULAR) > maxTitleWidth) {
        title.pop_back();
      }
      title += "...";
    }

    int titleX = 115;
    renderer.drawText(UI_10_FONT_ID, titleX, y, title.c_str(), true, EpdFontFamily::REGULAR);

    // Strikethrough if completed
    if (isCompleted) {
      int titleActualWidth = renderer.getTextWidth(UI_10_FONT_ID, title.c_str(), EpdFontFamily::REGULAR);
      int strikeY = y + renderer.getLineHeight(UI_10_FONT_ID) / 2;
      renderer.drawLine(titleX, strikeY, titleX + titleActualWidth, strikeY, true);
    }

    y += renderer.getLineHeight(UI_10_FONT_ID) + 3;
    visibleEventCount++;
  }

  if (!hasEvents) {
    renderer.drawText(UI_10_FONT_ID, 15, y, "No events scheduled", true, EpdFontFamily::REGULAR);
    y += renderer.getLineHeight(UI_10_FONT_ID) + 10;
  } else {
    y += 5;
  }

  // Display tasks (limit to 3 tasks)
  const size_t maxTasks = 3;
  const size_t taskCount = std::min(calendarData.today.tasks.size(), maxTasks);

  if (taskCount > 0) {
    y += 5;
    renderer.drawText(UI_10_FONT_ID, 10, y, "Tasks:", true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_10_FONT_ID) + 5;

    for (size_t i = 0; i < taskCount; i++) {
      const auto& task = calendarData.today.tasks[i];

      // Bullet point
      renderer.drawText(UI_10_FONT_ID, 15, y, "-", true, EpdFontFamily::REGULAR);

      // Task text (truncate if too long)
      std::string taskText = task;
      const int maxTaskWidth = pageWidth - 50;
      const int taskWidth = renderer.getTextWidth(UI_10_FONT_ID, taskText.c_str(), EpdFontFamily::REGULAR);
      if (taskWidth > maxTaskWidth) {
        while (taskText.length() > 3 && renderer.getTextWidth(UI_10_FONT_ID, (taskText + "...").c_str(), EpdFontFamily::REGULAR) > maxTaskWidth) {
          taskText.pop_back();
        }
        taskText += "...";
      }

      renderer.drawText(UI_10_FONT_ID, 30, y, taskText.c_str(), true, EpdFontFamily::REGULAR);
      y += renderer.getLineHeight(UI_10_FONT_ID) + 3;
    }
  }

  // Tomorrow's preview at the bottom
  if (!calendarData.tomorrow.events.empty()) {
    y = pageHeight - 60;
    renderer.drawLine(10, y, pageWidth - 10, y, true);
    y += 8;

    renderer.drawText(SMALL_FONT_ID, 10, y, "Tomorrow:", true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(SMALL_FONT_ID) + 3;

    // Show first event only
    const auto& firstEvent = calendarData.tomorrow.events[0];
    char tomorrowStr[128];
    snprintf(tomorrowStr, sizeof(tomorrowStr), "%s - %s", firstEvent.time.c_str(), firstEvent.title.c_str());
    renderer.drawText(SMALL_FONT_ID, 10, y, tomorrowStr, true, EpdFontFamily::REGULAR);
  }

  // Source and sync time at the very bottom
  if (!calendarData.source.empty()) {
    char sourceStr[64];
    snprintf(sourceStr, sizeof(sourceStr), "via %s", calendarData.source.c_str());
    renderer.drawText(SMALL_FONT_ID, 10, pageHeight - 15, sourceStr, true, EpdFontFamily::REGULAR);
  }

  renderer.displayBuffer(EInkDisplay::HALF_REFRESH);
}
