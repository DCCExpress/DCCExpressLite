#include "DCCExpressLiteSignalLogic.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "Sensors.h"

namespace
{
constexpr const char *RULES_PATH = "/signal-rules.json";
constexpr const char *LAYOUT_PATH = "/layout.json";
constexpr uint8_t MAX_GROUPS = 24;
constexpr uint8_t MAX_RULES_PER_GROUP = 6;
constexpr uint8_t MAX_CONDITIONS_PER_RULE = 6;
constexpr uint8_t MAX_SENSOR_INPUTS = 32;
constexpr uint32_t SENSOR_POLL_INTERVAL_MS = 100;

enum class SignalAspect : uint8_t
{
  Red = 0,
  Green = 1,
  Yellow = 2,
  White = 3,
};

struct SignalCondition
{
  uint16_t address = 0;
  bool sensor = false;
  bool expected = false;
};

struct SignalRule
{
  SignalAspect aspect = SignalAspect::Red;
  uint8_t conditionCount = 0;
  SignalCondition conditions[MAX_CONDITIONS_PER_RULE];
};

struct SignalGroup
{
  uint16_t signalAddress = 0;
  uint8_t addressLength = 0;
  uint32_t aspectValues[4] = {0, 0, 0, 0};
  SignalAspect defaultAspect = SignalAspect::Red;
  uint8_t ruleCount = 0;
  SignalRule rules[MAX_RULES_PER_GROUP];

