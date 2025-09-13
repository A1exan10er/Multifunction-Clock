#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <time.h>

// put function declarations here:
void drawDigitalClock(int h, int m, int s);
void drawAnalogClock(int h, int m, int s);
void drawDate(int y, int mo, int d);
void updateClocks();
void setupWiFi();
void setupWebServer();
void connectToWiFi(const char* ssid, const char* password);
void handleWiFiConnection(); // New async handler
void syncTimeFromNTP();
String getHtmlPage();
bool isLeapYear(int y);
int getDaysInMonth(int y, int mo);

TFT_eSPI tft = TFT_eSPI();
unsigned long lastUpdate = 0;

// Calculate clock update interval and time increment based on SPI frequency
// Logic: If display refreshes slower than 1 second, time advances by multiple seconds to stay accurate
//        If display refreshes faster than 1 second, time advances by 1 second normally
// Examples:
// - 1,000,000 Hz (1MHz) → 1000ms interval, +1 second per update
// -   500,000 Hz (500kHz) → 2000ms interval, +2 seconds per update (stays accurate)
// -   250,000 Hz (250kHz) → 4000ms interval, +4 seconds per update (stays accurate)
// - 2,000,000 Hz (2MHz) → 1000ms interval, +1 second per update (no faster than real time)
const unsigned long DISPLAY_REFRESH_TIME = (1000000UL / SPI_FREQUENCY) * 1000UL;
const unsigned long CLOCK_UPDATE_INTERVAL = max(1000UL, DISPLAY_REFRESH_TIME);
const int TIME_INCREMENT_SECONDS = max(1, (int)(DISPLAY_REFRESH_TIME / 1000UL));
int hours = 12, minutes = 0, seconds = 0;
int year = 2025, month = 9, day = 13;  // Default date

// Previous values for change detection
int prevHours = -1, prevMinutes = -1, prevSeconds = -1;
int prevYear = -1, prevMonth = -1, prevDay = -1;
bool firstUpdate = true;

// Access Point credentials
const char* ap_ssid = "MultifunctionClock";
const char* ap_password = "12345678";

// WiFi connection variables
String wifi_ssid = "";
String wifi_password = "";
bool wifi_connected = false;
bool ntp_synced = false;
bool wifi_connect_requested = false;
unsigned long wifi_connect_start = 0;
bool wifi_connecting = false;

// Timezone settings (can be configured via web interface)
long current_gmt_offset_sec = -25200;  // Default to PDT (-7 hours)
int current_daylight_offset_sec = 0;   // Included in gmt_offset_sec

// NTP settings
const char* ntp_server = "pool.ntp.org";

AsyncWebServer server(80);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(2000); // Give time for serial monitor to connect
  Serial.println();
  Serial.println("===========================================");
  Serial.println("      MULTIFUNCTION CLOCK STARTING       ");
  Serial.println("===========================================");
  Serial.print("SPI Frequency: ");
  Serial.print(SPI_FREQUENCY);
  Serial.println(" Hz");
  Serial.print("Clock Update Interval: ");
  Serial.print(CLOCK_UPDATE_INTERVAL);
  Serial.println(" ms");
  Serial.print("Time Increment per Update: ");
  Serial.print(TIME_INCREMENT_SECONDS);
  Serial.println(" seconds");
  Serial.println("-------------------------------------------");
  
  // Initialize TFT display
  tft.init();
  tft.setRotation(1); // Landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  
  // Show startup message
  tft.drawString("Multifunction Clock", 120, 60, 4);
  tft.drawString("Starting Access Point...", 120, 100, 2);
  
  // Setup Access Point and Web Server
  Serial.println("Setting up WiFi Access Point...");
  setupWiFi();
  Serial.println("Setting up Web Server...");
  setupWebServer();
  
  // Clear screen and start clock
  Serial.println("Initializing clock display...");
  tft.fillScreen(TFT_BLACK);
  updateClocks();
  
  Serial.println("===========================================");
  Serial.println("         SETUP COMPLETE - READY!         ");
  Serial.println("===========================================");
  Serial.println("Connect to: " + String(ap_ssid));
  Serial.println("Password: " + String(ap_password));
  Serial.println("IP: " + WiFi.softAPIP().toString());
  Serial.println("===========================================");
}

