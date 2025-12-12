/*
 * CrowPanel 5.79" E-Paper Status Dashboard (Portrait)
 * Project: MTL_DashBoard_V2
 * Version: v0.3.1  (2025-12-06)
 *
 * What it does
 * - TOP (50%): Montreal time + month name + month calendar + day/date line
 * - MID (20%): STM Montréal metro status (Green / Orange / Yellow / Blue)
 * - BOT (30%): OpenWeatherMap current weather + icon (IconsMono.h)
 *
 * License
 * - MIT License (see LICENSE file in repo root)
 *
 * Publishing notes
 * - Do NOT commit Wi-Fi passwords or API keys. Use a local secrets file.
 * - client.setInsecure() disables TLS certificate checks (demo only).
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <time.h>

#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

#include "IconsMono.h"  // Weather icons + helpers (ICON_W, ICON_H, draw_icon_weather)

// -----------------------------------------------------------------------------
// Configuration (publish-safe placeholders)
// -----------------------------------------------------------------------------
const char* WIFI_SSID     = "YOUR_WIFI_SSID"; //*************************************************************
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";//**********************************************************
const char* OWM_API_KEY   = "YOUR_OWM_API_KEY";//************************************************************
const char* OWM_CITY      = "Montreal,CA";//*****************************************************************

const char* STM_URL = "https://www.stm.info/en/info/service-updates/metro";

// -----------------------------------------------------------------------------
// Weather model (UI reads this struct; network fills it)
// -----------------------------------------------------------------------------
struct WeatherData {
  bool   valid = false;
  float  temp  = 0.0f;
  String main;
  String description;
};

WeatherData gWeather;

// -----------------------------------------------------------------------------
// STM metro status model
// -----------------------------------------------------------------------------
String s1 = "";
String s2 = "";
String s4 = "";
String s5 = "";

struct MetroLineStatus {
  const char* name;   // Stable display label ("Green", "Orange", ...)
  String      status; // Human-readable status text extracted from STM HTML
};

MetroLineStatus g_metro[4] = {
  { "Green",  "Unknown" },
  { "Orange", "Unknown" },
  { "Yellow", "Unknown" },
  { "Blue",   "Unknown" }
};

// -----------------------------------------------------------------------------
// Timing: screen refresh vs data refresh
// -----------------------------------------------------------------------------
unsigned long gLastScreenRefresh = 0;
unsigned long gLastDataFetch     = 0;

const unsigned long SCREEN_REFRESH_INTERVAL_MS = 60UL * 1000UL;        // 1 minute; screen redraw cadence
const unsigned long DATA_INTERVAL_MS          = 10UL * 60UL * 1000UL;  // 10 minutes; network calls cadence (reduces API usage)

// -----------------------------------------------------------------------------
// Display / Wiring (CrowPanel)
// -----------------------------------------------------------------------------
const int EINK_BUSY = 48;
const int EINK_RST  = 47;
const int EINK_DC   = 46;
const int EINK_CS   = 45;
const int EINK_SCK  = 12;
const int EINK_MOSI = 11;
const int EINK_PWR  = 7;

GxEPD2_BW<GxEPD2_579_GDEY0579T93, GxEPD2_579_GDEY0579T93::HEIGHT> display(
  GxEPD2_579_GDEY0579T93(EINK_CS, EINK_DC, EINK_RST, EINK_BUSY)  // CS/DC/RST/BUSY wiring for the panel driver
);

// Screen geometry (portrait 272 x 792)
const int16_t SCREEN_W  = 272;
const int16_t SCREEN_H  = 792;

// Section heights
const int16_t TOP_H = SCREEN_H / 2;              // 50% top zone; calendar/time block
const int16_t MID_H = SCREEN_H / 5;              // 20% middle zone; STM status block
const int16_t BOT_H = SCREEN_H - TOP_H - MID_H;  // remaining 30%; weather block

// Section Y origins
const int16_t TOP_Y  = 0;
const int16_t MID_Y  = TOP_H;                    // middle starts right after the top section
const int16_t BOT_Y  = TOP_H + MID_H;            // bottom starts right after the middle section

const int16_t PAD_X  = 10;
const int16_t PAD_Y  = 10;

const char* MONTH_NAMES[12] = {
  "January","February","March","April","May","June",
  "July","August","September","October","November","December"
};

// -----------------------------------------------------------------------------
// Generic helpers
// -----------------------------------------------------------------------------

/** Turn on panel power (CrowPanel uses an external power-control GPIO). */
void displayPowerOn() {
  pinMode(EINK_PWR, OUTPUT);
  digitalWrite(EINK_PWR, HIGH); // HIGH enables the e-paper power rail
}