  bool lastAspectValid = false;
  SignalAspect lastAspect = SignalAspect::Red;
  SignalAspect desiredAspect = SignalAspect::Red;
  bool outputPending = false;
  uint8_t outputIndex = 0;
};

struct SensorInput
{
  uint16_t address = 0;
  int8_t simulatedState = -1;
  int8_t observedState = -2;
};

SignalGroup groups[MAX_GROUPS];
uint8_t loadedGroupCount = 0;
SensorInput sensorInputs[MAX_SENSOR_INPUTS];
uint8_t sensorInputCount = 0;
bool enabled = false;
bool running = false;
bool evaluationPending = false;
uint32_t lastSensorPollAtMs = 0;
DCCExpressLiteSignalLogic::TurnoutStateReader readTurnout = nullptr;
DCCExpressLiteSignalLogic::AccessoryWriter writeAccessory = nullptr;

SignalAspect parseAspect(const char *value)
{
  if (!value) return SignalAspect::Red;
  if (!strcmp(value, "green")) return SignalAspect::Green;
  if (!strcmp(value, "yellow")) return SignalAspect::Yellow;
  if (!strcmp(value, "white")) return SignalAspect::White;
  return SignalAspect::Red;
}

uint8_t aspectIndex(SignalAspect aspect)
{
  return static_cast<uint8_t>(aspect);
}

void resetConfiguration()
{
  loadedGroupCount = 0;
  sensorInputCount = 0;
  for (SignalGroup &group : groups) group = SignalGroup{};
  for (SensorInput &sensor : sensorInputs) sensor = SensorInput{};
}

SensorInput *findSensorInput(uint16_t address)
{
  for (uint8_t i = 0; i < sensorInputCount; ++i)
    if (sensorInputs[i].address == address) return &sensorInputs[i];
  return nullptr;
}

void registerSensorInput(uint16_t address)
{
  if (!address || findSensorInput(address) || sensorInputCount >= MAX_SENSOR_INPUTS) return;
  sensorInputs[sensorInputCount].address = address;
  sensorInputs[sensorInputCount].simulatedState = -1;
  sensorInputs[sensorInputCount].observedState = -2;
  ++sensorInputCount;
}

int8_t readSensorState(uint16_t address)
{
  Sensor *sensor = Sensor::get(address);
  if (sensor) return sensor->active ? 1 : 0;

  SensorInput *input = findSensorInput(address);
  return input ? input->simulatedState : -1;
}

bool readCondition(const SignalCondition &condition)
{
  const int8_t state = condition.sensor
    ? readSensorState(condition.address)
    : (readTurnout ? readTurnout(condition.address) : -1);
  return state >= 0 && (state != 0) == condition.expected;
}

SignalAspect evaluateGroup(const SignalGroup &group)
{
  for (uint8_t ruleIndex = 0; ruleIndex < group.ruleCount; ++ruleIndex)
  {
    const SignalRule &rule = group.rules[ruleIndex];
    bool matches = true;
    for (uint8_t conditionIndex = 0; conditionIndex < rule.conditionCount; ++conditionIndex)
    {
      if (readCondition(rule.conditions[conditionIndex])) continue;
      matches = false;
      break;
    }
    if (matches) return rule.aspect;
  }
  return group.defaultAspect;
}

void evaluateAll()
{
  for (uint8_t i = 0; i < loadedGroupCount; ++i)
  {
    SignalGroup &group = groups[i];
    if (!group.signalAddress || !group.addressLength) continue;

    const SignalAspect nextAspect = evaluateGroup(group);
    if (group.lastAspectValid && group.lastAspect == nextAspect && !group.outputPending) continue;
    if (group.outputPending && group.desiredAspect == nextAspect) continue;

    group.desiredAspect = nextAspect;
    group.outputIndex = 0;
    group.outputPending = true;
  }
}

void processOneAccessoryOutput()
{
  if (!writeAccessory) return;

  for (uint8_t i = 0; i < loadedGroupCount; ++i)
  {
    SignalGroup &group = groups[i];
    if (!group.outputPending) continue;

    const uint32_t bits = group.aspectValues[aspectIndex(group.desiredAspect)];
    const uint8_t bit = group.outputIndex;
    writeAccessory(group.signalAddress + bit, ((bits >> bit) & 1U) != 0);

    ++group.outputIndex;
    if (group.outputIndex >= group.addressLength)
    {
      group.lastAspect = group.desiredAspect;
      group.lastAspectValid = true;
      group.outputPending = false;
    }
    return;
  }
}

void pollSensors()
{
  const uint32_t now = millis();
  if (now - lastSensorPollAtMs < SENSOR_POLL_INTERVAL_MS) return;
  lastSensorPollAtMs = now;

  for (uint8_t i = 0; i < sensorInputCount; ++i)
  {
    SensorInput &input = sensorInputs[i];
    const int8_t state = readSensorState(input.address);
    if (state == input.observedState) continue;
    input.observedState = state;
    evaluationPending = true;
  }
}

bool isElementType(JsonObjectConst element, const char *expected)
{
  const char *type = element["type"] | "";
  if (!strcmp(expected, "turnout")) return !strncmp(type, "trackturnout", 12);
  return !strcmp(type, expected);
}

int resolveElementAddress(JsonDocument &layout, const char *id, int legacyAddress,
                          const char *expectedType)
{
  if (id && id[0])
  {
    for (JsonObjectConst layer : layout["layers"].as<JsonArrayConst>())
      for (JsonObjectConst element : layer["elements"].as<JsonArrayConst>())
        if (!strcmp(element["id"] | "", id) && isElementType(element, expectedType))
        {
          if (!strcmp(expectedType, "turnout"))
          {
            const int turnoutAddress = element["turnoutAddress"] | 0;
            return turnoutAddress > 0
              ? turnoutAddress
              : static_cast<int>(element["turnout1Address"] | 0);
          }
          return static_cast<int>(element["address"] | 0);
        }
    return 0;
  }
  return legacyAddress;
}

void parseRules(JsonDocument &document, JsonDocument &layout)
{
  enabled = document["enabled"].is<bool>()
    ? document["enabled"].as<bool>()
    : document["autostart"].as<bool>();
  const JsonArrayConst jsonGroups = document["groups"].as<JsonArrayConst>();

  for (JsonObjectConst jsonGroup : jsonGroups)
  {
    if (loadedGroupCount >= MAX_GROUPS) break;
    const int address = resolveElementAddress(layout, jsonGroup["signalId"] | "",
      jsonGroup["signalAddress"] | 0, "tracksignal2");
    if (address < 1 || address > 2048) continue;

    SignalGroup &group = groups[loadedGroupCount++];
    group.signalAddress = static_cast<uint16_t>(address);
    group.defaultAspect = parseAspect(jsonGroup["defaultAspect"] | "red");

    for (JsonObjectConst jsonRule : jsonGroup["rules"].as<JsonArrayConst>())
    {
      if (group.ruleCount >= MAX_RULES_PER_GROUP) break;
      SignalRule &rule = group.rules[group.ruleCount];
      rule.aspect = parseAspect(jsonRule["aspect"] | "red");
      bool ruleValid = true;

      for (JsonObjectConst jsonCondition : jsonRule["conditions"].as<JsonArrayConst>())
      {
        if (rule.conditionCount >= MAX_CONDITIONS_PER_RULE) break;
        SignalCondition condition;
        const char *type = jsonCondition["type"] | "turnout";
        condition.sensor = !strcmp(type, "sensor");
        const int inputAddress = condition.sensor
          ? resolveElementAddress(layout, jsonCondition["sensorId"] | "",
              jsonCondition["sensorAddress"] | 0, "tracksensor")
          : resolveElementAddress(layout, jsonCondition["turnoutId"] | "",
              jsonCondition["turnoutAddress"] | 0, "turnout");
        if (inputAddress < 1 || inputAddress > 2048)
        {
          ruleValid = false;
          break;
        }

        condition.address = static_cast<uint16_t>(inputAddress);
        condition.expected = condition.sensor
          ? (jsonCondition["active"] | false)
          : (jsonCondition["closed"] | false);
        rule.conditions[rule.conditionCount++] = condition;
        if (condition.sensor) registerSensorInput(condition.address);
      }
      if (ruleValid) ++group.ruleCount;
      else rule = SignalRule{};
    }
  }
}

void loadSignalDefinitions(JsonDocument &layout)
{
  for (JsonObjectConst layer : layout["layers"].as<JsonArrayConst>())
  {
    for (JsonObjectConst element : layer["elements"].as<JsonArrayConst>())
    {
      const char *type = element["type"] | "";
      if (strcmp(type, "tracksignal2")) continue;
      const int address = element["address"] | 0;

      for (uint8_t i = 0; i < loadedGroupCount; ++i)
      {
        SignalGroup &group = groups[i];
        if (group.signalAddress != address) continue;
        group.addressLength = constrain(static_cast<int>(element["addressLength"] | 5), 1, 8);
        group.aspectValues[aspectIndex(SignalAspect::Red)] = element["valueRed"] | 0;
        group.aspectValues[aspectIndex(SignalAspect::Green)] = element["valueGreen"] | 0;
        group.aspectValues[aspectIndex(SignalAspect::Yellow)] = element["valueYellow"] | 0;
        group.aspectValues[aspectIndex(SignalAspect::White)] = element["valueWhite"] | 0;
      }
    }
  }
}
}

