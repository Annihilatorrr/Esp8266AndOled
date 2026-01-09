#include "oled.h"
#include <U8g2lib.h>

// Your wiring:
static constexpr uint8_t PIN_SDA = D5; // GPIO14
static constexpr uint8_t PIN_SCL = D6; // GPIO12

static U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(
    U8G2_R0, PIN_SCL, PIN_SDA, U8X8_PIN_NONE);

// If blank but I2C is fine, try SH1106 instead:
// static U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, PIN_SCL, PIN_SDA, U8X8_PIN_NONE);

Oled::Oled() {}
Oled::~Oled() {}

void Oled::init()
{

  u8g2.begin();
  u8g2.setFontMode(1);
  u8g2.setDrawColor(1);
}

void Oled::drawStatus(const UiStatus& s)
{
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tf);

  // Line 1: date + time
  char dt[32];
  snprintf(dt, sizeof(dt), "%s %s",
           s.date_ymd ? s.date_ymd : "----------",
           s.time_hms ? s.time_hms : "--:--:--");

  u8g2.drawStr(0, 14, dt);

  // Line 2: WiFi status (bottom)
  char wline[64];
  if (s.wifi_connected)
  {
    snprintf(wline, sizeof(wline), "WiFi:%s %ddBm",
             s.wifi_ssid ? s.wifi_ssid : "-",
             s.wifi_rssi);
  }
  else
  {
    snprintf(wline, sizeof(wline), "WiFi:%s (conn...)",
             s.wifi_ssid ? s.wifi_ssid : "-");
  }

  u8g2.drawStr(0, 30, wline);

  u8g2.sendBuffer();
}

// Internal instance used by legacy wrappers
static Oled _internalOled;

// Legacy API forwarders
void oledInit() { _internalOled.init(); }
void oledDrawStatus(const UiStatus& s) { _internalOled.drawStatus(s); }