void loop() {
  // put your main code here, to run repeatedly:
  static unsigned long lastHeartbeat = 0;
  
  // Prevent watchdog reset
  yield();
  
  // Handle WiFi connection asynchronously
  handleWiFiConnection();
  
  // Heartbeat every 10 seconds to show ESP32 is alive
  if (millis() - lastHeartbeat >= 10000) {
    lastHeartbeat = millis();
    Serial.print("💓 HEARTBEAT - Uptime: ");
    Serial.print(millis() / 1000);
    Serial.print("s, WiFi: ");
    Serial.print(wifi_connected ? "CONNECTED" : (wifi_connecting ? "CONNECTING" : "DISCONNECTED"));
    Serial.print(", Free RAM: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
  }
  
  if (millis() - lastUpdate >= CLOCK_UPDATE_INTERVAL) {
    lastUpdate = millis();
    
    // Advance time by the calculated increment to stay accurate
    seconds += TIME_INCREMENT_SECONDS;
    
    // Handle overflow for seconds, minutes, hours, and days
    while (seconds >= 60) {
      seconds -= 60;
      minutes++;
      if (minutes >= 60) {
        minutes -= 60;
        hours++;
        if (hours >= 24) {
          hours -= 24;
          // Advance to next day
          day++;
          if (day > getDaysInMonth(year, month)) {
            day = 1;
            month++;
            if (month > 12) {
              month = 1;
              year++;
            }
          }
        }
      }
    }
    updateClocks();
  }
}

// put function definitions here:

void updateClocks() {
  // Only clear screen on first update
  if (firstUpdate) {
    tft.fillScreen(TFT_BLACK);
    // Show Access Point IP address at the bottom (static, only draw once)
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("AP: " + WiFi.softAPIP().toString(), 120, 300, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    firstUpdate = false;
  }
  
  // Only update date if it changed
  if (year != prevYear || month != prevMonth || day != prevDay) {
    drawDate(year, month, day);
    prevYear = year;
    prevMonth = month;
    prevDay = day;
  }
  
  // Only update digital clock if time changed
  if (hours != prevHours || minutes != prevMinutes || seconds != prevSeconds) {
    drawDigitalClock(hours, minutes, seconds);
    prevHours = hours;
    prevMinutes = minutes;
    prevSeconds = prevSeconds;
  }
  
  // Always update analog clock for smooth second hand movement
  drawAnalogClock(hours, minutes, seconds);
}

void drawDate(int y, int mo, int d) {
  // Clear the date area
  tft.fillRect(30, 5, 180, 30, TFT_BLACK);
  
  char dateBuf[12];
  sprintf(dateBuf, "%02d-%02d-%04d", d, mo, y);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(dateBuf, 120, 20, 4); // Date at very top
  tft.setTextColor(TFT_WHITE, TFT_BLACK); // Reset color
}

void drawDigitalClock(int h, int m, int s) {
  // Clear the digital clock area
  tft.fillRect(10, 45, 220, 50, TFT_BLACK);
  
  char buf[9]; 
  sprintf(buf, "%02d:%02d:%02d", h, m, s);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(buf, 120, 60, 7); // Move time down more
}

void drawAnalogClock(int h, int m, int s) {
  int cx = 120, cy = 170, r = 55;  // Move down and make slightly smaller
  
  // Clear the analog clock area
  tft.fillCircle(cx, cy, r + 2, TFT_BLACK);
  
  // Draw clock face
  tft.drawCircle(cx, cy, r, TFT_WHITE);
  // Draw hour marks
  for (int i = 0; i < 12; i++) {
    float angle = (i * 30 - 90) * DEG_TO_RAD;
    int x1 = cx + cos(angle) * (r - 10);
    int y1 = cy + sin(angle) * (r - 10);
    int x2 = cx + cos(angle) * r;
    int y2 = cy + sin(angle) * r;
    tft.drawLine(x1, y1, x2, y2, TFT_WHITE);
  }
  // Calculate angles
  float s_angle = (s * 6 - 90) * DEG_TO_RAD;
  float m_angle = (m * 6 - 90) * DEG_TO_RAD;
  float h_angle = ((h % 12) * 30 + m * 0.5 - 90) * DEG_TO_RAD;
  // Draw hands
  int sx = cx + cos(s_angle) * (r - 15);
  int sy = cy + sin(s_angle) * (r - 15);
  tft.drawLine(cx, cy, sx, sy, TFT_RED);
  int mx = cx + cos(m_angle) * (r - 25);
  int my = cy + sin(m_angle) * (r - 25);
  tft.drawLine(cx, cy, mx, my, TFT_GREEN);
  int hx = cx + cos(h_angle) * (r - 40);
  int hy = cy + sin(h_angle) * (r - 40);
  tft.drawLine(cx, cy, hx, hy, TFT_BLUE);
  // Draw center
  tft.fillCircle(cx, cy, 4, TFT_WHITE);
}

bool isLeapYear(int y) {
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

int getDaysInMonth(int y, int mo) {
  int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (mo == 2 && isLeapYear(y)) return 29;
  return daysInMonth[mo - 1];
}

void setupWiFi() {
  // Start Access Point mode
  Serial.println("Starting Access Point...");
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(apIP);
  
  // Show AP info on display
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Access Point Mode", 120, 80, 4);
  tft.drawString("Network: " + String(ap_ssid), 120, 120, 2);
  tft.drawString("Password: " + String(ap_password), 120, 140, 2);
  tft.drawString("IP: " + apIP.toString(), 120, 160, 2);
  delay(3000);
}

void handleWiFiConnection() {
  // Handle async WiFi connection process
  if (wifi_connect_requested && !wifi_connecting) {
    // Start WiFi connection process
    Serial.println();
    Serial.println("🌐 === Starting Async WiFi Connection ===");
    Serial.print("SSID: '");
    Serial.print(wifi_ssid);
    Serial.print("' (length: ");
    Serial.print(wifi_ssid.length());
    Serial.println(")");
    
    // Disconnect and setup
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP_STA);
    
    // Start connection
    Serial.println("Starting WiFi connection (async)...");
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    
    wifi_connecting = true;
    wifi_connect_start = millis();
    wifi_connect_requested = false;
    Serial.println("WiFi connection initiated - monitoring in background...");
  }
  
  // Monitor ongoing connection
  if (wifi_connecting) {
    static unsigned long lastStatusCheck = 0;
    
    // Check status every 1 second
    if (millis() - lastStatusCheck >= 1000) {
      lastStatusCheck = millis();
      wl_status_t status = WiFi.status();
      
      Serial.print("WiFi Status: ");
      Serial.print(status);
      Serial.print(" (");
      Serial.print((millis() - wifi_connect_start) / 1000);
      Serial.println("s)");
      
      if (status == WL_CONNECTED) {
        wifi_connected = true;
        wifi_connecting = false;
        Serial.println("✅ WiFi connected successfully!");
        Serial.print("📍 IP address: ");
        Serial.println(WiFi.localIP());
        Serial.print("📶 Signal strength: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        
        // Sync time from NTP
        syncTimeFromNTP();
        
      } else if (millis() - wifi_connect_start >= 20000) {
        // Timeout after 20 seconds
        wifi_connecting = false;
        wifi_connected = false;
        Serial.println("❌ WiFi connection timeout!");
        Serial.print("Final status: ");
        Serial.println(status);
      }
    }
  }
}

void connectToWiFi(const char* ssid, const char* password) {
  // This function now just triggers the async connection
  wifi_ssid = String(ssid);
  wifi_password = String(password);
  wifi_connect_requested = true;
  wifi_connected = false;
  wifi_connecting = false;
  
  Serial.println("WiFi connection request queued for async processing...");
}

void syncTimeFromNTP() {
  if (!wifi_connected) {
    Serial.println("Cannot sync time: WiFi not connected");
    return;
  }
  
  Serial.println("Starting NTP time sync (async)...");
  Serial.print("Using timezone offset: ");
  Serial.print(current_gmt_offset_sec);
  Serial.println(" seconds from UTC");
  
  // Configure NTP with current timezone settings (non-blocking)
  configTime(current_gmt_offset_sec, current_daylight_offset_sec, ntp_server);
  
  // Try to get time immediately (non-blocking)
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    // Update internal clock variables
    hours = timeinfo.tm_hour;
    minutes = timeinfo.tm_min;
    seconds = timeinfo.tm_sec;
    year = timeinfo.tm_year + 1900;
    month = timeinfo.tm_mon + 1;
    day = timeinfo.tm_mday;
    
    ntp_synced = true;
    Serial.println("✅ Time synchronized from NTP to local timezone!");
    Serial.printf("Local time: %04d-%02d-%02d %02d:%02d:%02d\n", 
                 year, month, day, hours, minutes, seconds);
  } else {
    Serial.println("⏳ NTP sync initiated - time will update automatically when ready");
    ntp_synced = false;
  }
}

void setupWebServer() {
  // Serve the main page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", getHtmlPage());
  });
  
  // Handle date and time setting
  server.on("/settime", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("hours", true) && 
        request->hasParam("minutes", true) && 
        request->hasParam("seconds", true) &&
        request->hasParam("year", true) &&
        request->hasParam("month", true) &&
        request->hasParam("day", true)) {
      
      int newHours = request->getParam("hours", true)->value().toInt();
      int newMinutes = request->getParam("minutes", true)->value().toInt();
      int newSeconds = request->getParam("seconds", true)->value().toInt();
      int newYear = request->getParam("year", true)->value().toInt();
      int newMonth = request->getParam("month", true)->value().toInt();
      int newDay = request->getParam("day", true)->value().toInt();
      
      // Validate values
      if (newHours >= 0 && newHours < 24 && 
          newMinutes >= 0 && newMinutes < 60 && 
          newSeconds >= 0 && newSeconds < 60 &&
          newYear >= 2000 && newYear <= 2100 &&
          newMonth >= 1 && newMonth <= 12 &&
          newDay >= 1 && newDay <= getDaysInMonth(newYear, newMonth)) {
        
        hours = newHours;
        minutes = newMinutes;
        seconds = newSeconds;
        year = newYear;
        month = newMonth;
        day = newDay;
        
        Serial.printf("Date/Time updated to: %04d-%02d-%02d %02d:%02d:%02d\n", 
                     year, month, day, hours, minutes, seconds);
        request->send(200, "text/plain", "Date and time updated successfully!");
      } else {
        request->send(400, "text/plain", "Invalid date or time values!");
      }
    } else {
      request->send(400, "text/plain", "Missing date or time parameters!");
    }
  });
  
  // Get current date and time
  server.on("/gettime", HTTP_GET, [](AsyncWebServerRequest *request){
    String timeJson = "{\"hours\":" + String(hours) + 
                     ",\"minutes\":" + String(minutes) + 
                     ",\"seconds\":" + String(seconds) + 
                     ",\"year\":" + String(year) +
                     ",\"month\":" + String(month) +
                     ",\"day\":" + String(day) + "}";
    request->send(200, "application/json", timeJson);
  });
  
  // Handle WiFi connection setup
  server.on("/setwifi", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("=== /setwifi endpoint called ===");
    Serial.print("Number of parameters: ");
    Serial.println(request->params());
    
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      String newSSID = request->getParam("ssid", true)->value();
      String newPassword = request->getParam("password", true)->value();
      
      Serial.print("Received SSID: '");
      Serial.print(newSSID);
      Serial.println("'");
      Serial.print("Received Password: '");
      Serial.print(newPassword);
      Serial.println("'");
      
      if (newSSID.length() > 0 && newSSID.length() <= 32) {
        wifi_ssid = newSSID;
        wifi_password = newPassword;
        
        Serial.println("Starting WiFi connection process...");
        connectToWiFi(wifi_ssid.c_str(), wifi_password.c_str());
        
        // Since connection is async, always return success for valid credentials
        Serial.println("Sending connection initiated response");
        request->send(200, "text/plain", "WiFi connection initiated. Check status for connection progress.");
      } else {
        Serial.println("Invalid SSID length");
        request->send(400, "text/plain", "Invalid SSID length!");
      }
    } else {
      Serial.println("Missing parameters");
      request->send(400, "text/plain", "Missing WiFi credentials!");
    }
    Serial.println("=== End /setwifi ===");
  });
  
  // Get WiFi status
  server.on("/getstatus", HTTP_GET, [](AsyncWebServerRequest *request){
    String connectionStatus = "disconnected";
    if (wifi_connecting) {
      connectionStatus = "connecting";
    } else if (wifi_connected) {
      connectionStatus = "connected";
    }
    
    String statusJson = "{\"wifi_connected\":" + String(wifi_connected ? "true" : "false") + 
                       ",\"wifi_connecting\":" + String(wifi_connecting ? "true" : "false") +
                       ",\"wifi_ssid\":\"" + wifi_ssid + "\"" +
                       ",\"connection_status\":\"" + connectionStatus + "\"" +
                       ",\"ntp_synced\":" + String(ntp_synced ? "true" : "false") + 
                       ",\"ip_address\":\"" + (wifi_connected ? WiFi.localIP().toString() : "Not connected") + "\"" +
                       ",\"timezone_offset\":" + String(current_gmt_offset_sec) + "}";
    
    Serial.println("Status requested: " + statusJson);
    
    // Create response with CORS headers
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", statusJson);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });
  
  // Disconnect from WiFi
  server.on("/disconnectwifi", HTTP_POST, [](AsyncWebServerRequest *request){
    if (wifi_connected) {
      WiFi.disconnect();
      wifi_connected = false;
      ntp_synced = false;
      Serial.println("Disconnected from WiFi");
      request->send(200, "text/plain", "Disconnected from WiFi successfully!");
    } else {
      request->send(400, "text/plain", "Not connected to WiFi!");
    }
  });
  
  // Set timezone
  server.on("/settimezone", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("offset", true)) {
      long newOffset = request->getParam("offset", true)->value().toInt();
      
      // Validate timezone offset (between -12 and +14 hours)
      if (newOffset >= -43200 && newOffset <= 50400) {
        current_gmt_offset_sec = newOffset;
        Serial.print("Timezone updated to offset: ");
        Serial.print(newOffset);
        Serial.println(" seconds from UTC");
        
        // Re-sync time with new timezone if WiFi is connected
        if (wifi_connected) {
          syncTimeFromNTP();
        }
        
        request->send(200, "text/plain", "Timezone updated successfully!");
      } else {
        request->send(400, "text/plain", "Invalid timezone offset!");
      }
    } else {
      request->send(400, "text/plain", "Missing timezone offset!");
    }
  });
  
  server.begin();
  Serial.println("Web server started");
}

