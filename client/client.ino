#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "EPD.h"
#include "Pic.h"

extern uint8_t ImageBW[ALLSCREEN_BYTES];

// ============================================================
//  WI-FI / PROXY CONFIG
// ============================================================
const char* WIFI_SSID = "ORBI37";
const char* WIFI_PASS = "fuzzysocks068";
const char* PROXY_URL = "http://192.168.1.49:5000/subway";

int times_startX = 7; //Starting horizontal axis
int times_startY = 40;  //Starting ordinate
int times_fontSize = 16; // font size
int times_endX = 250;    // End horizontal axis
int times_endY = 122;    // End vertical axis

int stops_startX = 44; //Starting horizontal axis
int stops_startY = 6;  //Starting ordinate
int stops_fontSize = 24; // font size
int stops_endX = 250;    // End horizontal axis
int stops_endY = 122;    // End vertical axis

const char *n_stop_name = "Manhattan Bound";
const char *s_stop_nameP1 = "Coney";
const char *s_stop_nameP2 = "Island";
const char *s_stop_nameP3 = "Bound";

// These now get filled from the proxy server at runtime instead of
// being hardcoded — same "4 min, 17 min, 23 min, 37 min" format,
// just built dynamically. A true const char* can't be reassigned,
// so these are mutable buffers instead.
#define TIME_STR_LEN 64
char n_departure_times[TIME_STR_LEN] = "Loading...";
char s_departure_times[TIME_STR_LEN] = "Loading...";

// ============================================================
//  PERIODIC REFRESH CONFIG
// ============================================================
unsigned long lastUpdateMs = 0;
const unsigned long UPDATE_INTERVAL_MS = 30000; // how often to re-poll + redraw times (30s)

// SSD1680 panels accumulate visible "ghosting" over repeated partial
// refreshes since only R24H (current frame) gets updated each cycle —
// R26H (the reference frame used to compute the partial-refresh diff)
// never gets touched. Every FULL_REFRESH_INTERVAL partial updates, we
// do one full white-flash refresh (same sequence as setup()) to reset
// R26H and clear any built-up ghost artifacts.
int partialRefreshCount = 0;
const int FULL_REFRESH_INTERVAL = 1; // ~15 partials * 30s = every 7.5 min


// ============================================================
//  WI-FI CONNECT
// ============================================================
void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    Serial.print("Connecting to Wi-Fi");
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
        Serial.print(".");
        delay(500);
        timeout++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Wi-Fi connected!");
    } else {
        Serial.println("Wi-Fi connection failed!");
    }
}


// ============================================================
//  FETCH DEPARTURE TIMES FROM PROXY
// ============================================================

// Fills outBuf with a string like "4 min, 17 min, 23 min, 37 min"
void buildTimeString(JsonArray times, char* outBuf, size_t bufSize) {
    outBuf[0] = '\0';
    char entry[16];
    bool first = true;

    for (JsonVariant t : times) {
        snprintf(entry, sizeof(entry), "%s%d min", first ? "" : ", ", t.as<int>());
        strncat(outBuf, entry, bufSize - strlen(outBuf) - 1);
        first = false;
    }

    if (first) {  // no entries came back
        strncpy(outBuf, "No data", bufSize - 1);
    }
}

// Fetches the proxy JSON and fills n_departure_times / s_departure_times.
// Returns true on success.
bool fetchDepartureTimes() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi not connected, skipping fetch.");
        return false;
    }

    Serial.println("\nFetching latest subway data from proxy...");

    HTTPClient http;
    http.begin(PROXY_URL);
    int httpCode = http.GET();

    if (httpCode != 200) {
        Serial.print("HTTP request failed, code: ");
        Serial.println(httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.print("Error parsing data: ");
        Serial.println(err.c_str());
        return false;
    }

    buildTimeString(doc["manhattan"].as<JsonArray>(), n_departure_times, TIME_STR_LEN);
    buildTimeString(doc["coney"].as<JsonArray>(),     s_departure_times, TIME_STR_LEN);

    Serial.println("==============================");
    Serial.print("N Train (Manhattan-Bound): ");
    Serial.println(n_departure_times);
    Serial.println("------------------------------");
    Serial.print("N Train (Coney Island-Bound): ");
    Serial.println(s_departure_times);
    Serial.println("==============================");

    return true;
}


