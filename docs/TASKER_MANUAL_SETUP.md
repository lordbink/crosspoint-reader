# Tasker Manual Setup Guide

This guide provides step-by-step instructions to manually create the CrossPoint Calendar Export tasks in Tasker.

## Prerequisites

1. Install Tasker from Google Play Store
2. Grant Tasker the following permissions in Android Settings → Apps → Tasker → Permissions:
   - Location
   - Calendar
   - Storage/Files

## Task 1: CrossPoint Get Calendar Events

This task queries your Android calendar and stores events in Tasker variables.

1. In Tasker, tap the **Tasks** tab
2. Tap **+** to create a new task
3. Name it **"CrossPoint Get Calendar Events"**
4. Add the following actions:

### Action 1: Variable Clear
- Action: Variables → Variable Clear
- Name: `%CALTITLE*`
- Pattern Matching: ✓ (checked)

### Action 2: Variable Clear
- Action: Variables → Variable Clear
- Name: `%CALSTART*`
- Pattern Matching: ✓ (checked)

### Action 3: Variable Set
- Action: Variables → Variable Set
- Name: `%CP_TodayStart`
- To: `%TIMES` (this is the current time in seconds)

### Action 4: Variable Set
- Action: Variables → Variable Set
- Name: `%CP_SearchEnd`
- To: `%TIMES + 172800` (adds 2 days in seconds)

### Action 5: Get Calendar Events
- Action: Get Calendar Events (should be under System or Data category)
- Calendar: `*` (all calendars, or select specific calendar)
- Start Time: `%CP_TodayStart`
- End Time: `%CP_SearchEnd`
- Title Variable: `CALTITLE` (this will create CALTITLE1, CALTITLE2, etc.)
- Start Time Variable: `CALSTART` (this will create CALSTART1, CALSTART2, etc.)
- Available Only: ✓ (checked - only shows future events)
- Sort Order: Ascending

**Note:** If you can't find this action, it might be under:
- System → Get Calendar Events
- Data → Get Calendar Events
- Or search for "calendar" in the action search

### Action 6: Flash
- Action: Alert → Flash
- Text: `Found %CALTITLE(#) calendar events`

---

## Task 2: CrossPoint Export Calendar

This task gets weather data and builds the calendar JSON file.

1. In Tasker, tap the **Tasks** tab
2. Tap **+** to create a new task
3. Name it **"CrossPoint Export Calendar"**
4. Add the following actions:

### Action 1: Get Location
- Action: Location → Get Location v2
- Source: Any
- Timeout: 30 seconds
- Continue Task After Error: ✓ (checked)

### Action 2: Variable Set
- Action: Variables → Variable Set
- Name: `%CP_Lat`
- To: `%gl_latitude` (or `%LOC` if using older Tasker)

### Action 3: Variable Set
- Action: Variables → Variable Set
- Name: `%CP_Lon`
- To: `%gl_longitude` (or `%LOC` if using older Tasker)

### Action 4: HTTP Request
- Action: Net → HTTP Request
- Method: GET
- URL: `https://api.open-meteo.com/v1/forecast?latitude=%CP_Lat&longitude=%CP_Lon&current=temperature_2m,weather_code&timezone=auto`
- Output Variable: `%CP_WeatherJSON`
- Timeout: 30 seconds
- Continue Task After Error: ✓ (checked)

### Action 5: Variable Set
- Action: Variables → Variable Set
- Name: `%CP_TodayDate`
- To: `%DATE`

### Action 6: Variable Set
- Action: Variables → Variable Set
- Name: `%CP_TomorrowDate`
- To: Click the calendar icon and select "Tomorrow", or use JavaScript to format

### Action 7: JavaScriptlet
- Action: Code → JavaScriptlet
- Code: Copy and paste the following JavaScript code:

