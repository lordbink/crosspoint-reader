#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <map>
#include <string>
#include <vector>

/**
 * @brief Stores local state for calendar events (completed, hidden, notes)
 *
 * This allows the reader to track event modifications without syncing back to phone.
 * State is persisted to /calendar/event_state.json
 */
class CalendarEventState {
 public:
  struct EventState {
    bool completed = false;     // User marked event as done
    bool hidden = false;         // User dismissed/hid event
    std::string note;            // User's quick note
    unsigned long timestamp = 0; // When state was last modified
  };

  CalendarEventState() = default;

  /**
   * @brief Load event state from SD card
   * @return true if loaded successfully, false if file doesn't exist or parse error
   */
  bool loadFromFile();

  /**
   * @brief Save event state to SD card
   * @return true if saved successfully
   */
  bool saveToFile();

  /**
   * @brief Get state for a specific event
   * @param eventKey Unique key for event (format: "YYYY-MM-DD HH:MM Title")
   * @return EventState structure (default if not found)
   */
  EventState getState(const std::string& eventKey) const;

  /**
   * @brief Set completed status for an event
   * @param eventKey Unique key for event
   * @param completed true to mark as done, false to unmark
   */
  void setCompleted(const std::string& eventKey, bool completed);

  /**
   * @brief Set hidden status for an event
   * @param eventKey Unique key for event
   * @param hidden true to hide, false to show
   */
  void setHidden(const std::string& eventKey, bool hidden);

  /**
   * @brief Add or update note for an event
   * @param eventKey Unique key for event
   * @param note Note text (empty to clear)
   */
  void setNote(const std::string& eventKey, const std::string& note);

  /**
   * @brief Check if event is completed
   * @param eventKey Unique key for event
   * @return true if marked as completed
   */
  bool isCompleted(const std::string& eventKey) const;

  /**
   * @brief Check if event is hidden
   * @param eventKey Unique key for event
   * @return true if marked as hidden
   */
  bool isHidden(const std::string& eventKey) const;

  /**
   * @brief Get note for an event
   * @param eventKey Unique key for event
   * @return Note text (empty if no note)
   */
  std::string getNote(const std::string& eventKey) const;

  /**
   * @brief Clear old event states (older than 7 days)
   * @param daysToKeep Number of days to keep (default: 7)
   */
  void cleanupOldStates(int daysToKeep = 7);

  /**
   * @brief Clear all event states
   */
  void clear();

  /**
   * @brief Get total number of stored event states
   * @return Count of events with state
   */
  size_t getStateCount() const { return eventStates.size(); }

  /**
   * @brief Create event key from date, time, and title
   * @param date Date string (YYYY-MM-DD)
   * @param time Time string (HH:MM)
   * @param title Event title
   * @return Unique event key
   */
  static std::string makeEventKey(const std::string& date, const std::string& time, const std::string& title);

 private:
  static constexpr const char* STATE_FILE_PATH = "/calendar/event_state.json";
  std::map<std::string, EventState> eventStates;
};
