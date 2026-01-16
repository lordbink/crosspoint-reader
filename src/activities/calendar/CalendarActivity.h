#pragma once

#include <GfxRenderer.h>

#include "../Activity.h"
#include "CalendarData.h"
#include "CalendarEventState.h"
#include "MappedInputManager.h"

/**
 * @brief Interactive calendar view with event management
 *
 * Allows users to:
 * - View today's and tomorrow's events
 * - Mark events as completed (checkmark)
 * - Hide/dismiss events
 * - View event notes
 *
 * Controls:
 * - UP/DOWN: Navigate events
 * - SELECT: Toggle event completed
 * - LEFT: Hide event
 * - BACK: Return to home
 */
class CalendarActivity : public Activity {
 public:
  CalendarActivity(GfxRenderer& renderer, MappedInputManager& inputManager, const std::function<void()>& onGoHome);
  ~CalendarActivity() override = default;

  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  void render();
  void renderEventList();
  void renderEventDetails();
  void renderNoEvents();

  void navigateUp();
  void navigateDown();
  void toggleCompleted();
  void hideEvent();

  void loadCalendarData();
  void saveEventState();

  const std::function<void()> onGoHome;

  CalendarData calendarData;
  CalendarEventState eventState;

  int selectedEventIndex = 0;
  bool showTodayEvents = true;  // true = today, false = tomorrow
  bool dataLoaded = false;
  bool stateChanged = false;
  bool updateRequired = false;

  static constexpr int MAX_EVENTS_DISPLAY = 7;
};