// ============================================================
//  PARTIAL-AREA CLEAR (paints a rectangle white)
// ============================================================
// NOTE: This assumes EPD.h does NOT already expose a dedicated
// "fill rectangle" / "clear window" helper. If your EPD.h has one
// (something like EPD_Fill_Rect or Paint_ClearWindows), swap this
// out for that — it will be faster than drawing row-by-row lines.
void clearRegion(int startX, int startY, int endX, int endY) {
    for (int y = startY; y < endY; y++) {
        EPD_DrawLine(startX, y, endX, y, WHITE);
    }
}


// ============================================================
//  STATIC ELEMENTS (images, divider lines, stop-name labels)
// ============================================================
// Drawn once in setup(), and again during a periodic full refresh.
// Kept as one function so both places always stay in sync.
void drawStaticElements() {
    EPD_ShowPicture(4, 4, 32, 32, gImage_Subway_Sign, BLACK); // Display a picture on the EPD

    EPD_DrawLine(1, 60, 248, 61, BLACK);

    EPD_ShowPicture(4, 65, 32, 32, gImage_Subway_Sign, BLACK); // Display a picture on the EPD

    EPD_DrawLine(1, 120, 248, 121, BLACK);

    Part_Text_Display(n_stop_name, stops_startX, stops_startY, stops_fontSize, BLACK, stops_endX, stops_endY);

    //"Coney-Island Bound" display text is segmented into three displays, otherwise it'll be displayed on the next line
    Part_Text_Display(s_stop_nameP1, stops_startX - 4, stops_startY + 62, stops_fontSize, BLACK, stops_endX, stops_endY);
    Part_Text_Display(s_stop_nameP2, stops_startX - 4 + 68, stops_startY + 62, stops_fontSize, BLACK, stops_endX, stops_endY);
    Part_Text_Display(s_stop_nameP3, stops_startX - 4 + 146, stops_startY + 62, stops_fontSize, BLACK, stops_endX, stops_endY);
    EPD_DrawLine(102, 80, 107, 80, BLACK);
}


// ============================================================
//  PERIODIC FULL REFRESH (anti-ghosting maintenance)
// ============================================================
// Mirrors the exact sequence setup() uses: white-flash full refresh,
// clear R26H, then redraw everything (static elements + current times)
// and push it with a partial update. Resets partialRefreshCount.
void fullRefreshAndRedraw() {
    EPD_Init();
    EPD_ALL_Fill(WHITE);
    EPD_Update();
    EPD_Clear_R26H();

    drawStaticElements();
    Part_Text_Display(n_departure_times, times_startX, times_startY, times_fontSize, BLACK, times_endX, times_endY);
    Part_Text_Display(s_departure_times, times_startX, times_startY + 62, times_fontSize, BLACK, times_endX, times_endY);

    EPD_DisplayImage(ImageBW);
    EPD_PartUpdate();
    EPD_Sleep();

    partialRefreshCount = 0;
}


// ============================================================
//  REDRAW ONLY THE DEPARTURE TIMES
// ============================================================
// Fetches fresh times and repaints just the two time-text regions,
// leaving the images, divider lines, and stop-name labels untouched.
// Every FULL_REFRESH_INTERVAL calls, does a full anti-ghosting
// refresh instead (see fullRefreshAndRedraw above).
void updateDepartureTimesOnly() {
    if (!fetchDepartureTimes()) {
        // Keep whatever was already on screen rather than wiping it
        return;
    }

    partialRefreshCount++;
    if (partialRefreshCount >= FULL_REFRESH_INTERVAL) {
        fullRefreshAndRedraw();
        return;
    }

    EPD_Init();

    // Clear the two time-text regions only (top row + bottom row)
    clearRegion(times_startX, times_startY, times_endX, times_startY + times_fontSize * 2);
    clearRegion(times_startX, times_startY + 62, times_endX, times_startY + 62 + times_fontSize * 2);

    // Draw the freshly-fetched times back in
    Part_Text_Display(n_departure_times, times_startX, times_startY, times_fontSize, BLACK, times_endX, times_endY);
    Part_Text_Display(s_departure_times, times_startX, times_startY + 62, times_fontSize, BLACK, times_endX, times_endY);

    EPD_DisplayImage(ImageBW); // Push the updated buffer to the display
    EPD_PartUpdate();          // Fast partial-refresh waveform
    EPD_Sleep();
}


