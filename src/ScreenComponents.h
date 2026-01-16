#pragma once

#include <cstddef>
#include <cstdint>

class GfxRenderer;
class CalendarData;

class ScreenComponents {
 public:
  static void drawBattery(const GfxRenderer& renderer, int left, int top);

  /**
   * Draw a progress bar with percentage text.
   * @param renderer The graphics renderer
   * @param x Left position of the bar
   * @param y Top position of the bar
   * @param width Width of the bar
   * @param height Height of the bar
   * @param current Current progress value
   * @param total Total value for 100% progress
   */
  static void drawProgressBar(const GfxRenderer& renderer, int x, int y, int width, int height, size_t current,
                              size_t total);

  /**
   * Draw calendar/agenda in sleep screen
   * @param renderer The graphics renderer
   * @param calendarData The calendar data to display
   */
  static void drawCalendarSleepScreen(const GfxRenderer& renderer, const CalendarData& calendarData);
};
