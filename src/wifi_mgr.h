// (intentionally empty)
#pragma once
#include <Arduino.h>

class WifiMgr {
public:
  WifiMgr();
  ~WifiMgr();

  void init();
  void loop();

  String ssid() const;
  String ip() const;
  bool isConnected() const;
  int rssi() const;
};