/** Connect to Wi-Fi with basic retries; prints dots for progress feedback. */
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // starts connection attempt in background

  uint8_t retry = 40; // 40 * 500ms = ~20s max wait
  while (WiFi.status() != WL_CONNECTED && retry--) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.printf("WiFi status: %s\n",
                WiFi.status() == WL_CONNECTED ? "CONNECTED" : "NOT CONNECTED");
}

/** Configure Montreal timezone (DST rules included) and sync time via NTP. */
void setupTime() {
  configTzTime("EST5EDT,M3.2.0/2,M11.1.0/2", // TZ string = automatic DST switch
               "pool.ntp.org",
               "time.nist.gov");

  struct tm timeinfo;
  for (int i = 0; i < 20; ++i) {
    if (getLocalTime(&timeinfo, 5000)) { // blocks up to 5s per attempt
      Serial.println("Time sync OK");
      return;
    }
    Serial.println("Waiting for time...");
  }
  Serial.println("Time sync FAILED");
}

/**
 * Word-wrap printer for Adafruit_GFX.
 * It prints word by word, measuring each chunk, and moves to a new line when needed.
 */
void drawWrappedText(Adafruit_GFX &d,
                     int16_t x, int16_t y,
                     int16_t maxWidth,
                     int16_t lineHeight,
                     const String &text)
{
  int16_t cursorX = x;
  int16_t cursorY = y;
  int start = 0;

  while (start < (int)text.length()) {
    int space = text.indexOf(' ', start);
    if (space < 0) space = text.length(); // last word: no spaces left

    String word = text.substring(start, space);
    if (word.length() == 0) {
      start = space + 1;
      continue;
    }

    String chunk = word + " "; // keep spacing stable so getTextBounds() matches what we print

    int16_t x1, y1;
    uint16_t w, h;
    d.getTextBounds(chunk, cursorX, cursorY, &x1, &y1, &w, &h); // measure pixel width of this word

    if (cursorX + (int)w > x + maxWidth) { // if word doesn't fit, wrap to next line
      cursorX = x;
      cursorY += lineHeight;
    }

    d.setCursor(cursorX, cursorY);
    d.print(chunk);
    cursorX += w; // advance "pen position" by the measured pixel width

    start = space + 1;
  }
}

// -----------------------------------------------------------------------------
// Calendar helpers
// -----------------------------------------------------------------------------

/** Leap year check used by calendar day-count. */
bool isLeapYear(int year) {
  return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0); // Gregorian leap rule
}

/** Return days in month (month 1..12), with February corrected for leap years. */
int daysInMonth(int year, int month) {
  static const int days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  int d = days[month - 1];
  if (month == 2 && isLeapYear(year)) d = 29;
  return d;
}

// -----------------------------------------------------------------------------
// STM HTML helpers (fast + small; not a full HTML parser)
// -----------------------------------------------------------------------------

/** Trim leading/trailing whitespace so UI doesn't show "ragged" text. */
String trimWS(const String& s) {
  int a = 0, b = s.length() - 1;
  while (a <= b && isspace((unsigned char)s[a])) a++;
  while (b >= a && isspace((unsigned char)s[b])) b--;
  if (b < a) return "";
  return s.substring(a, b + 1);
}

/** Remove "<...>" tags from a snippet (good enough for the STM layout). */
String stripTags(const String& in) {
  String out;
  out.reserve(in.length());      // reserve reduces String reallocations (less heap churn)
  bool inTag = false;
  for (int i = 0; i < (int)in.length(); ++i) {
    char c = in[i];
    if (c == '<') { inTag = true;  continue; }  // enter tag
    if (c == '>') { inTag = false; continue; }  // exit tag
    if (!inTag) out += c;                       // keep only visible text
  }
  return trimWS(out);
}

/**
 * Extract the first <p>...</p> block following a line label.
 * Searches English first, then French label.
 */
