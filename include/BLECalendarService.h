#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <memory>

#include "CalendarData.h"

/**
 * BLE Calendar Service
 * Handles receiving calendar data via BLE GATT server
 * Allows Android apps to push calendar/agenda/tasks to the device
 */

class BLECalendarService {
 public:
  using CalendarReceivedCallback = std::function<void(const CalendarData&)>;

  BLECalendarService();
  ~BLECalendarService();

  // Initialize BLE server and calendar service
  bool begin(const char* deviceName = "Crosspoint Reader");

  // Stop BLE server
  void stop();

  // Check if BLE is currently enabled
  bool isRunning() const { return isRunning_; }

  // Check if a client is currently connected
  bool isClientConnected() const { return isClientConnected_; }

  // Register callback for when calendar data is received
  void setCalendarReceivedCallback(CalendarReceivedCallback callback) {
    calendarReceivedCallback_ = callback;
  }

  // Invoke the calendar received callback (called from BLE characteristic callback)
  void invokeCalendarReceivedCallback(const CalendarData& data) {
    if (calendarReceivedCallback_) {
      calendarReceivedCallback_(data);
    }
  }

  // Send current calendar data via BLE (for testing/sync)
  bool sendCalendarData(const CalendarData& data);

  // Handle periodic tasks (call from main loop)
  void handleTasks();

  // Get RSSI of connected client (for proximity detection)
  int32_t getClientRSSI() const;

 private:
  bool isRunning_ = false;
  bool isClientConnected_ = false;
  
  // UUID for calendar service
  static constexpr const char* CALENDAR_SERVICE_UUID = "12345678-1234-5678-1234-56789abcdef0";
  static constexpr const char* CALENDAR_WRITE_UUID = "12345678-1234-5678-1234-56789abcdef1";
  static constexpr const char* CALENDAR_NOTIFY_UUID = "12345678-1234-5678-1234-56789abcdef2";

  // Callback when calendar data is received
  CalendarReceivedCallback calendarReceivedCallback_;

  // Internal helper to handle received data
  void onCalendarDataReceived(const std::string& data);
};
