#include "timer.h"

// Minimal timer implementation
Timer::Timer() : _interval(0), _last(0), _expired(false) {}
Timer::~Timer() {}

void Timer::begin(unsigned long intervalMs) {
  _interval = intervalMs;
  _last = millis();
  _expired = false;
}

void Timer::update() {
  if (_interval == 0) return;
  unsigned long now = millis();
  if (! _expired && (now - _last >= _interval)) {
    _expired = true;
  }
}

bool Timer::expired() const { return _expired; }

void Timer::reset() {
  _last = millis();
  _expired = false;
}

// Internal instance for legacy wrappers
static Timer _internalTimer;

// Legacy API forwarders
void timerBegin(unsigned long intervalMs) { _internalTimer.begin(intervalMs); }
void timerUpdate() { _internalTimer.update(); }
bool timerExpired() { return _internalTimer.expired(); }
void timerReset() { _internalTimer.reset(); }