String extractLineStatus(const String &html,
                         const char* labelEn,
                         const char* labelFr)
{
  int pos = html.indexOf(labelEn);              // find "Line 1 - Green"
  if (pos < 0 && labelFr && labelFr[0]) {
    pos = html.indexOf(labelFr);                // fallback: "Ligne 1 - Verte"
  }

  if (pos < 0) {
    String msg = "Status not found (no match for '";
    msg += labelEn;
    if (labelFr && labelFr[0]) { msg += "' or '"; msg += labelFr; }
    msg += "')";
    return msg;
  }

  int pOpen = html.indexOf("<p", pos);          // find the first paragraph after the header marker
  if (pOpen < 0) return "Status not found (no <p> after header)";

  int pStart = html.indexOf('>', pOpen);        // locate end of opening tag: <p ... >
  if (pStart < 0) return "Status not found ('>' missing)";
  pStart++;                                     // start of actual text content

  int pEnd = html.indexOf("</p>", pStart);      // normal case: close tag exists
  if (pEnd < 0) {
    // fallback: bounded extraction so we don't accidentally grab the whole page
    pEnd = html.indexOf("\n", pStart);
    if (pEnd < 0 || pEnd > pStart + 200) pEnd = pStart + 200;
    if (pEnd > html.length()) pEnd = html.length();
  }

  String frag = html.substring(pStart, pEnd);
  frag.replace("&nbsp;", " ");                  // normalize common HTML space entity
  return stripTags(frag);
}

/**
 * Fetch STM HTML as plain text (tries to avoid gzip).
 * Returns false if we detect gzip (0x1F 0x8B) because we don't decompress here.
 */
bool fetchHtml(String &outHtml) {
  outHtml = "";

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERROR] WiFi not connected.");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();                          // demo only: skips TLS certificate verification

  HTTPClient http;
  if (!http.begin(client, STM_URL)) {
    Serial.println("[ERROR] http.begin() failed");
    return false;
  }

  http.useHTTP10(true);                          // HTTP/1.0 often reduces server-side compression/transfer tricks
  http.setReuse(false);                          // avoid keeping sockets around; simpler behavior for a demo
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader("Accept-Encoding", "identity");  // explicitly ask for uncompressed content

  int code = http.GET();
  Serial.printf("[INFO] STM HTTP code: %d\n", code);

  if (code <= 0) {
    Serial.printf("[ERROR] STM GET failed: %s\n", http.errorToString(code).c_str());
    http.end();
    return false;
  }
  if (code != HTTP_CODE_OK) {
    Serial.println("[ERROR] STM Non-200 response.");
    http.end();
    return false;
  }

  String body = http.getString();                // loads entire response into RAM (fine for this page size)
  http.end();

  Serial.printf("[INFO] STM payload length: %d bytes\n", body.length());

  // gzip magic bytes: 0x1F 0x8B => compressed response; bail out early.
  if (body.length() >= 2 &&
      (uint8_t)body[0] == 0x1F && (uint8_t)body[1] == 0x8B) {
    Serial.println("[WARN] STM body looks gzip-compressed (0x1F 0x8B).");
    Serial.println("[WARN] Without a gzip lib, parsing won't work reliably.");
    return false;
  }

  outHtml = body;
  return true;
}

// -----------------------------------------------------------------------------
// OpenWeatherMap fetch
// -----------------------------------------------------------------------------

/**
 * Fetch current weather (JSON) and fill WeatherData.
 * Keeps UI logic simple: drawBottomSection_Weather() only reads gWeather.
 */
bool fetchWeather(WeatherData &w) {
  if (WiFi.status() != WL_CONNECTED) {
    w.valid = false;
    return false;
  }

  String url = "http://api.openweathermap.org/data/2.5/weather?q=";
  url += OWM_CITY;
  url += "&units=metric&appid=";
  url += OWM_API_KEY;

  HTTPClient http;
  if (!http.begin(url)) {
    Serial.println("OWM: begin() failed");
    w.valid = false;
    return false;
  }

  int httpCode = http.GET();
  Serial.printf("OWM HTTP: %d\n", httpCode);
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    w.valid = false;
    return false;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<2048> doc;                  // fixed-size JSON buffer (keeps memory predictable)
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.println("OWM: JSON parse error");
    w.valid = false;
    return false;
  }

  JsonVariant main  = doc["main"];               // doc["main"]["temp"]
  JsonArray  weatherArr = doc["weather"];        // array like: [{ "main": "...", "description": "..." }]

  if (main.isNull() || weatherArr.isNull() || weatherArr.size() == 0) {
    Serial.println("OWM: missing fields");
    w.valid = false;
    return false;
  }

  float temp = main["temp"] | NAN;               // "|" provides a safe default (NAN) if field missing
  const char* mainStr = weatherArr[0]["main"]        | "";
  const char* descStr = weatherArr[0]["description"] | "";

  if (isnan(temp)) {
    w.valid = false;
    return false;
  }

  w.temp        = temp;
  w.main        = String(mainStr);
  w.description = String(descStr);
  w.valid       = true;
  return true;
}

