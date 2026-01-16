#include "CalendarEventState.h"

#include <SDCardManager.h>

bool CalendarEventState::loadFromFile() {
  FsFile file;
  if (!SdMan.openFileForRead("CAL-STATE", STATE_FILE_PATH, file)) {
    Serial.printf("[%lu] [CAL-STATE] No event state file found at %s\n", millis(), STATE_FILE_PATH);
    return false;
  }

  // Read file content
  String jsonContent;
  while (file.available()) {
    jsonContent += (char)file.read();
  }
  file.close();

  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonContent);

  if (error) {
    Serial.printf("[%lu] [CAL-STATE] Failed to parse event state JSON: %s\n", millis(), error.c_str());
    return false;
  }

  // Clear existing states
  eventStates.clear();

  // Load event states
  JsonObject events = doc["events"].as<JsonObject>();
  for (JsonPair kv : events) {
    EventState state;
    JsonObject eventObj = kv.value().as<JsonObject>();

    state.completed = eventObj["completed"] | false;
    state.hidden = eventObj["hidden"] | false;
    state.note = eventObj["note"] | "";
    state.timestamp = eventObj["timestamp"] | 0UL;

    eventStates[kv.key().c_str()] = state;
  }

  Serial.printf("[%lu] [CAL-STATE] Loaded %u event states from file\n", millis(), eventStates.size());
  return true;
}

bool CalendarEventState::saveToFile() {
  // Ensure calendar directory exists
  SdMan.mkdir("/calendar");

  // Build JSON document
  JsonDocument doc;
  JsonObject events = doc["events"].to<JsonObject>();

  for (const auto& kv : eventStates) {
    JsonObject eventObj = events[kv.first].to<JsonObject>();
    eventObj["completed"] = kv.second.completed;
    eventObj["hidden"] = kv.second.hidden;
    eventObj["note"] = kv.second.note;
    eventObj["timestamp"] = kv.second.timestamp;
  }

  doc["lastModified"] = millis();
  doc["version"] = 1;

  // Serialize to string
  String jsonContent;
  serializeJson(doc, jsonContent);

  // Write to file
  FsFile file;
  if (!SdMan.openFileForWrite("CAL-STATE", STATE_FILE_PATH, file)) {
    Serial.printf("[%lu] [CAL-STATE] Failed to open event state file for writing: %s\n", millis(), STATE_FILE_PATH);
    return false;
  }

  size_t written = file.print(jsonContent);
  file.close();

  if (written == 0) {
    Serial.printf("[%lu] [CAL-STATE] Failed to write event state to file\n", millis());
    return false;
  }

  Serial.printf("[%lu] [CAL-STATE] Saved %u event states to file (%u bytes)\n", millis(), eventStates.size(), written);
  return true;
}

CalendarEventState::EventState CalendarEventState::getState(const std::string& eventKey) const {
  auto it = eventStates.find(eventKey);
  if (it != eventStates.end()) {
    return it->second;
  }
  return EventState();  // Return default state if not found
}

void CalendarEventState::setCompleted(const std::string& eventKey, bool completed) {
  eventStates[eventKey].completed = completed;
  eventStates[eventKey].timestamp = millis();
}

void CalendarEventState::setHidden(const std::string& eventKey, bool hidden) {
  eventStates[eventKey].hidden = hidden;
  eventStates[eventKey].timestamp = millis();
}

void CalendarEventState::setNote(const std::string& eventKey, const std::string& note) {
  eventStates[eventKey].note = note;
  eventStates[eventKey].timestamp = millis();
}

bool CalendarEventState::isCompleted(const std::string& eventKey) const {
  auto it = eventStates.find(eventKey);
  return it != eventStates.end() && it->second.completed;
}

bool CalendarEventState::isHidden(const std::string& eventKey) const {
  auto it = eventStates.find(eventKey);
  return it != eventStates.end() && it->second.hidden;
}

std::string CalendarEventState::getNote(const std::string& eventKey) const {
  auto it = eventStates.find(eventKey);
  if (it != eventStates.end()) {
    return it->second.note;
  }
  return "";
}

void CalendarEventState::cleanupOldStates(int daysToKeep) {
  unsigned long cutoffTime = millis() - (daysToKeep * 24UL * 60UL * 60UL * 1000UL);

  auto it = eventStates.begin();
  while (it != eventStates.end()) {
    if (it->second.timestamp < cutoffTime) {
      it = eventStates.erase(it);
    } else {
      ++it;
    }
  }

  Serial.printf("[%lu] [CAL-STATE] Cleaned up old event states (keeping %d days)\n", millis(), daysToKeep);
}

void CalendarEventState::clear() {
  eventStates.clear();
  Serial.printf("[%lu] [CAL-STATE] Cleared all event states\n", millis());
}

std::string CalendarEventState::makeEventKey(const std::string& date, const std::string& time,
                                              const std::string& title) {
  // Format: "YYYY-MM-DD HH:MM Title"
  // This creates a reasonably unique key for each event
  return date + " " + time + " " + title;
}
