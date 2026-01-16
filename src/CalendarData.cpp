#include "CalendarData.h"

#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <SDCardManager.h>
#include <time.h>

#include <sstream>
#include <iomanip>

std::string getCurrentDateString() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  
  std::ostringstream oss;
  oss << std::put_time(timeinfo, "%Y-%m-%d");
  return oss.str();
}

bool DailyAgenda::isToday() const {
  return date == getCurrentDateString();
}

bool CalendarData::saveToFile(const char* path) const {
  // Create directory if it doesn't exist
  SdMan.mkdir("/calendar");

  FsFile file;
  if (!SdMan.openFileForWrite("CAL", path, file)) {
    Serial.printf("[%lu] [CAL] Failed to open file for writing: %s\n", millis(), path);
    return false;
  }

  JsonDocument doc;
  
  // Add today's agenda
  JsonObject todayObj = doc["today"].to<JsonObject>();
  todayObj["date"] = today.date;
  todayObj["temperature"] = today.temperature;
  if (!today.weather.empty()) {
    todayObj["weather"] = today.weather;
  }
  
  JsonArray todayEvents = todayObj["events"].to<JsonArray>();
  for (const auto& event : today.events) {
    JsonObject eventObj = todayEvents.add<JsonObject>();
    eventObj["title"] = event.title;
    eventObj["time"] = event.time;
    if (!event.endTime.empty()) eventObj["endTime"] = event.endTime;
    if (!event.description.empty()) eventObj["description"] = event.description;
    if (!event.location.empty()) eventObj["location"] = event.location;
  }
  
  JsonArray todayTasks = todayObj["tasks"].to<JsonArray>();
  for (const auto& task : today.tasks) {
    todayTasks.add(task);
  }

  // Add tomorrow's agenda
  JsonObject tomorrowObj = doc["tomorrow"].to<JsonObject>();
  tomorrowObj["date"] = tomorrow.date;
  if (!tomorrow.weather.empty()) {
    tomorrowObj["weather"] = tomorrow.weather;
  }
  
  JsonArray tomorrowEvents = tomorrowObj["events"].to<JsonArray>();
  for (const auto& event : tomorrow.events) {
    JsonObject eventObj = tomorrowEvents.add<JsonObject>();
    eventObj["title"] = event.title;
    eventObj["time"] = event.time;
    if (!event.endTime.empty()) eventObj["endTime"] = event.endTime;
    if (!event.description.empty()) eventObj["description"] = event.description;
  }

  // Add metadata
  doc["lastSyncTime"] = lastSyncTime;
  if (!source.empty()) {
    doc["source"] = source;
  }

  // Serialize and write
  size_t written = serializeJson(doc, file);
  file.close();

  if (written > 0) {
    Serial.printf("[%lu] [CAL] Calendar data saved to %s (%u bytes)\n", millis(), path, written);
    return true;
  } else {
    Serial.printf("[%lu] [CAL] Failed to serialize calendar data\n", millis());
    return false;
  }
}

bool CalendarData::loadFromFile(const char* path) {
  FsFile file;
  if (!SdMan.openFileForRead("CAL", path, file)) {
    Serial.printf("[%lu] [CAL] File not found: %s\n", millis(), path);
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.printf("[%lu] [CAL] Failed to parse JSON: %s\n", millis(), error.c_str());
    return false;
  }

  // Parse today's agenda
  if (doc.containsKey("today")) {
    JsonObject todayObj = doc["today"];
    today.date = todayObj["date"] | "";
    today.temperature = todayObj["temperature"] | 0;
    today.weather = todayObj["weather"] | "";

    if (todayObj.containsKey("events")) {
      for (JsonObject eventObj : todayObj["events"].as<JsonArray>()) {
        CalendarEvent event;
        event.title = eventObj["title"] | "";
        event.time = eventObj["time"] | "";
        event.endTime = eventObj["endTime"] | "";
        event.description = eventObj["description"] | "";
        event.location = eventObj["location"] | "";
        today.events.push_back(event);
      }
    }

    if (todayObj.containsKey("tasks")) {
      for (JsonVariant taskVar : todayObj["tasks"].as<JsonArray>()) {
        today.tasks.push_back(taskVar.as<std::string>());
      }
    }
  }

  // Parse tomorrow's agenda
  if (doc.containsKey("tomorrow")) {
    JsonObject tomorrowObj = doc["tomorrow"];
    tomorrow.date = tomorrowObj["date"] | "";
    tomorrow.weather = tomorrowObj["weather"] | "";

    if (tomorrowObj.containsKey("events")) {
      for (JsonObject eventObj : tomorrowObj["events"].as<JsonArray>()) {
        CalendarEvent event;
        event.title = eventObj["title"] | "";
        event.time = eventObj["time"] | "";
        event.endTime = eventObj["endTime"] | "";
        event.description = eventObj["description"] | "";
        tomorrow.events.push_back(event);
      }
    }
  }

  // Parse metadata
  lastSyncTime = doc["lastSyncTime"] | 0;
  source = doc["source"] | "";

  Serial.printf("[%lu] [CAL] Calendar data loaded: %u today events, %u tomorrow events\n", 
                millis(), today.getEventCount(), tomorrow.getEventCount());
  return true;
}

CalendarData* parseCalendarJSON(const std::string& jsonStr) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonStr);

  if (error) {
    Serial.printf("[%lu] [CAL] Failed to parse JSON: %s\n", millis(), error.c_str());
    return nullptr;
  }

  auto* calData = new CalendarData();

  // Parse today's agenda
  if (doc.containsKey("today")) {
    JsonObject todayObj = doc["today"];
    calData->today.date = todayObj["date"] | getCurrentDateString();
    calData->today.temperature = todayObj["temperature"] | 0;
    calData->today.weather = todayObj["weather"] | "";

    if (todayObj.containsKey("events")) {
      for (JsonObject eventObj : todayObj["events"].as<JsonArray>()) {
        CalendarEvent event;
        event.title = eventObj["title"] | "";
        event.time = eventObj["time"] | "";
        event.endTime = eventObj["endTime"] | "";
        event.description = eventObj["description"] | "";
        event.location = eventObj["location"] | "";
        calData->today.events.push_back(event);
      }
    }

    if (todayObj.containsKey("tasks")) {
      for (JsonVariant taskVar : todayObj["tasks"].as<JsonArray>()) {
        calData->today.tasks.push_back(taskVar.as<std::string>());
      }
    }
  }

  // Parse tomorrow's agenda
  if (doc.containsKey("tomorrow")) {
    JsonObject tomorrowObj = doc["tomorrow"];
    calData->tomorrow.date = tomorrowObj["date"] | "";
    calData->tomorrow.weather = tomorrowObj["weather"] | "";

    if (tomorrowObj.containsKey("events")) {
      for (JsonObject eventObj : tomorrowObj["events"].as<JsonArray>()) {
        CalendarEvent event;
        event.title = eventObj["title"] | "";
        event.time = eventObj["time"] | "";
        event.endTime = eventObj["endTime"] | "";
        event.description = eventObj["description"] | "";
        calData->tomorrow.events.push_back(event);
      }
    }

    if (tomorrowObj.containsKey("tasks")) {
      for (JsonVariant taskVar : tomorrowObj["tasks"].as<JsonArray>()) {
        calData->tomorrow.tasks.push_back(taskVar.as<std::string>());
      }
    }
  }

  calData->lastSyncTime = doc["lastSyncTime"] | time(nullptr);
  calData->source = doc["source"] | "";

  return calData;
}
