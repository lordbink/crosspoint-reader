#pragma once

#include <string>
#include <vector>
#include <ctime>

/**
 * CalendarData.h
 * Data structures for calendar events, daily agenda, and tasks
 * Designed for BLE transfer and sleep mode display
 */

struct CalendarEvent {
  std::string title;
  std::string time;           // HH:MM format
  std::string endTime;        // HH:MM format (optional)
  std::string description;
  std::string location;

  CalendarEvent() = default;
  CalendarEvent(const std::string& t, const std::string& tm)
      : title(t), time(tm) {}
};

struct DailyAgenda {
  std::string date;           // YYYY-MM-DD format
  std::vector<CalendarEvent> events;
  std::vector<std::string> tasks;
  
  // Metadata
  std::string weather;        // Today's weather summary
  int32_t temperature = 0;    // Temperature in Celsius

  DailyAgenda() = default;
  explicit DailyAgenda(const std::string& d) : date(d) {}

  // Get number of events happening today
  size_t getEventCount() const { return events.size(); }
  
  // Get number of tasks for today
  size_t getTaskCount() const { return tasks.size(); }

  // Check if this agenda is for today
  bool isToday() const;
};

struct CalendarData {
  DailyAgenda today;
  DailyAgenda tomorrow;
  std::vector<DailyAgenda> weekView;  // Up to 7 days
  
  // Last sync time (Unix timestamp)
  uint64_t lastSyncTime = 0;
  
  // Source info
  std::string source;  // e.g., "Google Calendar", "Android Calendar"
  
  CalendarData() = default;

  // Save to JSON file on SD card
  bool saveToFile(const char* path = "/calendar/agenda.json") const;
  
  // Load from JSON file on SD card
  bool loadFromFile(const char* path = "/calendar/agenda.json");
  
  // Clear all data
  void clear() {
    today = DailyAgenda();
    tomorrow = DailyAgenda();
    weekView.clear();
    lastSyncTime = 0;
  }
};

// Helper to get current date in YYYY-MM-DD format
std::string getCurrentDateString();

// Helper to parse JSON calendar data from string
CalendarData* parseCalendarJSON(const std::string& jsonStr);
