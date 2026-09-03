#pragma once

#include <Arduino.h>

class DCCExpressLiteSignalLogic
{
public:
  using TurnoutStateReader = int8_t (*)(uint16_t address);
  using VpinStateReader = int8_t (*)(uint16_t vpin);
  using OutputWriter = void (*)(uint16_t address, bool active, bool vpin);

  static void begin(
    TurnoutStateReader turnoutReader,
    VpinStateReader vpinReader,
    OutputWriter outputWriter);

  static void loop();
  static bool reload();
  static void setEnabled(bool enabled);
  static bool isEnabled();
  static bool isRunning();
  static uint8_t groupCount();

  // Kept address-based at the HTTP/DCC boundary. The engine itself resolves
  // every condition through the stable numeric layout ID registry.
  static void notifyTurnout(uint16_t address, bool physicalValue);
  static void notifySensor(uint16_t address, bool active);
  static void forceEvaluate();
};