String getHtmlPage() {
  String html = "<!DOCTYPE html><html><head><title>Clock & Date Setting</title>";
  html += "<style>body{font-family:Arial;text-align:center;margin:20px;}";
  html += "h1{color:#333;}.datetime{font-size:1.5em;margin:20px 0;}";
  html += ".date{color:#0099cc;}.time{color:#333;}";
  html += "input{width:60px;font-size:16px;text-align:center;margin:3px;}";
  html += ".wifi-input{width:200px;}";
  html += "button{padding:8px 16px;margin:8px;font-size:14px;}";
  html += ".msg{margin:10px 0;padding:10px;border-radius:5px;}";
  html += ".success{background:#d4edda;color:#155724;}";
  html += ".error{background:#f8d7da;color:#721c24;}";
  html += ".warning{background:#fff3cd;color:#856404;}";
  html += ".section{margin:15px 0;padding:10px;border:1px solid #ddd;border-radius:5px;}";
  html += ".status{font-size:14px;margin:5px 0;}";
  html += ".connected{color:#28a745;}.disconnected{color:#dc3545;}";
  html += "</style></head><body>";
  html += "<h1>Multifunction Clock</h1>";
  html += "<div class='datetime'>";
  html += "<div class='date' id='date'>13-09-2025</div>";
  html += "<div class='time' id='time'>00:00:00</div></div>";
  html += "<div class='section'><strong>WiFi Status:</strong><br>";
  html += "<div class='status' id='wifiStatus'>Checking...</div>";
  html += "<div class='status' id='ntpStatus'></div>";
  html += "<div class='status' id='timezoneStatus'></div></div>";
  html += "<form id='wifiForm' class='section'>";
  html += "<strong>Connect to WiFi:</strong><br>";
  html += "Network Name (SSID): <input type='text' id='ssid' class='wifi-input' placeholder='Enter WiFi name' required><br>";
  html += "Password: <input type='password' id='wifiPass' class='wifi-input' placeholder='Enter WiFi password'><br>";
  html += "<button type='submit'>Connect to WiFi</button>";
  html += "<button type='button' onclick='disconnectWiFi()' id='disconnectBtn'>Disconnect</button>";
  html += "</form>";
  html += "<form id='timezoneForm' class='section'>";
  html += "<strong>Timezone Setting:</strong><br>";
  html += "<select id='timezone' class='wifi-input'>";
  html += "<option value='-43200'>UTC-12 (Baker Island)</option>";
  html += "<option value='-39600'>UTC-11 (Hawaii)</option>";
  html += "<option value='-36000'>UTC-10 (Alaska)</option>";
  html += "<option value='-32400'>UTC-9 (Alaska DST)</option>";
  html += "<option value='-28800'>UTC-8 (PST - Pacific)</option>";
  html += "<option value='-25200' selected>UTC-7 (PDT - Pacific DST)</option>";
  html += "<option value='-21600'>UTC-6 (CST - Central)</option>";
  html += "<option value='-18000'>UTC-5 (EST - Eastern)</option>";
  html += "<option value='-14400'>UTC-4 (EDT - Eastern DST)</option>";
  html += "<option value='0'>UTC+0 (GMT/UTC)</option>";
  html += "<option value='3600'>UTC+1 (CET - Central Europe)</option>";
  html += "<option value='7200'>UTC+2 (CEST - Central Europe DST)</option>";
  html += "<option value='28800'>UTC+8 (China/Singapore)</option>";
  html += "<option value='32400'>UTC+9 (Japan/Korea)</option>";
  html += "</select><br>";
  html += "<button type='submit'>Set Timezone</button>";
  html += "</form>";
  html += "<form id='timeForm'>";
  html += "<div class='section'><strong>Manual Date/Time Setting:</strong><br>";
  html += "Year: <input type='number' id='y' min='2000' max='2100' required>";
  html += "Month: <input type='number' id='mo' min='1' max='12' required>";
  html += "Day: <input type='number' id='d' min='1' max='31' required><br>";
  html += "Hours: <input type='number' id='h' min='0' max='23' required>";
  html += "Minutes: <input type='number' id='m' min='0' max='59' required>";
  html += "Seconds: <input type='number' id='s' min='0' max='59' required></div>";
  html += "<button type='submit'>Set Date & Time</button>";
  html += "<button type='button' onclick='loadTime()'>Refresh</button>";
  html += "<button type='button' onclick='stopAuto()' id='stopBtn'>Stop Auto-refresh</button>";
  html += "</form><div id='msg'></div>";
  html += "<script>";
  html += "let autoRefresh=true;let intervalId;";
  html += "function updateDisplay(d){";
  html += "document.getElementById('date').textContent=String(d.day).padStart(2,'0')+'-'+String(d.month).padStart(2,'0')+'-'+d.year;";
  html += "document.getElementById('time').textContent=String(d.hours).padStart(2,'0')+':'+String(d.minutes).padStart(2,'0')+':'+String(d.seconds).padStart(2,'0');}";
  html += "function updateInputs(d){const active=document.activeElement.id;";
  html += "if(active!='y'&&active!='mo'&&active!='d'&&active!='h'&&active!='m'&&active!='s'){";
  html += "document.getElementById('y').value=d.year;document.getElementById('mo').value=d.month;document.getElementById('d').value=d.day;";
  html += "document.getElementById('h').value=d.hours;document.getElementById('m').value=d.minutes;document.getElementById('s').value=d.seconds;}}";
  html += "function loadTime(){fetch('/gettime').then(r=>r.json()).then(d=>{updateDisplay(d);updateInputs(d);});}";
  html += "function loadStatus(){fetch('/getstatus').then(r=>{";
  html += "if(!r.ok)throw new Error('Status fetch failed: '+r.status);";
  html += "return r.json();";
  html += "}).then(d=>{";
  html += "console.log('Status data:',d);";
  html += "const wifiStatus=document.getElementById('wifiStatus');";
  html += "const ntpStatus=document.getElementById('ntpStatus');";
  html += "const timezoneStatus=document.getElementById('timezoneStatus');";
  html += "if(d.wifi_connected){";
  html += "wifiStatus.innerHTML='<span class=\"connected\">Connected to: '+d.wifi_ssid+'</span><br>IP: '+d.ip_address;";
  html += "ntpStatus.innerHTML=d.ntp_synced?'<span class=\"connected\">[OK] Time synced from internet</span>':'<span class=\"warning\">[!] Time sync pending</span>';";
  html += "}else if(d.wifi_connecting){";
  html += "wifiStatus.innerHTML='<span class=\"warning\">Connecting to: '+d.wifi_ssid+'...</span>';";
  html += "ntpStatus.innerHTML='<span class=\"warning\">Waiting for connection</span>';";
  html += "}else{";
  html += "wifiStatus.innerHTML='<span class=\"disconnected\">Not connected to WiFi</span>';";
  html += "ntpStatus.innerHTML='<span class=\"disconnected\">[X] No internet time sync</span>';";
  html += "}";
  html += "const offsetHours=d.timezone_offset/3600;";
  html += "const offsetStr=(offsetHours>=0?'+':'')+offsetHours;";
  html += "timezoneStatus.innerHTML='Timezone: UTC'+offsetStr;";
  html += "document.getElementById('timezone').value=d.timezone_offset;";
  html += "}).catch(e=>{";
  html += "console.error('Status error:',e);";
  html += "document.getElementById('wifiStatus').innerHTML='<span class=\"error\">Status check failed</span>';";
  html += "});}";
  html += "function stopAuto(){autoRefresh=!autoRefresh;";
  html += "document.getElementById('stopBtn').textContent=autoRefresh?'Stop Auto-refresh':'Start Auto-refresh';";
  html += "if(autoRefresh){intervalId=setInterval(()=>{loadTime();loadStatus();},5000);}else{clearInterval(intervalId);}}";
  html += "function disconnectWiFi(){fetch('/disconnectwifi',{method:'POST'}).then(r=>r.text()).then(d=>{";
  html += "const msg=document.getElementById('msg');msg.textContent=d;";
  html += "msg.className=d.includes('successfully')?'msg success':'msg error';";
  html += "setTimeout(()=>msg.textContent='',3000);loadStatus();});}";
  html += "document.getElementById('wifiForm').addEventListener('submit',function(e){";
  html += "e.preventDefault();const f=new FormData();";
  html += "f.append('ssid',document.getElementById('ssid').value);f.append('password',document.getElementById('wifiPass').value);";
  html += "const msg=document.getElementById('msg');msg.textContent='Initiating WiFi connection...';msg.className='msg warning';";
  html += "fetch('/setwifi',{method:'POST',body:f}).then(r=>r.text()).then(d=>{";
  html += "if(r.ok){";
  html += "msg.textContent='WiFi connection started. Monitor status above for progress.';msg.className='msg success';";
  html += "}else{";
  html += "msg.textContent=d;msg.className='msg error';";
  html += "}";
  html += "setTimeout(()=>msg.textContent='',8000);setTimeout(()=>{loadTime();loadStatus();},1000);});});";
  html += "document.getElementById('timeForm').addEventListener('submit',function(e){";
  html += "e.preventDefault();const f=new FormData();";
  html += "f.append('year',document.getElementById('y').value);f.append('month',document.getElementById('mo').value);f.append('day',document.getElementById('d').value);";
  html += "f.append('hours',document.getElementById('h').value);f.append('minutes',document.getElementById('m').value);f.append('seconds',document.getElementById('s').value);";
  html += "fetch('/settime',{method:'POST',body:f}).then(r=>r.text()).then(d=>{";
  html += "const msg=document.getElementById('msg');msg.textContent=d;";
  html += "msg.className=d.includes('successfully')?'msg success':'msg error';";
  html += "setTimeout(()=>msg.textContent='',3000);";
  html += "if(d.includes('successfully'))setTimeout(loadTime,500);});});";
  html += "document.getElementById('timezoneForm').addEventListener('submit',function(e){";
  html += "e.preventDefault();const f=new FormData();";
  html += "f.append('offset',document.getElementById('timezone').value);";
  html += "const msg=document.getElementById('msg');msg.textContent='Updating timezone...';msg.className='msg warning';";
  html += "fetch('/settimezone',{method:'POST',body:f}).then(r=>r.text()).then(d=>{";
  html += "msg.textContent=d;msg.className=d.includes('successfully')?'msg success':'msg error';";
  html += "setTimeout(()=>msg.textContent='',3000);setTimeout(()=>{loadTime();loadStatus();},1000);});});";
  html += "loadTime();loadStatus();intervalId=setInterval(()=>{loadTime();loadStatus();},3000);</script></body></html>";
  return html;
}