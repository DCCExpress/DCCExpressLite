#pragma once

#include <Arduino.h>

class DCCExpressLiteSignalLogic
{
public:
  using TurnoutStateReader = int8_t (*)(uint16_t address);
  using VpinStateReader = int8_t (*)(uint16_t vpin);
  using OutputWriter = void (*)(uint16_t address, bool active, bool vpin);

  static void begin(TurnoutStateReader turnoutReader, VpinStateReader vpinReader, OutputWriter outputWriter);
  static void loop();

  static bool reload();
  static void setEnabled(bool enabled);
  static bool isEnabled();
  static bool isRunning();
  static uint8_t groupCount();

  static void notifyTurnout(uint16_t address, bool closed);
  static void notifySensor(uint16_t address, bool active);
  static void forceEvaluate();
};
