#pragma once
#include <Arduino.h>

class Timer
{
public:
  Timer();
  ~Timer();

  void begin(unsigned long intervalMs = 0);
  void update();
  bool expired() const;
  void reset();

private:
  unsigned long _interval;
  unsigned long _last;
  bool _expired;
};

// Legacy/free-function compatibility
void timerBegin(unsigned long intervalMs);
void timerUpdate();
bool timerExpired();
void timerReset();
