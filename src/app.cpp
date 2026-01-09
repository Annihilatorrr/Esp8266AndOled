#include "app.h"
#include <Arduino.h>
#include <string.h> // <--- added to fix memcpy/strcpy compilation errors
#include "wifi_mgr.h"
#include "time_mgr.h"
#include "oled.h"
#include "timer.h"

// Use Timer to trigger UI refresh every second

// Buffers live in App translation unit, stable pointers for OLED draw
static char g_time[16];
static char g_date[16];
static char g_ssid[33];
static char g_ip[20];

static void copyToBuf(char *dst, size_t cap, const String &s)
{
  if (cap == 0)
    return;
  size_t n = s.length();
  if (n >= cap)
    n = cap - 1;
  memcpy(dst, s.c_str(), n);
  dst[n] = '\0';
}

void App::setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("Boot OK");

  // use OOP-style init
  oled.init();
  wifi.init();
  timeMgr.init();

  // Init placeholders
  strcpy(g_time, "--:--:--");
  strcpy(g_date, "----------");
  strcpy(g_ssid, "asusyo24");
  strcpy(g_ip, "-");

  // start 1s UI timer
  timerBegin(1000);
}

void App::loop()
{
  wifi.loop();
  timeMgr.update();

  timerUpdate();
  if (!timerExpired())
    return;
  timerReset();

  // Format strings (outside OLED)
  copyToBuf(g_time, sizeof(g_time), timeMgr.timeString());
  copyToBuf(g_date, sizeof(g_date), timeMgr.dateString());
  copyToBuf(g_ssid, sizeof(g_ssid), wifi.ssid());
  copyToBuf(g_ip, sizeof(g_ip), wifi.ip());

  UiStatus s;
  s.time_hms = g_time;
  s.date_ymd = g_date;
  s.wifi_connected = wifi.isConnected();
  s.wifi_ssid = g_ssid;
  s.wifi_rssi = wifi.rssi();

  // use OOP-style draw
  oled.drawStatus(s);
}