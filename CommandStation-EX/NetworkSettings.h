#pragma once

#include <Arduino.h>

class NetworkSettings
{
public:
  static void begin();
  static bool hasStationConfig();
  static const String &ssid();
  static const String &password();
  static bool save(const String &ssid, const String &password, String &error);
  static bool clear(String &error);

private:
  static String savedSsid;
  static String savedPassword;
};