// -----------------------------------------------------------------------------
// STM metro fetch + update
// -----------------------------------------------------------------------------

/**
 * Fetch STM HTML, extract 4 lines, print to Serial (debug), and store in s1/s2/s4/s5.
 * Side effect: fills globals used by fetchSTMMetroStatus().
 */
void printStmStatus() {
  String html;
  if (!fetchHtml(html)) {
    Serial.println("[ERROR] Failed to fetch/decode STM status page.");
    return;
  }

  Serial.println("[DEBUG] STM HTML preview (first ~400 chars):");
  Serial.println("-----------------------------------");
  Serial.println(html.substring(0, 400));
  Serial.println("-----------------------------------");

  s1 = extractLineStatus(html, "Line 1 - Green",  "Ligne 1 - Verte");
  s2 = extractLineStatus(html, "Line 2 - Orange", "Ligne 2 - Orange");
  s4 = extractLineStatus(html, "Line 4 - Yellow", "Ligne 4 - Jaune");
  s5 = extractLineStatus(html, "Line 5 - Blue",   "Ligne 5 - Bleue");

  Serial.println();
  Serial.println("[RESULT] STM Metro Status:");
  Serial.println("-----------------------------------");
  Serial.printf("Line 1 - Green : %s\n", s1.c_str());
  Serial.printf("Line 2 - Orange: %s\n", s2.c_str());
  Serial.printf("Line 4 - Yellow: %s\n", s4.c_str());
  Serial.printf("Line 5 - Blue  : %s\n", s5.c_str());
  Serial.println("-----------------------------------");
}

/**
 * Stub for future "section parsing" approach.
 * Kept intentionally empty to avoid half-working logic in a published demo.
 */
String parseLineStatus(const String &html, const char* lineMarker) {
  (void)html;        // avoid unused warnings while function is a stub
  (void)lineMarker;  // "
  return "";
}

/**
 * Update the g_metro[] array from STM.
 * Returns true when updated, false when Wi-Fi is down or fetch fails.
 */
bool fetchSTMMetroStatus() {
  if (WiFi.status() != WL_CONNECTED) return false;

  printStmStatus();              // populates s1/s2/s4/s5

  g_metro[0].status = s1;        // map to display order (Green, Orange, Yellow, Blue)
  g_metro[1].status = s2;
  g_metro[2].status = s4;
  g_metro[3].status = s5;

  Serial.println("STM status updated:");
  for (int i = 0; i < 4; ++i) {
    Serial.printf("  %s: %s\n", g_metro[i].name, g_metro[i].status.c_str());
  }

  return true;
}

// -----------------------------------------------------------------------------
// Screen drawing (3 sections)
// -----------------------------------------------------------------------------

