#include "DCCExpressLiteRuntimeState.h"
#include "DCCExpressLiteLayoutRegistry.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <stdlib.h>

namespace
{
constexpr const char *STATE_PATH = "/runtime-state.json";
constexpr const char *TEMP_PATH = "/runtime-state.tmp";
constexpr const char *BACKUP_PATH = "/runtime-state.json.bak";
constexpr uint8_t MAX_BLOCK_STATES = 48;
constexpr uint8_t MAX_TURNOUT_STATES = 128;
constexpr uint32_t SAVE_DELAY_MS = 750;
constexpr uint8_t STATE_VERSION = 2;

struct BlockState
{
  uint16_t id = 0;
  uint16_t locoAddress = 0;
};

struct TurnoutState
{
  uint16_t id = 0;
  uint8_t channel = 0;
  bool closed = false;
};

BlockState *blocks = nullptr;
TurnoutState *turnouts = nullptr;
uint8_t loadedBlockCount = 0;
uint8_t loadedTurnoutCount = 0;
uint8_t blockCapacity = 0;
uint8_t turnoutCapacity = 0;
bool savePending = false;
uint32_t changedAtMs = 0;

bool ensureBlockCapacity(uint8_t required)
{
  if (required <= blockCapacity) return true;
  if (required > MAX_BLOCK_STATES) return false;
  uint8_t next = blockCapacity ? blockCapacity : 4;
  while (next < required)
  {
    const uint16_t doubled = static_cast<uint16_t>(next) * 2;
    next = static_cast<uint8_t>(doubled > MAX_BLOCK_STATES ? MAX_BLOCK_STATES : doubled);
  }
  void *memory = realloc(blocks, static_cast<size_t>(next) * sizeof(BlockState));
  if (!memory) return false;
  blocks = static_cast<BlockState *>(memory);
  blockCapacity = next;
  return true;
}

bool ensureTurnoutCapacity(uint8_t required)
{
  if (required <= turnoutCapacity) return true;
  if (required > MAX_TURNOUT_STATES) return false;
  uint8_t next = turnoutCapacity ? turnoutCapacity : 4;
  while (next < required)
  {
    const uint16_t doubled = static_cast<uint16_t>(next) * 2;
    next = static_cast<uint8_t>(doubled > MAX_TURNOUT_STATES ? MAX_TURNOUT_STATES : doubled);
  }
  void *memory = realloc(turnouts, static_cast<size_t>(next) * sizeof(TurnoutState));
  if (!memory) return false;
  turnouts = static_cast<TurnoutState *>(memory);
  turnoutCapacity = next;
  return true;
}

void clearLoadedState()
{
  free(blocks); blocks = nullptr; loadedBlockCount = 0; blockCapacity = 0;
  free(turnouts); turnouts = nullptr; loadedTurnoutCount = 0; turnoutCapacity = 0;
}

void markChanged()
{
  savePending = true;
  changedAtMs = millis();
}

int findBlock(uint16_t id)
{
  for (uint8_t i = 0; i < loadedBlockCount; ++i)
    if (blocks[i].id == id) return i;
  return -1;
}

void removeBlockAt(uint8_t index)
{
  if (index >= loadedBlockCount) return;
  for (uint8_t i = index + 1; i < loadedBlockCount; ++i) blocks[i - 1] = blocks[i];
  --loadedBlockCount;
}

int findTurnout(uint16_t id, uint8_t channel)
{
  for (uint8_t i = 0; i < loadedTurnoutCount; ++i)
    if (turnouts[i].id == id && turnouts[i].channel == channel) return i;
  return -1;
}

void removeTurnoutAt(uint8_t index)
{
  if (index >= loadedTurnoutCount) return;
  for (uint8_t i = index + 1; i < loadedTurnoutCount; ++i) turnouts[i - 1] = turnouts[i];
  --loadedTurnoutCount;
}

bool saveState()
{
  LittleFS.remove(TEMP_PATH);
  File file = LittleFS.open(TEMP_PATH, "w");
  if (!file) return false;

  file.print(F("{\"version\":"));
  file.print(STATE_VERSION);
  file.print(F(",\"blocks\":["));
  for (uint8_t i = 0; i < loadedBlockCount; ++i)
  {
    if (i) file.print(',');
    file.print(F("{\"id\":")); file.print(blocks[i].id);
    file.print(F(",\"locoAddress\":")); file.print(blocks[i].locoAddress);
    file.print('}');
  }
  file.print(F("],\"turnouts\":["));
  for (uint8_t i = 0; i < loadedTurnoutCount; ++i)
  {
    if (i) file.print(',');
    file.print(F("{\"id\":")); file.print(turnouts[i].id);
    file.print(F(",\"channel\":")); file.print(turnouts[i].channel);
    file.print(F(",\"closed\":")); file.print(turnouts[i].closed ? F("true") : F("false"));
    file.print('}');
  }
  file.print(F("]}"));
  file.flush();
  const bool ok = file.getWriteError() == 0;
  file.close();
  if (!ok) { LittleFS.remove(TEMP_PATH); return false; }

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

bool appendBlock(uint16_t id, int locoAddress)
{
  if (!id || locoAddress < 1 || locoAddress > 10239 || !DCCExpressLiteLayoutRegistry::containsBlock(id)) return false;
  if (loadedBlockCount >= MAX_BLOCK_STATES || !ensureBlockCapacity(loadedBlockCount + 1)) return false;
  BlockState &state = blocks[loadedBlockCount++];
  state.id = id;
  state.locoAddress = static_cast<uint16_t>(locoAddress);
  return true;
}

bool appendTurnout(uint16_t id, uint8_t channel, bool closed)
{
  DCCExpressLiteLayoutRegistry::TurnoutEndpoint endpoint;
  if (!id || channel > 1 || !DCCExpressLiteLayoutRegistry::getTurnout(id, channel, endpoint)) return false;
  if (loadedTurnoutCount >= MAX_TURNOUT_STATES || !ensureTurnoutCapacity(loadedTurnoutCount + 1)) return false;
  TurnoutState &state = turnouts[loadedTurnoutCount++];
  state.id = id;
  state.channel = channel;
  state.closed = closed;
  return true;
}

void loadState()
{
  clearLoadedState();
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

  const int version = document["version"] | 1;
  bool migrated = version != STATE_VERSION;

  for (JsonObjectConst source : document["blocks"].as<JsonArrayConst>())
  {
    if (loadedBlockCount >= MAX_BLOCK_STATES) break;
    const int locoAddress = source["locoAddress"] | 0;
    uint16_t id = 0;
    if (version >= 2)
    {
      const int rawId = source["id"] | 0;
      if (rawId > 0 && rawId <= 65535) id = static_cast<uint16_t>(rawId);
    }
    else
    {
      const char *legacyId = source["blockId"] | "";
      DCCExpressLiteLayoutRegistry::normalizeBlockId(legacyId, id);
    }
    if (!appendBlock(id, locoAddress)) continue;
  }

  for (JsonObjectConst source : document["turnouts"].as<JsonArrayConst>())
  {
    if (loadedTurnoutCount >= MAX_TURNOUT_STATES) break;
    uint16_t id = 0;
    uint8_t channel = 0;
    if (version >= 2)
    {
      const int rawId = source["id"] | 0;
      const int rawChannel = source["channel"] | 0;
      if (rawId > 0 && rawId <= 65535 && rawChannel >= 0 && rawChannel <= 1)
      {
        id = static_cast<uint16_t>(rawId);
        channel = static_cast<uint8_t>(rawChannel);
      }
    }
    else
    {
      const int address = source["address"] | 0;
      DCCExpressLiteLayoutRegistry::TurnoutEndpoint endpoint;
      if (address > 0 && address <= 32767 &&
          DCCExpressLiteLayoutRegistry::findTurnoutByAddress(static_cast<uint16_t>(address), endpoint))
      {
        id = endpoint.id;
        channel = endpoint.channel;
      }
    }
    appendTurnout(id, channel, source["closed"] | false);
  }

  Serial.printf("Runtime state: loaded %u block(s), %u turnout(s), version=%d.\n",
                loadedBlockCount, loadedTurnoutCount, version);

  if (migrated)
  {
    Serial.println(F("Runtime state: legacy state migrated to numeric layout IDs."));
    markChanged();
  }
}
}

void DCCExpressLiteRuntimeState::begin()
{
  DCCExpressLiteLayoutRegistry::begin();
  if (!LittleFS.exists(STATE_PATH))
  {
    File file = LittleFS.open(STATE_PATH, "w");
    if (file)
    {
      file.print(F("{\"version\":2,\"blocks\":[],\"turnouts\":[]}"));
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

bool DCCExpressLiteRuntimeState::setBlock(uint16_t blockId, uint16_t locoAddress)
{
  if (!DCCExpressLiteLayoutRegistry::containsBlock(blockId) ||
      locoAddress < 1 || locoAddress > 10239)
    return false;

  for (int i = loadedBlockCount - 1; i >= 0; --i)
  {
    if (blocks[i].id == blockId || blocks[i].locoAddress == locoAddress)
      removeBlockAt(static_cast<uint8_t>(i));
  }

  if (!appendBlock(blockId, locoAddress)) return false;
  markChanged();
  return true;
}

bool DCCExpressLiteRuntimeState::removeBlock(uint16_t blockId)
{
  if (!DCCExpressLiteLayoutRegistry::containsBlock(blockId)) return false;
  const int index = findBlock(blockId);
  if (index < 0) return true;
  removeBlockAt(static_cast<uint8_t>(index));
  markChanged();
  return true;
}

void DCCExpressLiteRuntimeState::resetBlocks()
{
  if (!loadedBlockCount) return;
  loadedBlockCount = 0;
  markChanged();
}

String DCCExpressLiteRuntimeState::blockStateJson()
{
  String json = "{";
  for (uint8_t i = 0; i < loadedBlockCount; ++i)
  {
    if (i) json += ',';
    const String id = String(blocks[i].id);
    json += "\"" + id + "\":{\"blockId\":\"" + id +
      "\",\"locoId\":null,\"locoAddress\":" + String(blocks[i].locoAddress) + "}";
  }
  json += '}';
  return json;
}

uint8_t DCCExpressLiteRuntimeState::blockCount() { return loadedBlockCount; }

void DCCExpressLiteRuntimeState::setTurnout(uint16_t address, bool closed)
{
  DCCExpressLiteLayoutRegistry::TurnoutEndpoint endpoint;
  if (!DCCExpressLiteLayoutRegistry::findTurnoutByAddress(address, endpoint))
  {
    Serial.printf("Runtime state: turnout address #%u is not uniquely mapped to a layout ID.\n", address);
    return;
  }

  const int index = findTurnout(endpoint.id, endpoint.channel);
  if (index >= 0)
  {
    if (turnouts[index].closed == closed) return;
    turnouts[index].closed = closed;
    markChanged();
    return;
  }

  if (!appendTurnout(endpoint.id, endpoint.channel, closed)) return;
  markChanged();
}

int8_t DCCExpressLiteRuntimeState::getTurnout(uint16_t address)
{
  DCCExpressLiteLayoutRegistry::TurnoutEndpoint endpoint;
  if (!DCCExpressLiteLayoutRegistry::findTurnoutByAddress(address, endpoint)) return -1;
  const int index = findTurnout(endpoint.id, endpoint.channel);
  return index >= 0 ? (turnouts[index].closed ? 1 : 0) : -1;
}

uint8_t DCCExpressLiteRuntimeState::turnoutCount() { return loadedTurnoutCount; }

bool DCCExpressLiteRuntimeState::getTurnoutAt(uint8_t index, uint16_t &address, bool &closed)
{
  if (index >= loadedTurnoutCount) return false;
  DCCExpressLiteLayoutRegistry::TurnoutEndpoint endpoint;
  if (!DCCExpressLiteLayoutRegistry::getTurnout(turnouts[index].id, turnouts[index].channel, endpoint)) return false;
  address = endpoint.address;
  closed = turnouts[index].closed;
  return true;
}

bool DCCExpressLiteRuntimeState::pruneBlocksFromLayout()
{
  if (!DCCExpressLiteLayoutRegistry::reload()) return false;

  bool changed = false;
  for (int i = loadedBlockCount - 1; i >= 0; --i)
  {
    if (DCCExpressLiteLayoutRegistry::containsBlock(blocks[i].id)) continue;
    removeBlockAt(static_cast<uint8_t>(i));
    changed = true;
  }

  for (int i = loadedTurnoutCount - 1; i >= 0; --i)
  {
    DCCExpressLiteLayoutRegistry::TurnoutEndpoint endpoint;
    if (DCCExpressLiteLayoutRegistry::getTurnout(turnouts[i].id, turnouts[i].channel, endpoint)) continue;
    removeTurnoutAt(static_cast<uint8_t>(i));
    changed = true;
  }

  if (changed) markChanged();
  return true;
}
