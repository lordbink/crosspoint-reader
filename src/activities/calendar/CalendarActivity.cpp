#include "CalendarActivity.h"

#include "CrossPointSettings.h"
#include "fontIds.h"

CalendarActivity::CalendarActivity(GfxRenderer& renderer, MappedInputManager& inputManager, const std::function<void()>& onGoHome)
    : Activity("Calendar", renderer, inputManager), onGoHome(onGoHome) {}

void CalendarActivity::onEnter() {
  Activity::onEnter();
  loadCalendarData();
  render();
}

void CalendarActivity::onExit() {
  if (stateChanged) {
    saveEventState();
  }
  Activity::onExit();
}

void CalendarActivity::loadCalendarData() {
  dataLoaded = calendarData.loadFromFile();
  eventState.loadFromFile();

  if (!dataLoaded) {
    Serial.printf("[%lu] [CAL-ACT] No calendar data found\n", millis());
  } else {
    Serial.printf("[%lu] [CAL-ACT] Loaded calendar data: %u today events, %u tomorrow events\n", millis(),
                  calendarData.today.getEventCount(), calendarData.tomorrow.getEventCount());
  }
}

void CalendarActivity::saveEventState() {
  if (eventState.saveToFile()) {
    Serial.printf("[%lu] [CAL-ACT] Event state saved successfully\n", millis());
    stateChanged = false;
  } else {
    Serial.printf("[%lu] [CAL-ACT] Failed to save event state\n", millis());
  }
}

void CalendarActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    navigateUp();
    updateRequired = true;
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    navigateDown();
    updateRequired = true;
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    toggleCompleted();
    updateRequired = true;
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    hideEvent();
    updateRequired = true;
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    // Toggle between today and tomorrow
    showTodayEvents = !showTodayEvents;
    selectedEventIndex = 0;
    updateRequired = true;
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }

  if (updateRequired) {
    render();
    updateRequired = false;
  }
}

void CalendarActivity::navigateUp() {
  if (!dataLoaded) return;

  const auto& events = showTodayEvents ? calendarData.today.events : calendarData.tomorrow.events;

  if (selectedEventIndex > 0) {
    selectedEventIndex--;
  }
}

void CalendarActivity::navigateDown() {
  if (!dataLoaded) return;

  const auto& events = showTodayEvents ? calendarData.today.events : calendarData.tomorrow.events;

  if (selectedEventIndex < static_cast<int>(events.size()) - 1) {
    selectedEventIndex++;
  }
}

void CalendarActivity::toggleCompleted() {
  if (!dataLoaded) return;

  const auto& events = showTodayEvents ? calendarData.today.events : calendarData.tomorrow.events;

  if (selectedEventIndex < 0 || selectedEventIndex >= static_cast<int>(events.size())) {
    return;
  }

  const auto& event = events[selectedEventIndex];
  const std::string date = showTodayEvents ? calendarData.today.date : calendarData.tomorrow.date;
  std::string eventKey = CalendarEventState::makeEventKey(date, event.time, event.title);

  bool currentlyCompleted = eventState.isCompleted(eventKey);
  eventState.setCompleted(eventKey, !currentlyCompleted);
  stateChanged = true;

  Serial.printf("[%lu] [CAL-ACT] Toggled completion for event: %s (now: %s)\n", millis(), event.title.c_str(),
                !currentlyCompleted ? "completed" : "not completed");
}

void CalendarActivity::hideEvent() {
  if (!dataLoaded) return;

  const auto& events = showTodayEvents ? calendarData.today.events : calendarData.tomorrow.events;

  if (selectedEventIndex < 0 || selectedEventIndex >= static_cast<int>(events.size())) {
    return;
  }

  const auto& event = events[selectedEventIndex];
  const std::string date = showTodayEvents ? calendarData.today.date : calendarData.tomorrow.date;
  std::string eventKey = CalendarEventState::makeEventKey(date, event.time, event.title);

  eventState.setHidden(eventKey, true);
  stateChanged = true;

  Serial.printf("[%lu] [CAL-ACT] Hidden event: %s\n", millis(), event.title.c_str());

  // Move selection to next visible event
  navigateDown();
  if (selectedEventIndex >= static_cast<int>(events.size())) {
    selectedEventIndex = static_cast<int>(events.size()) - 1;
  }
}

void CalendarActivity::render() {
  renderer.clearScreen();

  if (!dataLoaded) {
    renderNoEvents();
  } else {
    renderEventList();
  }

  renderer.displayBuffer(EInkDisplay::FAST_REFRESH);
}