```javascript
// Parse weather data
var temp = 20;
var weather = "Unknown";
try {
  var weatherData = JSON.parse(global('CP_WeatherJSON'));
  temp = Math.round(weatherData.current.temperature_2m);
  var code = weatherData.current.weather_code;

  // Convert weather code to description
  if (code === 0) weather = "Clear";
  else if (code === 1) weather = "Mostly Clear";
  else if (code === 2) weather = "Partly Cloudy";
  else if (code === 3) weather = "Cloudy";
  else if (code >= 45 && code <= 48) weather = "Foggy";
  else if (code >= 51 && code <= 67) weather = "Rainy";
  else if (code >= 71 && code <= 77) weather = "Snowy";
  else if (code >= 80 && code <= 86) weather = "Showers";
} catch(e) {
  // Use defaults if weather fetch failed
}

// Format dates
var today = new Date();
var todayStr = today.getFullYear() + '-' +
               String(today.getMonth() + 1).padStart(2, '0') + '-' +
               String(today.getDate()).padStart(2, '0');

var tomorrow = new Date(today);
tomorrow.setDate(tomorrow.getDate() + 1);
var tomorrowStr = tomorrow.getFullYear() + '-' +
                  String(tomorrow.getMonth() + 1).padStart(2, '0') + '-' +
                  String(tomorrow.getDate()).padStart(2, '0');

// Parse calendar events
var todayEvents = [];
var tomorrowEvents = [];

// Get today's timestamp range
today.setHours(0, 0, 0, 0);
var todayStart = today.getTime();
var todayEnd = todayStart + (24 * 60 * 60 * 1000);

var tomorrowStart = tomorrow.getTime();
var tomorrowEnd = tomorrowStart + (24 * 60 * 60 * 1000);

// Process calendar entries
if (global('CALTITLE1')) {
  for (var i = 1; i <= 20; i++) {
    var title = global('CALTITLE' + i);
    var startMs = parseInt(global('CALSTART' + i));

    if (!title || !startMs) break;

    var eventDate = new Date(startMs);
    var timeStr = String(eventDate.getHours()).padStart(2, '0') + ':' +
                  String(eventDate.getMinutes()).padStart(2, '0');

    var event = {
      title: title,
      time: timeStr
    };

    // Categorize by date
    if (startMs >= todayStart && startMs < todayEnd && todayEvents.length < 5) {
      todayEvents.push(event);
    } else if (startMs >= tomorrowStart && startMs < tomorrowEnd && tomorrowEvents.length < 3) {
      tomorrowEvents.push(event);
    }
  }
}

// Build final calendar JSON
var calendarData = {
  today: {
    date: todayStr,
    temperature: temp,
    weather: weather,
    events: todayEvents,
    tasks: []
  },
  tomorrow: {
    date: tomorrowStr,
    events: tomorrowEvents
  },
  lastSyncTime: Math.floor(Date.now() / 1000),
  source: "Tasker"
};

// Store result
setGlobal('CP_CalendarJSON', JSON.stringify(calendarData, null, 2));
```

### Action 8: Write File
- Action: File → Write File
- File: `Download/crosspoint_calendar.json`
- Text: `%CP_CalendarJSON`
- Append: ✗ (unchecked - overwrite)

### Action 9: Flash
- Action: Alert → Flash
- Text: `Calendar exported to Download folder`

### Action 10: Notify
- Action: Alert → Notify
- Title: `CrossPoint Calendar`
- Text:
```
Calendar saved to Downloads.
File: crosspoint_calendar.json

Upload to reader:
- Via FTP to /calendar/agenda.json
- Or copy manually to SD card
```

---

## Task 3: CrossPoint Full Export

This task orchestrates the calendar query and export.

1. In Tasker, tap the **Tasks** tab
2. Tap **+** to create a new task
3. Name it **"CrossPoint Full Export"**
4. Add the following actions:

### Action 1: Perform Task
- Action: Tasker → Perform Task
- Name: `CrossPoint Get Calendar Events`
- Priority: 10

### Action 2: Wait
- Action: Task → Wait
- Seconds: 1

### Action 3: Perform Task
- Action: Tasker → Perform Task
- Name: `CrossPoint Export Calendar`
- Priority: 10

