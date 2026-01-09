#include "time_mgr.h"
#include "wifi_mgr.h"
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <time.h>

static const char* kTimeApiUrl = "https://worldtimeapi.org/api/ip";
// JSON key we look for and compile-time lengths to avoid magic numbers
static constexpr const char kDateTimeKey[] = "\"datetime\":\""; // "datetime":"
static constexpr size_t kDateTimeKeyLen = sizeof(kDateTimeKey) - 1;
static constexpr int kDateTimeTotalLen = 19; // YYYY-MM-DDTHH:MM:SS
static constexpr int kDateLen = 10;          // YYYY-MM-DD
static constexpr int kTimeStart =
    11; // index where HH:MM:SS starts inside the datetime string
static constexpr int kTimeLen = 8; // HH:MM:SS
static constexpr int kDateTimeTPos =
    10; // position of 'T' inside the datetime string
static constexpr unsigned long kFetchIntervalMs =
    60UL * 60UL * 1000UL; // refresh once per hour
static constexpr const char* kNtpServer = "pool.ntp.org";
static constexpr uint16_t kNtpPort = 123;
static WiFiUDP sntpUdp;

// Internal single instance for legacy/free-function callers
static TimeMgr _internalTimeMgr;

TimeMgr::TimeMgr()
    : synced_(false), lastFetchMs_(0), lastTime_("--:--:--"),
      lastDate_("----------")
{
}

TimeMgr::~TimeMgr() {}

void TimeMgr::init()
{
    synced_ = false;
    lastFetchMs_ = 0;
    lastTime_ = "--:--:--";
    lastDate_ = "----------";
    lastEpochLocal_ = 0;
}

