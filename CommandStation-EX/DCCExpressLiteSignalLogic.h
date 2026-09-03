#pragma once

#include <Arduino.h>

class DCCExpressLiteSignalLogic
{
public:
  using TurnoutStateReader = int8_t (*)(uint16_t address);
  using OutputWriter = void (*)(uint16_t address, bool active);
  using AspectWriter = void (*)(uint16_t address, uint8_t aspect);

  static void begin(
    TurnoutStateReader turnoutReader,
    OutputWriter outputWriter,
    AspectWriter aspectWriter);

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
