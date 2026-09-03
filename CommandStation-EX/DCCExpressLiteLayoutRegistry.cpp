#include "DCCExpressLiteLayoutRegistry.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <stdlib.h>
#include <string.h>

namespace
{
constexpr const char *LAYOUT_PATH = "/layout.json";
constexpr uint16_t MAX_LAYOUT_ELEMENTS = 384;
constexpr uint16_t MAX_TURNOUT_ENDPOINTS = 192;
constexpr uint16_t MAX_BASIC_ACCESSORY_ENDPOINTS = 256;
constexpr uint16_t MAX_SENSOR_ENDPOINTS = 128;
constexpr uint16_t MAX_SIGNAL_ENDPOINTS = 64;
constexpr uint16_t MAX_BLOCK_IDS = 96;
constexpr uint16_t MAX_LEGACY_IDS = 384;

struct LegacyIdEntry
{
  uint64_t hash = 0;
  uint16_t id = 0;
};

DCCExpressLiteLayoutRegistry::TurnoutEndpoint turnoutEndpoints[MAX_TURNOUT_ENDPOINTS];
DCCExpressLiteLayoutRegistry::BasicAccessoryEndpoint basicAccessoryEndpoints[MAX_BASIC_ACCESSORY_ENDPOINTS];
DCCExpressLiteLayoutRegistry::SensorEndpoint sensorEndpoints[MAX_SENSOR_ENDPOINTS];
DCCExpressLiteLayoutRegistry::SignalEndpoint signalEndpoints[MAX_SIGNAL_ENDPOINTS];
uint16_t blockIds[MAX_BLOCK_IDS];
LegacyIdEntry legacyIds[MAX_LEGACY_IDS];
uint16_t reservedIds[MAX_LAYOUT_ELEMENTS];
uint16_t secondPassNumericIds[MAX_LAYOUT_ELEMENTS];

uint16_t loadedElementCount = 0;
uint16_t loadedTurnoutCount = 0;
uint16_t loadedBasicAccessoryCount = 0;
uint16_t loadedSensorCount = 0;
uint16_t loadedSignalCount = 0;
uint16_t loadedBlockCount = 0;
uint16_t loadedLegacyCount = 0;
uint16_t reservedCount = 0;
uint16_t secondPassNumericCount = 0;
bool registryReady = false;

uint64_t hashId(const char *text)
{
  // 64-bit FNV-1a. We only keep hashes for legacy string IDs, avoiding a
  // several-kilobyte permanent UUID string table on the ESP32.
  uint64_t hash = 1469598103934665603ULL;
  if (!text) return hash;
  while (*text)
  {
    hash ^= static_cast<uint8_t>(*text++);
    hash *= 1099511628211ULL;
  }
  return hash;
}

bool containsValue(const uint16_t *items, uint16_t count, uint16_t value)
{
  for (uint16_t i = 0; i < count; ++i)
    if (items[i] == value) return true;
  return false;
}

bool validId(long value)
{
  return value >= 1 && value <= 65535;
}

bool parseDecimalId(const char *value, uint16_t &id)
{
  if (!value || !value[0]) return false;
  char *end = nullptr;
  const unsigned long parsed = strtoul(value, &end, 10);
  if (!end || *end != '\0' || parsed < 1 || parsed > 65535) return false;
  id = static_cast<uint16_t>(parsed);
  return true;
}

void resetRegistry()
{
  loadedElementCount = 0;
  loadedTurnoutCount = 0;
  loadedBasicAccessoryCount = 0;
  loadedSensorCount = 0;
  loadedSignalCount = 0;
  loadedBlockCount = 0;
  loadedLegacyCount = 0;
  reservedCount = 0;
  secondPassNumericCount = 0;
  registryReady = false;
}

uint16_t allocateId()
{
  for (uint32_t candidate = 1; candidate <= 65535; ++candidate)
  {
    const uint16_t id = static_cast<uint16_t>(candidate);
    if (containsValue(reservedIds, reservedCount, id)) continue;
    if (reservedCount >= MAX_LAYOUT_ELEMENTS) return 0;
    reservedIds[reservedCount++] = id;
    return id;
  }
  return 0;
}

void rememberLegacy(const char *legacy, uint16_t id)
{
  if (!legacy || !legacy[0] || !id || loadedLegacyCount >= MAX_LEGACY_IDS) return;
  const uint64_t hash = hashId(legacy);
  for (uint16_t i = 0; i < loadedLegacyCount; ++i)
  {
    if (legacyIds[i].hash == hash)
    {
      // Same old ID should always resolve to the first element that owned it.
      return;
    }
  }
  LegacyIdEntry &entry = legacyIds[loadedLegacyCount++];
  entry.hash = hash;
  entry.id = id;
}

uint16_t assignedId(JsonVariantConst value)
{
  if (value.is<long>() || value.is<unsigned long>() || value.is<int>() || value.is<unsigned int>())
  {
    const long raw = value.as<long>();
    if (validId(raw))
    {
      const uint16_t id = static_cast<uint16_t>(raw);
      if (!containsValue(secondPassNumericIds, secondPassNumericCount, id))
      {
        if (secondPassNumericCount < MAX_LAYOUT_ELEMENTS)
          secondPassNumericIds[secondPassNumericCount++] = id;
        return id;
      }
      // Duplicate persisted numeric ID: give the later element a fresh ID.
      return allocateId();
    }
  }

  const char *legacy = value.is<const char *>() ? value.as<const char *>() : "";
  const uint16_t id = allocateId();
  rememberLegacy(legacy, id);
  return id;
}

bool isTurnoutType(const char *type)
{
  return type && !strncmp(type, "trackturnout", 12);
}

bool isDoubleTurnoutType(const char *type)
{
  return type && !strcmp(type, "trackturnoutdouble");
}

bool addTurnout(uint16_t id, uint8_t channel, uint16_t address,
                DCCExpressLiteLayoutRegistry::OutputMode mode, bool closedValue)
{
  if (!id || !address || loadedTurnoutCount >= MAX_TURNOUT_ENDPOINTS) return false;
  DCCExpressLiteLayoutRegistry::TurnoutEndpoint &endpoint =
    turnoutEndpoints[loadedTurnoutCount++];
  endpoint.id = id;
  endpoint.channel = channel;
  endpoint.address = address;
  endpoint.mode = mode;
  endpoint.closedValue = closedValue;
  return true;
}


bool addBasicAccessory(uint16_t id, uint8_t channel, uint16_t address)
{
  if (!id || !address || loadedBasicAccessoryCount >= MAX_BASIC_ACCESSORY_ENDPOINTS) return false;
  DCCExpressLiteLayoutRegistry::BasicAccessoryEndpoint &endpoint =
    basicAccessoryEndpoints[loadedBasicAccessoryCount++];
  endpoint.id = id;
  endpoint.channel = channel;
  endpoint.address = address;
  return true;
}

void indexElement(JsonObjectConst element, uint16_t id)
{
  const char *type = element["type"] | "";
  if (!id) return;

  if (isTurnoutType(type))
  {
    const char *modeText = element["outputMode"] | "accessory";
    const auto mode = !strcmp(modeText, "vpin")
      ? DCCExpressLiteLayoutRegistry::OutputMode::Vpin
      : DCCExpressLiteLayoutRegistry::OutputMode::Accessory;

    if (isDoubleTurnoutType(type))
    {
      const int address1 = element["turnout1Address"] | 0;
      const int address2 = element["turnout2Address"] | 0;
      if (address1 > 0 && address1 <= 32767)
        addTurnout(id, 0, static_cast<uint16_t>(address1), mode,
                   element["turnout1ClosedValue"] | false);
      if (address2 > 0 && address2 <= 32767)
        addTurnout(id, 1, static_cast<uint16_t>(address2), mode,
                   element["turnout2ClosedValue"] | false);
    }
    else
    {
      const int address = element["turnoutAddress"] | 0;
      if (address > 0 && address <= 32767)
        addTurnout(id, 0, static_cast<uint16_t>(address), mode,
                   element["turnoutClosedValue"] | false);
    }
    return;
  }

  if (!strcmp(type, "button"))
  {
    const char *modeText = element["outputMode"] | "accessory";
    const int address = element["address"] | 0;
    if (strcmp(modeText, "vpin") && address > 0 && address <= 32767)
      addBasicAccessory(id, 0, static_cast<uint16_t>(address));
    return;
  }

  if (!strcmp(type, "tracklevelcrossing"))
  {
    const int address = element["basicAccessoryAddress"] | 0;
    if (address > 0 && address <= 32767)
      addBasicAccessory(id, 0, static_cast<uint16_t>(address));
    return;
  }

  if (!strcmp(type, "tracksensor"))
  {
    const int address = element["address"] | 0;
    if (address > 0 && address <= 32767 && loadedSensorCount < MAX_SENSOR_ENDPOINTS)
      {
        DCCExpressLiteLayoutRegistry::SensorEndpoint &endpoint =
          sensorEndpoints[loadedSensorCount++];
        endpoint.id = id;
        endpoint.address = static_cast<uint16_t>(address);
      }
    return;
  }

  if (!strcmp(type, "trackblock"))
  {
    if (loadedBlockCount < MAX_BLOCK_IDS) blockIds[loadedBlockCount++] = id;
    return;
  }

  if (!strcmp(type, "tracksignal2") || !strcmp(type, "tracksignal3") || !strcmp(type, "tracksignal4"))
  {
    JsonObjectConst output = element["signalOutput"].as<JsonObjectConst>();
    if (output.isNull()) return;
    const int address = output["address"] | 0;
    if (address < 1 || address > 32767 || loadedSignalCount >= MAX_SIGNAL_ENDPOINTS) return;
    const char *protocol = output["protocol"] | "dcc";
    const bool extended = !strcmp(protocol, "dccext");
    int outputs = output["outputCount"] | 1;
    if (outputs < 1) outputs = 1;
    if (outputs > 16) outputs = 16;
    DCCExpressLiteLayoutRegistry::SignalEndpoint &endpoint =
      signalEndpoints[loadedSignalCount++];
    endpoint.id = id;
    endpoint.address = static_cast<uint16_t>(address);
    endpoint.extended = extended;
    endpoint.outputCount = static_cast<uint8_t>(outputs);

    if (!extended)
    {
      for (int channel = 0; channel < outputs; ++channel)
      {
        const uint32_t outputAddress = static_cast<uint32_t>(address) + static_cast<uint32_t>(channel);
        if (outputAddress > 32767) break;
        addBasicAccessory(id, static_cast<uint8_t>(channel), static_cast<uint16_t>(outputAddress));
      }
    }
  }
}
}