void TimeMgr::update()
{
    // Only try when WiFi is up
    if (WiFi.status() != WL_CONNECTED)
        return;

    const unsigned long now = millis();
    // Only attempt a fetch if not synced yet or refresh interval passed
    if (synced_ && (int32_t)(now - lastFetchMs_) < 0)
        return;
    if (synced_ && (now - lastFetchMs_ < kFetchIntervalMs))
        return;
    // throttle rapid retries to once every 5 seconds
    if (!synced_ && (now - lastFetchMs_ < 5000))
        return;
    lastFetchMs_ = now;

    // Use a secure client for HTTPS; accept any cert for simplicity (not for
    // production)
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    if (!http.begin(client, kTimeApiUrl))
    {
        Serial.println("[Time] http.begin() failed");
        return;
    }
    Serial.printf("[Time] Fetching %s\n", kTimeApiUrl);

    int code = http.GET();
    Serial.printf("[Time] HTTP code=%d\n", code);
    if (code == HTTP_CODE_OK)
    {
        String payload = http.getString();
        // print a truncated preview of the payload for debugging
        String preview = payload;
        if (preview.length() > 200)
            preview = preview.substring(0, 200) + "...";
        Serial.printf("[Time] payload(len=%u): %s\n",
                      (unsigned)payload.length(), preview.c_str());

        // Try to parse unixtime (preferred) and utc_offset
        long unixtime = 0;
        int p = payload.indexOf("\"unixtime\":");
        if (p >= 0)
        {
            p += 11; // move past "unixtime":
            // parse digits
            int q = p;
            while (q < payload.length() && isDigit(payload.charAt(q)))
                q++;
            if (q > p)
            {
                String num = payload.substring(p, q);
                unixtime = num.toInt();
                Serial.printf("[Time] parsed unixtime=%ld\n", unixtime);
            }
        }

        // parse utc_offset like "+01:00"
        int offsetSeconds = 0;
        p = payload.indexOf("\"utc_offset\":\"");
        if (p >= 0)
        {
            p += 14; // move past "utc_offset":"
            if (p + 6 <= payload.length())
            {
                String off = payload.substring(p, p + 6); // e.g. +01:00
                char sign = off.charAt(0);
                int hh = (off.charAt(1) - '0') * 10 + (off.charAt(2) - '0');
                int mm = (off.charAt(4) - '0') * 10 + (off.charAt(5) - '0');
                offsetSeconds = hh * 3600 + mm * 60;
                if (sign == '-')
                    offsetSeconds = -offsetSeconds;
                Serial.printf("[Time] parsed utc_offset=%s -> %d seconds\n",
                              off.c_str(), offsetSeconds);
            }
        }

        if (unixtime > 0)
        {
            lastEpochLocal_ =
                (unsigned long)unixtime + (unsigned long)offsetSeconds;
            lastFetchMs_ = now;
            Serial.printf(
                "[Time] unixtime=%ld offset=%d -> lastEpochLocal=%lu\n",
                unixtime, offsetSeconds, lastEpochLocal_);
            // initialize lastDate_/lastTime_ from epoch
            time_t t = (time_t)lastEpochLocal_;
            struct tm* tm = gmtime(&t);
            if (tm)
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tm->tm_year + 1900,
                         tm->tm_mon + 1, tm->tm_mday);
                lastDate_ = String(buf);
                snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm->tm_hour,
                         tm->tm_min, tm->tm_sec);
                lastTime_ = String(buf);
                synced_ = true;
                Serial.printf("[Time] initial date=%s time=%s\n",
                              lastDate_.c_str(), lastTime_.c_str());
            }
        }
        // fallback: try the datetime key parsing (legacy)
        else
        {
            int q = payload.indexOf(kDateTimeKey);
            if (q >= 0)
            {
                q += (int)kDateTimeKeyLen;
                if (q + kDateTimeTotalLen <= payload.length())
                {
                    String dt = payload.substring(q, q + kDateTimeTotalLen);
                    if (dt.length() >= kDateTimeTotalLen &&
                        dt.charAt(kDateTimeTPos) == 'T')
                    {
                        // parse components YYYY-MM-DDTHH:MM:SS
                        int y = dt.substring(0, 4).toInt();
                        int mo = dt.substring(5, 7).toInt();
                        int d = dt.substring(8, 10).toInt();
                        int hh = dt.substring(11, 13).toInt();
                        int mm = dt.substring(14, 16).toInt();
                        int ss = dt.substring(17, 19).toInt();

                        // compute days since epoch using civil_from_days
                        // algorithm
                        auto days_from_civil = [](int y, unsigned m,
                                                  unsigned d) -> int64_t
                        {
                            y -= m <= 2;
                            const int64_t era = (y >= 0 ? y : y - 399) / 400;
                            const unsigned yoe = (unsigned)(y - era * 400);
                            const unsigned doy =
                                (153 * (m + (m > 2 ? -3u : 9u)) + 2) / 5 + d -
                                1;
                            const unsigned doe =
                                yoe * 365 + yoe / 4 - yoe / 100 + doy;
                            return era * 146097 + (int64_t)doe - 719468;
                        };

                        int64_t days =
                            days_from_civil(y, (unsigned)mo, (unsigned)d);
                        unsigned long epoch =
                            (unsigned long)(days * 86400LL + hh * 3600 +
                                            mm * 60 + ss);
                        // apply utc_offset if available (offsetSeconds parsed
                        // earlier)
                        epoch -= (unsigned long)offsetSeconds;

                        lastEpochLocal_ = epoch;
                        lastDate_ = dt.substring(0, kDateLen);
                        lastTime_ =
                            dt.substring(kTimeStart, kTimeStart + kTimeLen);
                        synced_ = true;
                        lastFetchMs_ = now;
                        Serial.printf("[Time] fallback datetime=%s -> "
                                      "epoch=%lu (offsetApplied=%d)\n",
                                      dt.c_str(), epoch, offsetSeconds);
                    }
                }
            }
        }
    }
    http.end();

    // If still not synced, try SNTP as fallback and log status
    if (!synced_)
    {
        Serial.println("[Time] attempting SNTP fallback...");
        // prepare NTP packet (48 bytes)
        uint8_t packet[48];
        memset(packet, 0, sizeof(packet));
        packet[0] = 0x1B; // LI=0, VN=3, Mode=3 (client)

        sntpUdp.begin(0);
        sntpUdp.beginPacket(kNtpServer, kNtpPort);
        sntpUdp.write(packet, sizeof(packet));
        int res = sntpUdp.endPacket();
        Serial.printf("[Time][NTP] send result=%d\n", res);

        unsigned long start = millis();
        while (millis() - start < 1500)
        {
            int len = sntpUdp.parsePacket();
            if (len >= 48)
            {
                Serial.printf("[Time][NTP] response_len=%d\n", len);
                uint8_t buf[48];
                sntpUdp.read(buf, 48);
                unsigned long sec = ((unsigned long)buf[40] << 24) |
                                    ((unsigned long)buf[41] << 16) |
                                    ((unsigned long)buf[42] << 8) |
                                    ((unsigned long)buf[43]);
                const unsigned long seventyYears = 2208988800UL;
                if (sec > seventyYears)
                {
                    unsigned long unix = sec - seventyYears;
                    lastEpochLocal_ = unix;
                    lastFetchMs_ = millis();
                    time_t t = (time_t)lastEpochLocal_;
                    struct tm* tm = gmtime(&t);
                    if (tm)
                    {
                        char buf2[32];
                        snprintf(buf2, sizeof(buf2), "%04d-%02d-%02d",
                                 tm->tm_year + 1900, tm->tm_mon + 1,
                                 tm->tm_mday);
                        lastDate_ = String(buf2);
                        snprintf(buf2, sizeof(buf2), "%02d:%02d:%02d",
                                 tm->tm_hour, tm->tm_min, tm->tm_sec);
                        lastTime_ = String(buf2);
                    }
                    synced_ = true;
                    Serial.printf("[Time][NTP] OK unix=%lu -> %s %s\n", unix,
                                  lastDate_.c_str(), lastTime_.c_str());
                }
                else
                {
                    Serial.printf("[Time][NTP] invalid ntp seconds=%lu\n", sec);
                }
                sntpUdp.stop();
                break;
            }
            delay(10);
        }
        sntpUdp.stop();
    }
}

bool TimeMgr::isSynced() const { return synced_; }
String TimeMgr::timeString() const
{
    if (!synced_ || lastEpochLocal_ == 0)
        return lastTime_;
    unsigned long now = millis();
    unsigned long cur = lastEpochLocal_ + ((now - lastFetchMs_) / 1000UL);
    time_t t = (time_t)cur;
    struct tm* tm = gmtime(&t);
    if (!tm)
        return lastTime_;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm->tm_hour, tm->tm_min,
             tm->tm_sec);
    return String(buf);
}

String TimeMgr::dateString() const
{
    if (!synced_ || lastEpochLocal_ == 0)
        return lastDate_;
    unsigned long now = millis();
    unsigned long cur = lastEpochLocal_ + ((now - lastFetchMs_) / 1000UL);
    time_t t = (time_t)cur;
    struct tm* tm = gmtime(&t);
    if (!tm)
        return lastDate_;
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tm->tm_year + 1900,
             tm->tm_mon + 1, tm->tm_mday);
    return String(buf);
}

// Legacy API implementations (forwarders)
void timeInit() { _internalTimeMgr.init(); }
void timeLoop() { _internalTimeMgr.update(); }
bool timeIsSynced() { return _internalTimeMgr.isSynced(); }
String timeString() { return _internalTimeMgr.timeString(); }
String dateString() { return _internalTimeMgr.dateString(); }
