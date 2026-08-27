#pragma once

#include <Arduino.h>

class DCCExpressLite
{
public:
  static void begin();
  static void loop();
  static void appendDeviceJson(String &json);
  static int getMainCurrentmA();
  static int getProgCurrentmA();

private:
  static bool started;
};