/** Draw TOP section: time + month + calendar grid + day/date line. */
void drawTopSection_Calendar() {
  int16_t y0 = TOP_Y;
  int16_t h  = TOP_H;

  display.fillRect(0, y0, SCREEN_W, h, GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);

  struct tm timeinfo;
  bool hasTime = getLocalTime(&timeinfo);          // needs setupTime() + Wi-Fi/NTP before it works

  char timeStr[6]  = "--:--";
  char dateStr[16] = "----/--/--";
  char dayStr[12]  = "No time";

  int year  = 2000;
  int month = 1;
  int dom   = 1;

  if (hasTime) {
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);
    strftime(dayStr,  sizeof(dayStr),  "%A",      &timeinfo);
    year  = timeinfo.tm_year + 1900;               // tm_year is years since 1900
    month = timeinfo.tm_mon + 1;                   // tm_mon is 0..11, we want 1..12
    dom   = timeinfo.tm_mday;
  }

  // Section title
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextSize(1);
  display.setCursor(PAD_X, y0 + PAD_Y + 12);
  display.print("MONTREAL TIME");

  // Big centered time
  display.setFont(&FreeSansBold24pt7b);
  display.setTextSize(1);

  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(timeStr, 0, 0, &tbx, &tby, &tbw, &tbh); // measure text box to center it

  int16_t timeX = (SCREEN_W - tbw) / 2;            // center horizontally
  int16_t timeY = y0 + 80;

  display.setCursor(timeX, timeY);
  display.print(timeStr);

  // Month name centered under time
  display.setFont(&FreeSans12pt7b);
  display.setTextSize(1);

  String monthName = hasTime ? MONTH_NAMES[month - 1] : String("Unknown"); // month-1 => 0..11 index
  int16_t mx, my;
  uint16_t mw, mh2;
  display.getTextBounds(monthName, 0, 0, &mx, &my, &mw, &mh2);

  int16_t monthX = (SCREEN_W - mw) / 2;
  int16_t monthY = timeY + 30;

  display.setCursor(monthX, monthY);
  display.print(monthName);

  // Calendar grid (only if time is valid)
  if (!hasTime) return;

  struct tm first = timeinfo;
  first.tm_mday = 1;                               // set day-of-month to 1
  mktime(&first);                                  // normalize struct and compute tm_wday

  int wdayFirst = first.tm_wday;                   // 0=Sun..6=Sat
  int startCol  = (wdayFirst + 6) % 7;             // convert to Monday-based index (Mo=0..Su=6)

  int days = daysInMonth(year, month);

  const char* weekDays[7] = {"Mo","Tu","We","Th","Fr","Sa","Su"};

  int16_t calTopY = monthY + 50;
  int16_t colW    = (SCREEN_W - 2 * PAD_X) / 7;    // 7 columns
  int16_t rowH    = 22;

  // Weekday header row
  display.setFont(&FreeSans12pt7b);
  for (int c = 0; c < 7; ++c) {
    String wd = weekDays[c];

    int16_t bx, by;
    uint16_t bw, bh;
    display.getTextBounds(wd, 0, 0, &bx, &by, &bw, &bh);

    int16_t x = PAD_X + c * colW + (colW - bw) / 2; // center each weekday label inside its column
    int16_t y = calTopY;

    display.setCursor(x, y);
    display.print(wd);
  }

  // Day numbers
  display.setFont(&FreeMonoBold9pt7b);

  for (int day = 1; day <= days; ++day) {
    int offset = startCol + (day - 1);             // linear index from first day
    int row    = offset / 7;                       // row in calendar grid (0-based)
    int col    = offset % 7;                       // col in calendar grid (0..6)

    char buf[3];
    snprintf(buf, sizeof(buf), "%d", day);
    String dstr = String(buf);

    int16_t bx, by;
    uint16_t bw, bh;
    display.getTextBounds(dstr, 0, 0, &bx, &by, &bw, &bh);

    int16_t x = PAD_X + col * colW + (colW - bw) / 2;     // center day number
    int16_t y = calTopY + rowH * (row + 1);               // +1 to account for weekday header row

    display.setCursor(x, y);
    display.print(dstr);
  }

  // Day + date line under the calendar
  String dayDateLine = String(dayStr) + "  " + String(dateStr);

  display.setFont(&FreeSans12pt7b);
  display.setTextSize(1);

  int16_t ddx, ddy;
  uint16_t ddw, ddh;
  display.getTextBounds(dayDateLine, 0, 0, &ddx, &ddy, &ddw, &ddh);

  int16_t dayLineX = (SCREEN_W - ddw) / 2;
  int16_t dayLineY = y0 + h - 80;

  display.setCursor(dayLineX, dayLineY);
  display.print(dayDateLine);
}

/** Draw MID section: STM metro statuses from g_metro[]. */
void drawMiddleSection_Metro() {
  int16_t y0 = MID_Y;
  int16_t h  = MID_H;

  display.fillRect(0, y0, SCREEN_W, h, GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);

  display.setFont(&FreeMonoBold9pt7b);
  display.setTextSize(1);
  display.setCursor(PAD_X, y0 + PAD_Y - 10);
  display.print("STM METRO STATUS");

  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1);

  int16_t rowY    = y0 + PAD_Y + 35;
  int16_t rowStep = 22;

  for (int i = 0; i < 4; ++i) {
    int16_t y = rowY + i * rowStep;
    if (y > y0 + h - 10) break;                    // prevents drawing outside section if fonts change

    display.setCursor(PAD_X, y);
    display.print(g_metro[i].name);
    display.print(": ");

    display.setCursor(PAD_X + 90, y);              // fixed column for status text (simple layout)
    display.print(g_metro[i].status);
  }
}

