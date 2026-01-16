#include "ScreenComponents.h"

#include <GfxRenderer.h>

#include <cstdint>
#include <string>

#include "Battery.h"
#include "CalendarData.h"
#include "fontIds.h"

void ScreenComponents::drawBattery(const GfxRenderer& renderer, const int left, const int top) {
  // Left aligned battery icon and percentage
  const uint16_t percentage = battery.readPercentage();
  const auto percentageText = std::to_string(percentage) + "%";
  renderer.drawText(SMALL_FONT_ID, left + 20, top, percentageText.c_str());

  // 1 column on left, 2 columns on right, 5 columns of battery body
  constexpr int batteryWidth = 15;
  constexpr int batteryHeight = 12;
  const int x = left;
  const int y = top + 6;

  // Top line
  renderer.drawLine(x + 1, y, x + batteryWidth - 3, y);
  // Bottom line
  renderer.drawLine(x + 1, y + batteryHeight - 1, x + batteryWidth - 3, y + batteryHeight - 1);
  // Left line
  renderer.drawLine(x, y + 1, x, y + batteryHeight - 2);
  // Battery end
  renderer.drawLine(x + batteryWidth - 2, y + 1, x + batteryWidth - 2, y + batteryHeight - 2);
  renderer.drawPixel(x + batteryWidth - 1, y + 3);
  renderer.drawPixel(x + batteryWidth - 1, y + batteryHeight - 4);
  renderer.drawLine(x + batteryWidth - 0, y + 4, x + batteryWidth - 0, y + batteryHeight - 5);

  // The +1 is to round up, so that we always fill at least one pixel
  int filledWidth = percentage * (batteryWidth - 5) / 100 + 1;
  if (filledWidth > batteryWidth - 5) {
    filledWidth = batteryWidth - 5;  // Ensure we don't overflow
  }

  renderer.fillRect(x + 2, y + 2, filledWidth, batteryHeight - 4);
}

void ScreenComponents::drawProgressBar(const GfxRenderer& renderer, const int x, const int y, const int width,
                                       const int height, const size_t current, const size_t total) {
  if (total == 0) {
    return;
  }

  // Use 64-bit arithmetic to avoid overflow for large files
  const int percent = static_cast<int>((static_cast<uint64_t>(current) * 100) / total);

  // Draw outline
  renderer.drawRect(x, y, width, height);

  // Draw filled portion
  const int fillWidth = (width - 4) * percent / 100;
  if (fillWidth > 0) {
    renderer.fillRect(x + 2, y + 2, fillWidth, height - 4);
  }

  // Draw percentage text centered below bar
  const std::string percentText = std::to_string(percent) + "%";
  renderer.drawCenteredText(UI_10_FONT_ID, y + height + 15, percentText.c_str());
}
void ScreenComponents::drawCalendarSleepScreen(const GfxRenderer& renderer, const CalendarData& calendarData) {
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  
  // Clear screen
  renderer.clearScreen();
  
  // Title with date and weather
  int y = 20;
  const std::string dateTitle = calendarData.today.date;
  renderer.drawCenteredText(UI_12_FONT_ID, y, dateTitle.c_str(), true, EpdFontFamily::BOLD);
  
  if (!calendarData.today.weather.empty()) {
    y += 35;
    std::string weatherStr = calendarData.today.weather;
    if (calendarData.today.temperature != 0) {
      weatherStr += " " + std::to_string(calendarData.today.temperature) + "°C";
    }
    renderer.drawCenteredText(SMALL_FONT_ID, y, weatherStr.c_str());
  }
  
  y += 40;
  
  // Today's events section
  if (calendarData.today.getEventCount() > 0) {
    renderer.drawText(UI_10_FONT_ID, 20, y, "Today's Events:", true, EpdFontFamily::BOLD);
    y += 30;
    
    // Show up to 4 events
    size_t eventCount = calendarData.today.getEventCount();
    if (eventCount > 4) eventCount = 4;
    
    for (size_t i = 0; i < eventCount; i++) {
      const auto& event = calendarData.today.events[i];
      std::string eventStr = event.time + " - " + event.title;
      
      // Truncate if too long
      if (eventStr.length() > 40) {
        eventStr = eventStr.substr(0, 37) + "...";
      }
      
      renderer.drawText(SMALL_FONT_ID, 30, y, eventStr.c_str());
      y += 25;
    }
  }
  
  // Tasks section
  if (calendarData.today.getTaskCount() > 0) {
    if (y < pageHeight - 150) {  // Only show if space available
      y += 10;
      renderer.drawText(UI_10_FONT_ID, 20, y, "Tasks:", true, EpdFontFamily::BOLD);
      y += 30;
      
      size_t taskCount = calendarData.today.getTaskCount();
      if (taskCount > 3) taskCount = 3;
      
      for (size_t i = 0; i < taskCount; i++) {
        std::string taskStr = "• " + calendarData.today.tasks[i];
        
        if (taskStr.length() > 45) {
          taskStr = taskStr.substr(0, 42) + "...";
        }
        
        renderer.drawText(SMALL_FONT_ID, 30, y, taskStr.c_str());
        y += 20;
      }
    }
  }
  
  // Tomorrow preview at bottom
  if (!calendarData.tomorrow.date.empty() && calendarData.tomorrow.getEventCount() > 0) {
    y = pageHeight - 80;
    renderer.drawText(SMALL_FONT_ID, 20, y, "Tomorrow:", true, EpdFontFamily::BOLD);
    y += 25;
    
    // Show first tomorrow event
    const auto& event = calendarData.tomorrow.events[0];
    std::string tomorrowStr = event.time + " - " + event.title;
    if (tomorrowStr.length() > 40) {
      tomorrowStr = tomorrowStr.substr(0, 37) + "...";
    }
    renderer.drawText(SMALL_FONT_ID, 30, y, tomorrowStr.c_str());
  }
}