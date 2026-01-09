#pragma once
#include <Arduino.h>

class TimeMgr {
public:
	TimeMgr();
	~TimeMgr();

	void init();
	void update();

	bool isSynced() const;
	String timeString() const;   // "HH:MM:SS"
	String dateString() const;   // "YYYY-MM-DD"
private:
    bool synced_;
    unsigned long lastFetchMs_;
    String lastTime_;
    String lastDate_;
	unsigned long lastEpochLocal_; // seconds since epoch adjusted to local timezone
};

// Compatibility wrappers (legacy API)
void timeInit();
void timeLoop();

bool timeIsSynced();
String timeString();   // "HH:MM:SS"
String dateString();   // "YYYY-MM-DD"