bool DCCExpressLiteLayoutRegistry::begin()
{
  return reload();
}

bool DCCExpressLiteLayoutRegistry::reload()
{
  resetRegistry();

  File file = LittleFS.open(LAYOUT_PATH, "r");
  if (!file)
  {
    Serial.println(F("Layout registry: /layout.json is missing."));
    return false;
  }

  JsonDocument document;
  const DeserializationError error = deserializeJson(document, file);
  file.close();
  if (error)
  {
    Serial.printf("Layout registry: invalid layout JSON: %s\n", error.c_str());
    return false;
  }

  // Pass 1 reserves every unique valid numeric ID before legacy UUIDs are
  // assigned. This makes the migration deterministic and prevents an early
  // UUID element from stealing a numeric ID that occurs later in the file.
  for (JsonObjectConst layer : document["layers"].as<JsonArrayConst>())
  {
    for (JsonObjectConst element : layer["elements"].as<JsonArrayConst>())
    {
      JsonVariantConst value = element["id"];
      if (!(value.is<long>() || value.is<unsigned long>() || value.is<int>() || value.is<unsigned int>()))
        continue;
      const long raw = value.as<long>();
      if (!validId(raw)) continue;
      const uint16_t id = static_cast<uint16_t>(raw);
      if (containsValue(reservedIds, reservedCount, id)) continue;
      if (reservedCount >= MAX_LAYOUT_ELEMENTS)
      {
        Serial.println(F("Layout registry: too many layout elements."));
        resetRegistry();
        return false;
      }
      reservedIds[reservedCount++] = id;
    }
  }

  // Pass 2 assigns IDs in persisted layer/element order and builds the compact
  // endpoint index needed by runtime state and automation.
  for (JsonObjectConst layer : document["layers"].as<JsonArrayConst>())
  {
    for (JsonObjectConst element : layer["elements"].as<JsonArrayConst>())
    {
      if (loadedElementCount >= MAX_LAYOUT_ELEMENTS)
      {
        Serial.println(F("Layout registry: element limit reached."));
        resetRegistry();
        return false;
      }
      const uint16_t id = assignedId(element["id"]);
      if (!id)
      {
        Serial.println(F("Layout registry: could not allocate numeric element ID."));
        resetRegistry();
        return false;
      }
      ++loadedElementCount;
      indexElement(element, id);
    }
  }

  registryReady = true;
  Serial.printf(
    "Layout registry: %u element(s), %u turnout endpoint(s), %u basic accessory endpoint(s), %u sensor(s), %u signal(s), %u block(s).\n",
    loadedElementCount, loadedTurnoutCount, loadedBasicAccessoryCount, loadedSensorCount, loadedSignalCount, loadedBlockCount);
  return true;
}