---

## Profile: Daily Auto Export at 6 AM

This profile automatically runs the export every morning.

1. In Tasker, tap the **Profiles** tab
2. Tap **+** to create a new profile
3. Name it **"CrossPoint Daily Export"**
4. Select Context: **Time**
   - From: `06:00`
   - To: `06:01`
5. Tap the checkmark
6. Select Task: **CrossPoint Full Export**

---

## Testing

### First Test Run

1. Tap the **Tasks** tab in Tasker
2. Find **"CrossPoint Full Export"**
3. Tap the play button ▶️ to run the task
4. Watch for flash messages confirming calendar query and export
5. Check your **Downloads** folder for `crosspoint_calendar.json`

### Verify the JSON File

Open `crosspoint_calendar.json` in a text editor. It should look like:

```json
{
  "today": {
    "date": "2025-01-13",
    "temperature": 22,
    "weather": "Clear",
    "events": [
      {
        "title": "Morning Meeting",
        "time": "09:00"
      }
    ],
    "tasks": []
  },
  "tomorrow": {
    "date": "2025-01-14",
    "events": []
  },
  "lastSyncTime": 1736726400,
  "source": "Tasker"
}
```

### Upload to Reader

Choose one of these methods:

**Option 1: FTP Upload (Recommended)**
- Enable FTP on your CrossPoint Reader
- Use an FTP client or the FTP action in Tasker/MacroDroid
- Upload `crosspoint_calendar.json` to `/calendar/agenda.json`

**Option 2: Manual Copy**
- Connect reader to PC via USB
- Copy `crosspoint_calendar.json` from phone Downloads to PC
- Rename to `agenda.json`
- Copy to reader's SD card at `/calendar/agenda.json`

**Option 3: BLE Upload**
- Enable BLE on your CrossPoint Reader
- Use a BLE app like nRF Connect
- Send the JSON file to the reader's calendar characteristic

---

## Troubleshooting

### No Calendar Events Found

- Check that Tasker has Calendar permission
- Verify you have events in your calendar for today or tomorrow
- Try increasing the search range in Action 4 of Task 1

### Location/Weather Not Working

- Check that Tasker has Location permission
- Enable GPS/Location services on your phone
- The script uses fallback values (20°C, "Unknown") if weather fails

### File Not Created

- Check that Tasker has Storage/Files permission
- Try changing the path to `/sdcard/Download/crosspoint_calendar.json`
- Verify the JavaScriptlet action completed without errors

### Profile Not Triggering

- Ensure the profile is enabled (toggle switch on)
- Check that battery optimization is disabled for Tasker
- Test manually first before relying on the time trigger

---

## Customization

### Change Event Limits

In the JavaScriptlet (Action 7 of Task 2), modify these lines:

```javascript
if (startMs >= todayStart && startMs < todayEnd && todayEvents.length < 5) {
  // Change 5 to your desired limit for today's events
}

if (startMs >= tomorrowStart && startMs < tomorrowEnd && tomorrowEvents.length < 3) {
  // Change 3 to your desired limit for tomorrow's events
}
```

### Change Auto-Export Time

1. Edit the **CrossPoint Daily Export** profile
2. Tap the **Time** context
3. Change the From/To times as desired

### Add More Event Data

Modify the Calendar Entry Query action to include more fields:
- End Variable: `CALEND`
- Location Variable: `CALLOC`
- Description Variable: `CALDESC`

Then update the JavaScriptlet to include this data in the JSON.

---

## Next Steps

Once your calendar JSON is uploaded to the reader:

1. **View Calendar**: Select "Calendar" from the home menu
2. **Navigate Events**: Use UP/DOWN buttons
3. **Mark Complete**: Press SELECT on an event
4. **Hide Event**: Press LEFT on an event
5. **Toggle Day**: Press RIGHT to switch between today/tomorrow

See [CALENDAR_EVENT_MANAGEMENT.md](CALENDAR_EVENT_MANAGEMENT.md) for full details on using the calendar activity.
