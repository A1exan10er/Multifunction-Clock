#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

// put function declarations here:
void drawDigitalClock(int h, int m, int s);
void drawAnalogClock(int h, int m, int s);
void drawDate(int y, int mo, int d);
void updateClocks();
void setupWiFi();
void setupWebServer();
String getHtmlPage();
bool isLeapYear(int y);
int getDaysInMonth(int y, int mo);

TFT_eSPI tft = TFT_eSPI();
unsigned long lastUpdate = 0;
int hours = 12, minutes = 0, seconds = 0;
int year = 2025, month = 9, day = 13;  // Default date

// Previous values for change detection
int prevHours = -1, prevMinutes = -1, prevSeconds = -1;
int prevYear = -1, prevMonth = -1, prevDay = -1;
bool firstUpdate = true;

// Access Point credentials
const char* ap_ssid = "MultifunctionClock";
const char* ap_password = "12345678";

AsyncWebServer server(80);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Starting Multifunction Clock...");
  
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
  setupWiFi();
  setupWebServer();
  
  // Clear screen and start clock
  tft.fillScreen(TFT_BLACK);
  updateClocks();
  
  Serial.println("Setup complete");
}

void loop() {
  // put your main code here, to run repeatedly:
  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();
    seconds++;
    if (seconds >= 60) {
      seconds = 0;
      minutes++;
      if (minutes >= 60) {
        minutes = 0;
        hours++;
        if (hours >= 24) {
          hours = 0;
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
  sprintf(dateBuf, "%04d-%02d-%02d", y, mo, d);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(dateBuf, 120, 20, 4); // Date at very top
  tft.setTextColor(TFT_WHITE, TFT_BLACK); // Reset color
}

void drawDigitalClock(int h, int m, int s) {
  // Clear the digital clock area
  tft.fillRect(10, 35, 220, 50, TFT_BLACK);
  
  char buf[9]; 
  sprintf(buf, "%02d:%02d:%02d", h, m, s);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(buf, 120, 50, 7); // Move time down slightly
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
  
  server.begin();
  Serial.println("Web server started");
}

String getHtmlPage() {
  String html = "<!DOCTYPE html><html><head><title>Clock & Date Setting</title>";
  html += "<style>body{font-family:Arial;text-align:center;margin:20px;}";
  html += "h1{color:#333;}.datetime{font-size:1.5em;margin:20px 0;}";
  html += ".date{color:#0099cc;}.time{color:#333;}";
  html += "input{width:60px;font-size:16px;text-align:center;margin:3px;}";
  html += "button{padding:8px 16px;margin:8px;font-size:14px;}";
  html += ".msg{margin:10px 0;padding:10px;border-radius:5px;}";
  html += ".success{background:#d4edda;color:#155724;}";
  html += ".error{background:#f8d7da;color:#721c24;}";
  html += ".section{margin:15px 0;padding:10px;border:1px solid #ddd;border-radius:5px;}";
  html += "</style></head><body>";
  html += "<h1>Clock & Date Setting</h1>";
  html += "<div class='datetime'>";
  html += "<div class='date' id='date'>2025-09-13</div>";
  html += "<div class='time' id='time'>00:00:00</div></div>";
  html += "<form id='form'>";
  html += "<div class='section'><strong>Date:</strong><br>";
  html += "Year: <input type='number' id='y' min='2000' max='2100' required>";
  html += "Month: <input type='number' id='mo' min='1' max='12' required>";
  html += "Day: <input type='number' id='d' min='1' max='31' required></div>";
  html += "<div class='section'><strong>Time:</strong><br>";
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
  html += "document.getElementById('date').textContent=d.year+'-'+String(d.month).padStart(2,'0')+'-'+String(d.day).padStart(2,'0');";
  html += "document.getElementById('time').textContent=String(d.hours).padStart(2,'0')+':'+String(d.minutes).padStart(2,'0')+':'+String(d.seconds).padStart(2,'0');}";
  html += "function updateInputs(d){const active=document.activeElement.id;";
  html += "if(active!='y'&&active!='mo'&&active!='d'&&active!='h'&&active!='m'&&active!='s'){";
  html += "document.getElementById('y').value=d.year;document.getElementById('mo').value=d.month;document.getElementById('d').value=d.day;";
  html += "document.getElementById('h').value=d.hours;document.getElementById('m').value=d.minutes;document.getElementById('s').value=d.seconds;}}";
  html += "function loadTime(){fetch('/gettime').then(r=>r.json()).then(d=>{updateDisplay(d);updateInputs(d);});}";
  html += "function stopAuto(){autoRefresh=!autoRefresh;";
  html += "document.getElementById('stopBtn').textContent=autoRefresh?'Stop Auto-refresh':'Start Auto-refresh';";
  html += "if(autoRefresh){intervalId=setInterval(loadTime,5000);}else{clearInterval(intervalId);}}";
  html += "document.getElementById('form').addEventListener('submit',function(e){";
  html += "e.preventDefault();const f=new FormData();";
  html += "f.append('year',document.getElementById('y').value);f.append('month',document.getElementById('mo').value);f.append('day',document.getElementById('d').value);";
  html += "f.append('hours',document.getElementById('h').value);f.append('minutes',document.getElementById('m').value);f.append('seconds',document.getElementById('s').value);";
  html += "fetch('/settime',{method:'POST',body:f}).then(r=>r.text()).then(d=>{";
  html += "const msg=document.getElementById('msg');msg.textContent=d;";
  html += "msg.className=d.includes('successfully')?'msg success':'msg error';";
  html += "setTimeout(()=>msg.textContent='',3000);";
  html += "if(d.includes('successfully'))setTimeout(loadTime,500);});});";
  html += "loadTime();intervalId=setInterval(loadTime,5000);</script></body></html>";
  return html;
}