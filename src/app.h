#pragma once
#include "time_mgr.h"
#include "oled.h"
#include "wifi_mgr.h"

class App {
public:
  void setup();
  void loop();

  TimeMgr timeMgr;
  Oled oled;
  WifiMgr wifi;
};