bool DCCExpressLiteLayoutRegistry::ready() { return registryReady; }

bool DCCExpressLiteLayoutRegistry::getTurnout(uint16_t id, uint8_t channel, TurnoutEndpoint &out)
{
  for (uint16_t i = 0; i < loadedTurnoutCount; ++i)
  {
    if (turnoutEndpoints[i].id == id && turnoutEndpoints[i].channel == channel)
    {
      out = turnoutEndpoints[i];
      return true;
    }
  }
  return false;
}

bool DCCExpressLiteLayoutRegistry::findTurnoutByAddress(uint16_t address, TurnoutEndpoint &out)
{
  int found = -1;
  for (uint16_t i = 0; i < loadedTurnoutCount; ++i)
  {
    if (turnoutEndpoints[i].address != address) continue;
    if (found >= 0)
    {
      Serial.printf("Layout registry: turnout address #%u is ambiguous.\n", address);
      return false;
    }
    found = static_cast<int>(i);
  }
  if (found < 0) return false;
  out = turnoutEndpoints[found];
  return true;
}

bool DCCExpressLiteLayoutRegistry::findTurnoutByAddress(
  uint16_t address, OutputMode mode, TurnoutEndpoint &out)
{
  int found = -1;
  for (uint16_t i = 0; i < loadedTurnoutCount; ++i)
  {
    if (turnoutEndpoints[i].address != address || turnoutEndpoints[i].mode != mode) continue;
    if (found >= 0)
    {
      Serial.printf("Layout registry: turnout address #%u is ambiguous for output mode.\n", address);
      return false;
    }
    found = static_cast<int>(i);
  }
  if (found < 0) return false;
  out = turnoutEndpoints[found];
  return true;
}


