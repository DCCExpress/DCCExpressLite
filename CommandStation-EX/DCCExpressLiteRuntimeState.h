#pragma once

#include <Arduino.h>

class DCCExpressLiteRuntimeState
{
public:
  static void begin();
  static void loop();

  static bool setBlock(uint16_t blockId, uint16_t locoAddress);
  static bool removeBlock(uint16_t blockId);
  static void resetBlocks();
  static String blockStateJson();
  static uint8_t blockCount();

  // Address-facing compatibility boundary. Internally the state is keyed by
  // stable (layoutId, channel) and the current address is resolved on demand.
  static void setTurnout(uint16_t address, bool closed);
  static int8_t getTurnout(uint16_t address);
  static uint8_t turnoutCount();
  static bool getTurnoutAt(uint8_t index, uint16_t &address, bool &closed);

  // Reloads the layout registry and removes references to deleted elements.
  static bool pruneBlocksFromLayout();
};
