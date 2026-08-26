#pragma once

#include <Arduino.h>

class NetworkSettings
{
public:
  static void begin();
  static void loop();
  static bool hasStationConfig();
  static const String &ssid();
  static const String &password();
  static bool save(const String &ssid, const String &password, String &error);
  static bool clear(String &error);
  static bool handleSerialCommand(Print *stream, const char *command);

private:
  static String savedSsid;
  static String savedPassword;
  static uint32_t restartAtMs;
};
