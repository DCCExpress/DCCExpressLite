#include "DCCExpressLiteRuntimeState.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

namespace
{
constexpr const char *STATE_PATH = "/runtime-state.json";
constexpr const char *TEMP_PATH = "/runtime-state.tmp";
constexpr const char *BACKUP_PATH = "/runtime-state.json.bak";
constexpr const char *LAYOUT_PATH = "/layout.json";
constexpr uint8_t MAX_BLOCK_STATES = 48;
constexpr uint8_t MAX_TURNOUT_STATES = 128;
constexpr uint32_t SAVE_DELAY_MS = 750;

struct BlockState
{
  char blockId[48] = {};
  uint16_t locoAddress = 0;
};

struct TurnoutState
{
  uint16_t address = 0;
  bool closed = false;
};

BlockState blocks[MAX_BLOCK_STATES];
TurnoutState turnouts[MAX_TURNOUT_STATES];
uint8_t loadedBlockCount = 0;
uint8_t loadedTurnoutCount = 0;
bool savePending = false;
uint32_t changedAtMs = 0;

String escapeJsonText(const char *input)
{
  String result;
  if (!input) return result;
  for (const char *cursor = input; *cursor; ++cursor)
  {
    if (*cursor == '\\' || *cursor == '"') result += '\\';
    result += *cursor;
  }
  return result;
}

void markChanged()
{
  savePending = true;
  changedAtMs = millis();
}

int findBlock(const char *blockId)
{
  if (!blockId || !blockId[0]) return -1;
  for (uint8_t i = 0; i < loadedBlockCount; ++i)
    if (!strcmp(blocks[i].blockId, blockId)) return i;
  return -1;
}

void removeBlockAt(uint8_t index)
{
  if (index >= loadedBlockCount) return;
  for (uint8_t i = index + 1; i < loadedBlockCount; ++i)
    blocks[i - 1] = blocks[i];
  blocks[--loadedBlockCount] = BlockState{};
}

int findTurnout(uint16_t address)
{
  for (uint8_t i = 0; i < loadedTurnoutCount; ++i)
    if (turnouts[i].address == address) return i;
  return -1;
}

bool saveState()
{
  LittleFS.remove(TEMP_PATH);
  File file = LittleFS.open(TEMP_PATH, "w");
  if (!file) return false;

  file.print(F("{\"version\":1,\"blocks\":["));
  for (uint8_t i = 0; i < loadedBlockCount; ++i)
  {
    if (i) file.print(',');
    file.print(F("{\"blockId\":\""));
    file.print(escapeJsonText(blocks[i].blockId));
    file.print(F("\",\"locoAddress\":"));
    file.print(blocks[i].locoAddress);
    file.print('}');
  }
  file.print(F("],\"turnouts\":["));
  for (uint8_t i = 0; i < loadedTurnoutCount; ++i)
  {
    if (i) file.print(',');
    file.print(F("{\"address\":"));
    file.print(turnouts[i].address);
    file.print(F(",\"closed\":"));
    file.print(turnouts[i].closed ? F("true") : F("false"));
    file.print('}');
  }
  file.print(F("]}"));
  file.flush();
  const bool ok = file.getWriteError() == 0;
  file.close();
  if (!ok)
  {
    LittleFS.remove(TEMP_PATH);
    return false;
  }

  LittleFS.remove(BACKUP_PATH);
  if (LittleFS.exists(STATE_PATH) && !LittleFS.rename(STATE_PATH, BACKUP_PATH))
  {
    LittleFS.remove(TEMP_PATH);
    return false;
  }
  if (LittleFS.rename(TEMP_PATH, STATE_PATH)) return true;
  if (LittleFS.exists(BACKUP_PATH)) LittleFS.rename(BACKUP_PATH, STATE_PATH);
  LittleFS.remove(TEMP_PATH);
  return false;
}

void loadState()
{
  for (BlockState &state : blocks) state = BlockState{};
  for (TurnoutState &state : turnouts) state = TurnoutState{};
  loadedBlockCount = 0;
  loadedTurnoutCount = 0;

  File file = LittleFS.open(STATE_PATH, "r");
  if (!file) return;
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, file);
  file.close();
  if (error)
  {
    Serial.printf("Runtime state: invalid JSON: %s\n", error.c_str());
    return;
  }

  for (JsonObjectConst source : document["blocks"].as<JsonArrayConst>())
  {
    if (loadedBlockCount >= MAX_BLOCK_STATES) break;
    const char *blockId = source["blockId"] | "";
    const int locoAddress = source["locoAddress"] | 0;
    if (!blockId[0] || locoAddress < 1 || locoAddress > 10239) continue;
    BlockState &target = blocks[loadedBlockCount++];
    strlcpy(target.blockId, blockId, sizeof(target.blockId));
    target.locoAddress = static_cast<uint16_t>(locoAddress);
  }