void DCCExpressLiteSignalLogic::begin(TurnoutStateReader turnoutReader, AccessoryWriter accessoryWriter)
{
  readTurnout = turnoutReader;
  writeAccessory = accessoryWriter;

  if (!LittleFS.exists(RULES_PATH))
  {
    File file = LittleFS.open(RULES_PATH, "w");
    if (file)
    {
      file.print(F("{\"version\":2,\"enabled\":false,\"groups\":[]}"));
      file.close();
    }
  }

  reload();
}

bool DCCExpressLiteSignalLogic::reload()
{
  resetConfiguration();
  enabled = false;
  running = false;

  File rulesFile = LittleFS.open(RULES_PATH, "r");
  if (!rulesFile)
  {
    Serial.println(F("Signal logic: rules file is unavailable."));
    return false;
  }

  JsonDocument rulesDocument;
  const DeserializationError rulesError = deserializeJson(rulesDocument, rulesFile);
  rulesFile.close();
  if (rulesError)
  {
    Serial.printf("Signal logic: invalid rules JSON: %s\n", rulesError.c_str());
    return false;
  }
  File layoutFile = LittleFS.open(LAYOUT_PATH, "r");
  if (layoutFile)
  {
    JsonDocument layoutDocument;
    const DeserializationError layoutError = deserializeJson(layoutDocument, layoutFile);
    layoutFile.close();
    if (!layoutError)
    {
      parseRules(rulesDocument, layoutDocument);
      loadSignalDefinitions(layoutDocument);
    }
    else Serial.printf("Signal logic: invalid layout JSON: %s\n", layoutError.c_str());
  }
  else Serial.println(F("Signal logic: layout file is unavailable."));

  running = enabled;
  evaluationPending = running;
  lastSensorPollAtMs = 0;
  Serial.printf("Signal logic: %s, %u group(s), %u sensor input(s).\n",
    running ? "enabled" : "disabled", loadedGroupCount, sensorInputCount);
  return true;
}

void DCCExpressLiteSignalLogic::loop()
{
  if (!running) return;
  pollSensors();
  if (evaluationPending)
  {
    evaluationPending = false;
    evaluateAll();
  }
  processOneAccessoryOutput();
}

void DCCExpressLiteSignalLogic::setEnabled(bool nextEnabled)
{
  enabled = nextEnabled;
  running = nextEnabled;
  if (!running)
  {
    for (uint8_t i = 0; i < loadedGroupCount; ++i) groups[i].outputPending = false;
    return;
  }
  forceEvaluate();
}

bool DCCExpressLiteSignalLogic::isEnabled()
{
  return enabled;
}

bool DCCExpressLiteSignalLogic::isRunning()
{
  return running;
}

uint8_t DCCExpressLiteSignalLogic::groupCount()
{
  return loadedGroupCount;
}

void DCCExpressLiteSignalLogic::notifyTurnout(uint16_t, bool)
{
  if (running) evaluationPending = true;
}

void DCCExpressLiteSignalLogic::notifySensor(uint16_t address, bool active)
{
  SensorInput *input = findSensorInput(address);
  if (!input) return;
  input->simulatedState = active ? 1 : 0;
  if (running) evaluationPending = true;
}

void DCCExpressLiteSignalLogic::forceEvaluate()
{
  for (uint8_t i = 0; i < loadedGroupCount; ++i)
  {
    groups[i].lastAspectValid = false;
    groups[i].outputPending = false;
    groups[i].outputIndex = 0;
  }
  if (running) evaluationPending = true;
}
