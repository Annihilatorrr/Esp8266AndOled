// (intentionally empty)
#include "wifi_mgr.h"
#include <ESP8266WiFi.h>

static const char* kSsid = "asusyo24";
static const char* kPass = "cheche452";

namespace {
// Backoff reconnect (prevents auth/handshake storms)
static bool installedHandlers = false;
static uint32_t nextTryMs = 0;
static uint32_t backoffMs = 3000;   // start 3s
static const uint32_t kBackoffMax = 60000;

static void installWifiHandlersOnce() {
  if (installedHandlers) return;
  installedHandlers = true;

  WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& e){
    Serial.printf("[WiFi] GOT IP: %s\n", e.ip.toString().c_str());
  });

  WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& e){
    Serial.printf("[WiFi] DISCONNECTED reason=%d\n", (int)e.reason);
  });

  WiFi.onStationModeConnected([](const WiFiEventStationModeConnected& e){
    Serial.printf("[WiFi] CONNECTED to '%s' CH=%d\n", e.ssid.c_str(), e.channel);
  });
}

static void startConnect() {
  Serial.printf("[WiFi] Connecting to '%s'...\n", kSsid);

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setOutputPower(20.5f);

  // Soft reset of state (NOT erase):
  WiFi.disconnect(false);
  delay(80);

  WiFi.begin(kSsid, kPass);
}

} // namespace

WifiMgr::WifiMgr() {}
WifiMgr::~WifiMgr() {}

void WifiMgr::init() {
  installWifiHandlersOnce();
  startConnect();
  nextTryMs = millis() + backoffMs;
}

void WifiMgr::loop() {
  if (WiFi.status() == WL_CONNECTED) {
    backoffMs = 3000;
    nextTryMs = millis() + backoffMs;
    return;
  }

  const uint32_t now = millis();
  if ((int32_t)(now - nextTryMs) >= 0) {
    Serial.printf("[WiFi] retry (backoff=%u ms)\n", backoffMs);
    startConnect();

    backoffMs = (backoffMs < kBackoffMax) ? (backoffMs * 2) : kBackoffMax;
    nextTryMs = now + backoffMs;
  }
}

String WifiMgr::ssid() const { return (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : String(kSsid); }
String WifiMgr::ip() const { return (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : String("-"); }
bool WifiMgr::isConnected() const { return WiFi.status() == WL_CONNECTED; }
int WifiMgr::rssi() const { return (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0; }
