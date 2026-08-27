#ifndef DCCEXPRESSLITE_CONFIG_H
#define DCCEXPRESSLITE_CONFIG_H

// DCCExpressLite board configuration. Keep project settings here so the
// upstream CommandStation-EX sources and config.example.h remain untouched.
#define MOTOR_SHIELD_TYPE EXCSB1
#define OLED_DRIVER 128,64
#define WIFI_LED 33

// Keep the upstream ESP32 network command plumbing compiled, but make its
// initial setup a quick no-op. DCCExpressLite starts WifiESP moments later with
// credentials loaded from NVS (or with the generated setup access point).
#define ENABLE_WIFI true
#define WIFI_SSID "OFF"
#define WIFI_PASSWORD ""
#define WIFI_HOSTNAME "dccex"
#define WIFI_CHANNEL 1
#define WIFI_FORCE_AP false
#define IP_PORT 2560

// DCCExpressLite serves its UI and JSON API through HTTP port 80 and /ws.
// Keep the upstream Wi-Fi connection management, but do not allocate or open
// the additional DCC-EX/WiThrottle TCP and WebSocket server on port 2560.
// Set this to 1 only when compatibility with external DCC-EX clients is needed.
#define DCCEXPRESSLITE_ENABLE_DCCEX_PORT 0

#include "DCCExpressLite.h"

#endif
