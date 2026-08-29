#pragma once

#include <Arduino.h>

class DCCExpressLiteRuntimeState
{
public:
  static void begin();
  static void loop();

  static bool setBlock(const char *blockId, uint16_t locoAddress);
  static bool removeBlock(const char *blockId);
  static void resetBlocks();
  static String blockStateJson();
  static uint8_t blockCount();

  static void setTurnout(uint16_t address, bool closed);
  static int8_t getTurnout(uint16_t address);

  static bool pruneBlocksFromLayout();
};
