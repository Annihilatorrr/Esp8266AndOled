#pragma once
#include <Arduino.h>
#include "ui_status.h"

class Oled {
public:
  Oled();
  ~Oled();

  void init();
  void drawStatus(const UiStatus& s);
};
