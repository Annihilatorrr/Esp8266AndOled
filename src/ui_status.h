#pragma once
#include <Arduino.h>

struct UiStatus
{
  const char* time_hms; // "HH:MM:SS"
  const char* date_ymd; // "YYYY-MM-DD"

  bool wifi_connected;
  const char* wifi_ssid; // "asusyo24"
  int wifi_rssi;         // -55 dBm (optional)
};