bool DCCExpressLiteLayoutRegistry::getBasicAccessory(
  uint16_t id, uint8_t channel, BasicAccessoryEndpoint &out)
{
  for (uint16_t i = 0; i < loadedBasicAccessoryCount; ++i)
  {
    if (basicAccessoryEndpoints[i].id == id && basicAccessoryEndpoints[i].channel == channel)
    {
      out = basicAccessoryEndpoints[i];
      return true;
    }
  }
  return false;
}

bool DCCExpressLiteLayoutRegistry::findBasicAccessoryByAddress(
  uint16_t address, BasicAccessoryEndpoint &out)
{
  int found = -1;
  for (uint16_t i = 0; i < loadedBasicAccessoryCount; ++i)
  {
    if (basicAccessoryEndpoints[i].address != address) continue;
    if (found >= 0)
    {
      Serial.printf("Layout registry: basic accessory address #%u is ambiguous.\n", address);
      return false;
    }
    found = static_cast<int>(i);
  }
  if (found < 0) return false;
  out = basicAccessoryEndpoints[found];
  return true;
}

bool DCCExpressLiteLayoutRegistry::getSensor(uint16_t id, SensorEndpoint &out)
{
  for (uint16_t i = 0; i < loadedSensorCount; ++i)
    if (sensorEndpoints[i].id == id) { out = sensorEndpoints[i]; return true; }
  return false;
}