/** Draw BOT section: weather values from gWeather + icon from IconsMono.h. */
void drawBottomSection_Weather() {
  int16_t y0 = BOT_Y;
  int16_t h  = BOT_H;

  display.fillRect(0, y0, SCREEN_W, h, GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);

  display.setFont(&FreeMonoBold9pt7b);
  display.setTextSize(1);
  display.setCursor(PAD_X, y0 + PAD_Y + 10);
  display.print("WEATHER ");
  display.print(OWM_CITY);

  if (!gWeather.valid) {
    display.setFont(&FreeSans12pt7b);
    display.setCursor(PAD_X, y0 + PAD_Y + 40);
    display.print("No data (WiFi/API)");
    return;
  }

  // Big temperature
  display.setFont(&FreeSansBold24pt7b);

  char tempStr[16];
  snprintf(tempStr, sizeof(tempStr), "%.1f", gWeather.temp);

  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(tempStr, 0, 0, &tbx, &tby, &tbw, &tbh); // used for positioning "C"

  int16_t bigX = PAD_X;
  int16_t bigY = y0 + PAD_Y + 100;

  display.setTextSize(2);                           // x2 scaling makes temperature pop on e-paper
  display.setCursor(bigX, bigY);
  display.print(tempStr);

  // °C label
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextSize(1);

  int16_t degX = bigX + tbw * 2 + 10;               // tbw * 2 because temperature was printed with setTextSize(2)
  int16_t degY = bigY - 30;

  display.setCursor(degX, degY);
  display.print("C");

  // Description (wrapped)
  display.setFont(&FreeSans12pt7b);
  display.setTextSize(1);

  String desc = gWeather.description;
  desc.toLowerCase();                               // consistent style (OWM sometimes returns mixed case)

  int16_t descX = PAD_X;
  int16_t descY = bigY + 70;
  int16_t descW = SCREEN_W - ICON_W - 3 * PAD_X;    // reserve space on the right for the icon
  int16_t lineH = 18;

  drawWrappedText(display, descX, descY, descW, lineH, desc);

  // Icon on the right
  int16_t iconX = SCREEN_W - ICON_W - PAD_X;
  int16_t iconY = y0 + (h - ICON_H) / 2;            // vertical center inside the bottom section

  String iconText = gWeather.main + " " + gWeather.description; // drive icon selection by "main + description"
  draw_icon_weather(display, iconX, iconY, iconText);
}

/** Draw the full UI in one e-paper refresh pass. */
void drawFullUI() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    display.drawFastHLine(0, MID_Y, SCREEN_W, GxEPD_BLACK); // separator between top and middle
    display.drawFastHLine(0, BOT_Y, SCREEN_W, GxEPD_BLACK); // separator between middle and bottom

    drawTopSection_Calendar();
    drawMiddleSection_Metro();
    drawBottomSection_Weather();
  } while (display.nextPage());                      // required for GxEPD2 to push the full framebuffer
}

// -----------------------------------------------------------------------------
// Data fetch orchestration
// -----------------------------------------------------------------------------

/** Fetch weather + STM metro status; reconnect Wi-Fi if needed. */
void fetchAllData() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();                                  // simple recovery for intermittent Wi-Fi
  }

  fetchWeather(gWeather);                           // updates gWeather.valid + fields
  fetchSTMMetroStatus();                            // updates g_metro[] statuses
}

// -----------------------------------------------------------------------------
// Arduino setup / loop
// -----------------------------------------------------------------------------

/** Initialize Serial, display, Wi-Fi, time sync, first data fetch, then render UI. */
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("STATUS_QUO_EPAPER_LUCA_DEMO1 v0.3.1");

  displayPowerOn();

  SPI.begin(EINK_SCK, -1, EINK_MOSI, EINK_CS);      // MISO not used on this wiring (-1)
  display.init(115200);
  display.setRotation(1);                           // portrait orientation as used by SCREEN_W/SCREEN_H constants

  // Initial clear (ensures clean background after boot)
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
  } while (display.nextPage());

  connectWiFi();
  setupTime();
  fetchAllData();

  drawFullUI();

  gLastScreenRefresh = millis();
  gLastDataFetch     = millis();
}

/**
 * Periodic refresh:
 * - Data fetch every DATA_INTERVAL_MS
 * - Screen redraw every SCREEN_REFRESH_INTERVAL_MS
 */
void loop() {
  unsigned long now = millis();

  if (now - gLastDataFetch >= DATA_INTERVAL_MS) {
    gLastDataFetch = now;
    fetchAllData();
  }

  if (now - gLastScreenRefresh >= SCREEN_REFRESH_INTERVAL_MS) {
    gLastScreenRefresh = now;
    drawFullUI();
  }
}