void CalendarActivity::renderNoEvents() {
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  int y = pageHeight / 2 - 40;

  const char* msg1 = "No Calendar Data";
  const char* msg2 = "Sync calendar from your phone";
  const char* msg3 = "via BLE or FTP";

  int w1 = renderer.getTextWidth(UI_12_FONT_ID, msg1, EpdFontFamily::BOLD);
  int w2 = renderer.getTextWidth(SMALL_FONT_ID, msg2, EpdFontFamily::REGULAR);
  int w3 = renderer.getTextWidth(SMALL_FONT_ID, msg3, EpdFontFamily::REGULAR);

  renderer.drawText(UI_12_FONT_ID, (pageWidth - w1) / 2, y, msg1, true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 10;

  renderer.drawText(SMALL_FONT_ID, (pageWidth - w2) / 2, y, msg2, true, EpdFontFamily::REGULAR);
  y += renderer.getLineHeight(SMALL_FONT_ID) + 3;

  renderer.drawText(SMALL_FONT_ID, (pageWidth - w3) / 2, y, msg3, true, EpdFontFamily::REGULAR);
  y += renderer.getLineHeight(SMALL_FONT_ID) + 20;

  const char* backMsg = "Press BACK to return";
  int wBack = renderer.getTextWidth(SMALL_FONT_ID, backMsg, EpdFontFamily::REGULAR);
  renderer.drawText(SMALL_FONT_ID, (pageWidth - wBack) / 2, y, backMsg, true, EpdFontFamily::REGULAR);
}

void CalendarActivity::renderEventList() {
  const int pageWidth = renderer.getScreenWidth();
  int y = 10;

  // Title
  const char* title = showTodayEvents ? "Today's Events" : "Tomorrow's Events";
  renderer.drawText(UI_12_FONT_ID, 10, y, title, true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 5;

  // Date and weather
  if (showTodayEvents) {
    char dateWeather[128];
    snprintf(dateWeather, sizeof(dateWeather), "%s  %s %d°C", calendarData.today.date.c_str(),
             calendarData.today.weather.c_str(), calendarData.today.temperature);
    renderer.drawText(SMALL_FONT_ID, 10, y, dateWeather, true, EpdFontFamily::REGULAR);
  } else {
    renderer.drawText(SMALL_FONT_ID, 10, y, calendarData.tomorrow.date.c_str(), true, EpdFontFamily::REGULAR);
  }
  y += renderer.getLineHeight(SMALL_FONT_ID) + 8;

  // Separator line
  renderer.drawLine(10, y, pageWidth - 10, y, true);
  y += 10;

  // Events
  const auto& events = showTodayEvents ? calendarData.today.events : calendarData.tomorrow.events;
  const std::string& date = showTodayEvents ? calendarData.today.date : calendarData.tomorrow.date;

  if (events.empty()) {
    renderer.drawText(UI_10_FONT_ID, 10, y, "No events scheduled", true, EpdFontFamily::REGULAR);
    y += renderer.getLineHeight(UI_10_FONT_ID) + 10;
  } else {
    int visibleEventCount = 0;

    for (size_t i = 0; i < events.size() && visibleEventCount < MAX_EVENTS_DISPLAY; i++) {
      const auto& event = events[i];
      std::string eventKey = CalendarEventState::makeEventKey(date, event.time, event.title);

      // Skip hidden events
      if (eventState.isHidden(eventKey)) {
        continue;
      }

      bool isSelected = (static_cast<int>(i) == selectedEventIndex);
      bool isCompleted = eventState.isCompleted(eventKey);

      // Highlight selected event
      if (isSelected) {
        renderer.fillRect(5, y - 2, pageWidth - 10, renderer.getLineHeight(UI_10_FONT_ID) + 4, true);
      }

      // Completion checkbox
      const char* checkbox = isCompleted ? "[X]" : "[ ]";
      renderer.drawText(UI_10_FONT_ID, 10, y, checkbox, !isSelected, EpdFontFamily::REGULAR);

      // Time
      renderer.drawText(UI_10_FONT_ID, 50, y, event.time.c_str(), !isSelected, EpdFontFamily::REGULAR);

      // Title (with strikethrough if completed)
      std::string displayTitle = event.title;
      const int maxTitleWidth = pageWidth - 130;
      const int titleWidth = renderer.getTextWidth(UI_10_FONT_ID, displayTitle.c_str(), EpdFontFamily::REGULAR);

      if (titleWidth > maxTitleWidth) {
        // Truncate if too long
        while (displayTitle.length() > 3 &&
               renderer.getTextWidth(UI_10_FONT_ID, (displayTitle + "...").c_str(), EpdFontFamily::REGULAR) >
                   maxTitleWidth) {
          displayTitle.pop_back();
        }
        displayTitle += "...";
      }

      int titleX = 120;
      renderer.drawText(UI_10_FONT_ID, titleX, y, displayTitle.c_str(), !isSelected, EpdFontFamily::REGULAR);

      // Strikethrough if completed
      if (isCompleted) {
        int titleActualWidth = renderer.getTextWidth(UI_10_FONT_ID, displayTitle.c_str(), EpdFontFamily::REGULAR);
        int strikeY = y + renderer.getLineHeight(UI_10_FONT_ID) / 2;
        renderer.drawLine(titleX, strikeY, titleX + titleActualWidth, strikeY, !isSelected);
      }

      y += renderer.getLineHeight(UI_10_FONT_ID) + 5;
      visibleEventCount++;
    }
  }

  // Instructions at bottom
  y = renderer.getScreenHeight() - 50;
  renderer.drawLine(10, y, pageWidth - 10, y, true);
  y += 5;

  renderer.drawText(SMALL_FONT_ID, 10, y, "UP/DOWN: Navigate  SELECT: Complete  LEFT: Hide  RIGHT: Tomorrow",
                    true, EpdFontFamily::REGULAR);
  y += renderer.getLineHeight(SMALL_FONT_ID) + 3;
  renderer.drawText(SMALL_FONT_ID, 10, y, "BACK: Return to home", true, EpdFontFamily::REGULAR);
}

void CalendarActivity::renderEventDetails() {
  // Reserved for future use - detailed event view
}
