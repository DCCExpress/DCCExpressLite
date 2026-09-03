#pragma once

#include <Arduino.h>

class DCCExpressLiteLayoutRegistry
{
public:
  enum class OutputMode : uint8_t
  {
    Accessory,
    Vpin,
  };

  struct TurnoutEndpoint
  {
    uint16_t id = 0;
    uint8_t channel = 0;
    uint16_t address = 0;
    OutputMode mode = OutputMode::Accessory;
    bool closedValue = true;
  };

  struct BasicAccessoryEndpoint
  {
    uint16_t id = 0;
    uint8_t channel = 0;
    uint16_t address = 0;
  };

  struct SensorEndpoint
  {
    uint16_t id = 0;
    uint16_t address = 0;
  };

  struct SignalEndpoint
  {
    uint16_t id = 0;
    uint16_t address = 0;
    bool extended = false;
    uint8_t outputCount = 1;
  };

  static bool begin();
  static bool reload();
  static bool ready();

  static bool getTurnout(uint16_t id, uint8_t channel, TurnoutEndpoint &out);
  static bool findTurnoutByAddress(uint16_t address, TurnoutEndpoint &out);
  static bool findTurnoutByAddress(
    uint16_t address, OutputMode mode, TurnoutEndpoint &out);

  static bool getBasicAccessory(uint16_t id, uint8_t channel, BasicAccessoryEndpoint &out);
  static bool findBasicAccessoryByAddress(uint16_t address, BasicAccessoryEndpoint &out);

  static bool getSensor(uint16_t id, SensorEndpoint &out);
  static bool findSensorByAddress(uint16_t address, SensorEndpoint &out);

  static bool getSignal(uint16_t id, SignalEndpoint &out);
  static bool findSignalByAddress(
    uint16_t address, bool extended, SignalEndpoint &out);

  static bool containsBlock(uint16_t id);
  static bool resolveLegacyId(const char *legacyId, uint16_t &id);
  static bool normalizeBlockId(const char *value, uint16_t &id);

  static uint16_t elementCount();
  static uint16_t turnoutCount();
  static uint16_t basicAccessoryCount();
  static uint16_t sensorCount();
  static uint16_t signalCount();
};