// Function to set up the system. Executed once when the program starts.
void setup() {
    Serial.begin(115200);
    delay(1000);
    connectWiFi();
    fetchDepartureTimes();  // fills n_departure_times / s_departure_times before drawing

    // Configure pin 7 for screen power control
    pinMode(7, OUTPUT);        // Set pin 7 as output
    digitalWrite(7, HIGH);     // Activate screen power by setting pin 7 high

    EPD_Init();                // Initialize the EPD
    EPD_ALL_Fill(WHITE);       // Fill the entire EPD with white color
    EPD_Update();              // Update the EPD display
    EPD_Clear_R26H();          // Clear the EPD using a specific method

    // ---- Everything below this point is drawn ONCE and is never redrawn ----
    drawStaticElements();

    // ---- The times, however, get drawn every refresh, so draw the initial values here too ----
    Part_Text_Display(n_departure_times, times_startX, times_startY, times_fontSize, BLACK, times_endX, times_endY);
    Part_Text_Display(s_departure_times, times_startX, times_startY + 62, times_fontSize, BLACK, times_endX, times_endY);

    EPD_DisplayImage(ImageBW); // Display the image stored in ImageBW array
    EPD_PartUpdate();          // Perform a partial update on the EPD
    EPD_Sleep();               // Put the EPD to sleep mode

    lastUpdateMs = millis();
}

// The main loop function.
// Every UPDATE_INTERVAL_MS, re-fetch and redraw ONLY the departure times.
void loop() {
    if (millis() - lastUpdateMs >= UPDATE_INTERVAL_MS) {
        lastUpdateMs = millis();
        updateDepartureTimesOnly();
    }
}

// Function to clear all content on the EPD.
void clear_all() {
    EPD_Init();                // Initialize the EPD
    EPD_ALL_Fill(WHITE);       // Fill the entire EPD with white color
    EPD_Update();              // Update the EPD display
    EPD_Clear_R26H();          // Clear the EPD using a specific method
    EPD_Sleep();               // Put the EPD to sleep mode
}

/*
*---------Function description: Display text content locally------------
*----Parameter introduction:
    content：Text content
    startX：Starting horizontal axis
    startY：Starting ordinate
    fontSize：font size
    color：font color
    endX：End horizontal axis
    endY：End vertical axis
*/

void Part_Text_Display(const char* content, int startX, int startY, int fontSize, int color, int endX, int endY) {
    int length = strlen(content);
    int i = 0;
    char line[(endX - startX) / (fontSize/2) + 1]; //Calculate the maximum number of characters per line based on the width of the area
    int currentX = startX;
    int currentY = startY;
    int lineHeight = fontSize;

    while (i < length) {
        int lineLength = 0;
        memset(line, 0, sizeof(line));

        //Fill the line until it reaches the width of the region or the end of the string
        while (lineLength < (endX - startX) / (fontSize/2) && i < length) {
            line[lineLength++] = content[i++];
        }

        // If the current Y coordinate plus font size exceeds the area height, stop displaying
        if (currentY + lineHeight > endY) {
            break;
        }
        // Display this line
        EPD_ShowString(currentX, currentY, line, color, fontSize);

        // Update the Y coordinate for displaying the next line
        currentY += lineHeight;

        // If there are still remaining strings but they have reached the bottom of the area, stop displaying them
        if (currentY + lineHeight > endY) {
            break;
        }
    }
}