#pragma once
#include "ui_status.h"
#include <Arduino.h>

class Oled
{
public:
  Oled();
  ~Oled();

  void init();
  void drawStatus(const UiStatus& s);
};