bool DCCExpressLiteLayoutRegistry::findSensorByAddress(uint16_t address, SensorEndpoint &out)
{
  int found = -1;
  for (uint16_t i = 0; i < loadedSensorCount; ++i)
  {
    if (sensorEndpoints[i].address != address) continue;
    if (found >= 0) return false;
    found = static_cast<int>(i);
  }
  if (found < 0) return false;
  out = sensorEndpoints[found];
  return true;
}

bool DCCExpressLiteLayoutRegistry::getSignal(uint16_t id, SignalEndpoint &out)
{
  for (uint16_t i = 0; i < loadedSignalCount; ++i)
    if (signalEndpoints[i].id == id) { out = signalEndpoints[i]; return true; }
  return false;
}

bool DCCExpressLiteLayoutRegistry::findSignalByAddress(
  uint16_t address, bool extended, SignalEndpoint &out)
{
  int found = -1;
  for (uint16_t i = 0; i < loadedSignalCount; ++i)
  {
    if (signalEndpoints[i].address != address || signalEndpoints[i].extended != extended) continue;
    if (found >= 0)
    {
      Serial.printf("Layout registry: signal address #%u is ambiguous for mode.\n", address);
      return false;
    }
    found = static_cast<int>(i);
  }
  if (found < 0) return false;
  out = signalEndpoints[found];
  return true;
}

bool DCCExpressLiteLayoutRegistry::containsBlock(uint16_t id)
{
  return containsValue(blockIds, loadedBlockCount, id);
}

bool DCCExpressLiteLayoutRegistry::resolveLegacyId(const char *legacyId, uint16_t &id)
{
  if (!legacyId || !legacyId[0]) return false;
  const uint64_t hash = hashId(legacyId);
  for (uint16_t i = 0; i < loadedLegacyCount; ++i)
  {
    if (legacyIds[i].hash == hash)
    {
      id = legacyIds[i].id;
      return true;
    }
  }
  return false;
}

bool DCCExpressLiteLayoutRegistry::normalizeBlockId(const char *value, uint16_t &id)
{
  uint16_t numeric = 0;
  if (parseDecimalId(value, numeric) && containsBlock(numeric))
  {
    id = numeric;
    return true;
  }
  return resolveLegacyId(value, id) && containsBlock(id);
}

uint16_t DCCExpressLiteLayoutRegistry::elementCount() { return loadedElementCount; }
uint16_t DCCExpressLiteLayoutRegistry::turnoutCount() { return loadedTurnoutCount; }
uint16_t DCCExpressLiteLayoutRegistry::basicAccessoryCount() { return loadedBasicAccessoryCount; }
uint16_t DCCExpressLiteLayoutRegistry::sensorCount() { return loadedSensorCount; }
uint16_t DCCExpressLiteLayoutRegistry::signalCount() { return loadedSignalCount; }