  for (JsonObjectConst source : document["turnouts"].as<JsonArrayConst>())
  {
    if (loadedTurnoutCount >= MAX_TURNOUT_STATES) break;
    const int address = source["address"] | 0;
    if (address < 1 || address > 2048) continue;
    TurnoutState &target = turnouts[loadedTurnoutCount++];
    target.address = static_cast<uint16_t>(address);
    target.closed = source["closed"] | false;
  }
  Serial.printf("Runtime state: loaded %u block(s), %u turnout(s).\n",
    loadedBlockCount, loadedTurnoutCount);
}

bool layoutContainsBlock(JsonDocument &layout, const char *blockId)
{
  for (JsonObjectConst layer : layout["layers"].as<JsonArrayConst>())
    for (JsonObjectConst element : layer["elements"].as<JsonArrayConst>())
      if (!strcmp(element["type"] | "", "trackblock") &&
          !strcmp(element["id"] | "", blockId)) return true;
  return false;
}
}

void DCCExpressLiteRuntimeState::begin()
{
  if (!LittleFS.exists(STATE_PATH))
  {
    File file = LittleFS.open(STATE_PATH, "w");
    if (file)
    {
      file.print(F("{\"version\":1,\"blocks\":[],\"turnouts\":[]}"));
      file.close();
    }
  }
  loadState();
  pruneBlocksFromLayout();
}

void DCCExpressLiteRuntimeState::loop()
{
  if (!savePending || millis() - changedAtMs < SAVE_DELAY_MS) return;
  if (saveState()) savePending = false;
  else changedAtMs = millis();
}

bool DCCExpressLiteRuntimeState::setBlock(const char *blockId, uint16_t locoAddress)
{
  if (!blockId || !blockId[0] || strlen(blockId) >= sizeof(BlockState::blockId) ||
      locoAddress < 1 || locoAddress > 10239) return false;

  for (int i = loadedBlockCount - 1; i >= 0; --i)
  {
    if (!strcmp(blocks[i].blockId, blockId) || blocks[i].locoAddress == locoAddress)
    {
      removeBlockAt(static_cast<uint8_t>(i));
    }
  }
  if (loadedBlockCount >= MAX_BLOCK_STATES) return false;
  BlockState &target = blocks[loadedBlockCount++];
  strlcpy(target.blockId, blockId, sizeof(target.blockId));
  target.locoAddress = locoAddress;
  markChanged();
  return true;
}

bool DCCExpressLiteRuntimeState::removeBlock(const char *blockId)
{
  const int index = findBlock(blockId);
  if (index < 0) return true;
  removeBlockAt(static_cast<uint8_t>(index));
  markChanged();
  return true;
}

void DCCExpressLiteRuntimeState::resetBlocks()
{
  if (!loadedBlockCount) return;
  for (BlockState &state : blocks) state = BlockState{};
  loadedBlockCount = 0;
  markChanged();
}

String DCCExpressLiteRuntimeState::blockStateJson()
{
  String json = "{";
  for (uint8_t i = 0; i < loadedBlockCount; ++i)
  {
    if (i) json += ',';
    const String id = escapeJsonText(blocks[i].blockId);
    json += "\"" + id + "\":{\"blockId\":\"" + id +
      "\",\"locoId\":null,\"locoAddress\":" + String(blocks[i].locoAddress) + "}";
  }
  json += '}';
  return json;
}

uint8_t DCCExpressLiteRuntimeState::blockCount()
{
  return loadedBlockCount;
}

void DCCExpressLiteRuntimeState::setTurnout(uint16_t address, bool closed)
{
  if (address < 1 || address > 2048) return;
  const int index = findTurnout(address);
  if (index >= 0)
  {
    if (turnouts[index].closed == closed) return;
    turnouts[index].closed = closed;
    markChanged();
    return;
  }
  if (loadedTurnoutCount >= MAX_TURNOUT_STATES) return;
  TurnoutState &target = turnouts[loadedTurnoutCount++];
  target.address = address;
  target.closed = closed;
  markChanged();
}

int8_t DCCExpressLiteRuntimeState::getTurnout(uint16_t address)
{
  const int index = findTurnout(address);
  return index >= 0 ? (turnouts[index].closed ? 1 : 0) : -1;
}

bool DCCExpressLiteRuntimeState::pruneBlocksFromLayout()
{
  File file = LittleFS.open(LAYOUT_PATH, "r");
  if (!file) return false;
  JsonDocument layout;
  const DeserializationError error = deserializeJson(layout, file);
  file.close();
  if (error) return false;

  bool changed = false;
  for (int i = loadedBlockCount - 1; i >= 0; --i)
  {
    if (layoutContainsBlock(layout, blocks[i].blockId)) continue;
    removeBlockAt(static_cast<uint8_t>(i));
    changed = true;
  }
  if (changed) markChanged();
  return true;
}
