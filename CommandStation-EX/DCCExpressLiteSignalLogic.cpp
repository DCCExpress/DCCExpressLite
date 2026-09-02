#include "DCCExpressLiteSignalLogic.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "DCC.h"
#include "Sensors.h"

namespace
{
constexpr const char *RULES_PATH = "/signal-rules.jsonl";
constexpr const char *LEGACY_RULES_PATH = "/signal-rules.json";
constexpr const char *LAYOUT_PATH = "/layout.json";

constexpr uint8_t MAX_GROUPS = 24;
constexpr uint8_t MAX_RULES_PER_GROUP = 6;
constexpr uint8_t MAX_CONDITIONS_PER_RULE = 6;
constexpr uint8_t MAX_SENSOR_INPUTS = 32;
constexpr uint8_t MAX_SIGNAL_STATES = 16;
constexpr uint8_t MAX_SIGNAL_OUTPUTS = 16;
constexpr uint32_t SENSOR_POLL_INTERVAL_MS = 100;
constexpr uint32_t RULES_FILE_CHECK_INTERVAL_MS = 500;

struct SignalCondition
{
  uint16_t address = 0;
  bool sensor = false;
  bool vpin = false;
  bool expected = false;
};

struct SignalState
{
  uint64_t idHash = 0;
  uint8_t extendedAspect = 0;
  uint16_t dccBits = 0;
};

struct SignalRule
{
  uint8_t stateIndex = 0;
  uint8_t conditionCount = 0;
  SignalCondition conditions[MAX_CONDITIONS_PER_RULE];
};

struct SignalGroup
{
  uint16_t signalAddress = 0;

  bool extended = false;
  bool vpin = false;

  uint8_t outputCount = 0;
  uint8_t stateCount = 0;
  SignalState states[MAX_SIGNAL_STATES];

  uint8_t defaultStateIndex = 0;
  uint8_t ruleCount = 0;
  SignalRule rules[MAX_RULES_PER_GROUP];

  bool lastStateValid = false;
  uint8_t lastStateIndex = 0;
  uint8_t desiredStateIndex = 0;

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
uint32_t lastRulesFileCheckAtMs = 0;
uint64_t observedRulesFingerprint = 0;
size_t observedRulesSize = 0;
bool observedRulesJsonl = false;

DCCExpressLiteSignalLogic::TurnoutStateReader readTurnout = nullptr;
DCCExpressLiteSignalLogic::VpinStateReader readVpin = nullptr;
DCCExpressLiteSignalLogic::OutputWriter writeOutput = nullptr;

uint64_t hashStateId(const char *value)
{
  // 64-bit FNV-1a. The web UI still stores the full stable state ID;
  // firmware only needs a compact key while resolving rules at load time.
  constexpr uint64_t offsetBasis = 14695981039346656037ULL;
  constexpr uint64_t prime = 1099511628211ULL;

  uint64_t hash = offsetBasis;

  if (!value)
    return hash;

  while (*value)
  {
    hash ^= static_cast<uint8_t>(*value++);
    hash *= prime;
  }

  return hash;
}

uint64_t fingerprintFile(const char *path, size_t &sizeOut)
{
  constexpr uint64_t offsetBasis = 14695981039346656037ULL;
  constexpr uint64_t prime = 1099511628211ULL;

  File file = LittleFS.open(path, "r");
  if (!file)
  {
    sizeOut = 0;
    return 0;
  }

  sizeOut = file.size();
  uint64_t hash = offsetBasis;
  uint8_t buffer[128];

  while (file.available())
  {
    const size_t count = file.read(buffer, sizeof(buffer));
    if (!count) break;

    for (size_t i = 0; i < count; ++i)
    {
      hash ^= buffer[i];
      hash *= prime;
    }
  }

  file.close();
  return hash;
}

const char *activeRulesPath()
{
  return LittleFS.exists(RULES_PATH)
    ? RULES_PATH
    : LEGACY_RULES_PATH;
}

void rememberRulesFingerprint()
{
  observedRulesJsonl = LittleFS.exists(RULES_PATH);
  observedRulesFingerprint =
    fingerprintFile(activeRulesPath(), observedRulesSize);
  lastRulesFileCheckAtMs = millis();
}

bool rulesFileChanged()
{
  const uint32_t now = millis();
  if (now - lastRulesFileCheckAtMs < RULES_FILE_CHECK_INTERVAL_MS)
    return false;

  lastRulesFileCheckAtMs = now;

  const bool jsonlExists = LittleFS.exists(RULES_PATH);
  size_t size = 0;
  const uint64_t fingerprint =
    fingerprintFile(jsonlExists ? RULES_PATH : LEGACY_RULES_PATH, size);

  if (jsonlExists == observedRulesJsonl &&
      size == observedRulesSize &&
      fingerprint == observedRulesFingerprint)
    return false;

  observedRulesJsonl = jsonlExists;
  observedRulesSize = size;
  observedRulesFingerprint = fingerprint;
  return true;
}

bool equalsIgnoreCase(const char *a, const char *b)
{
  if (!a || !b) return false;

  while (*a && *b)
  {
    if (tolower(static_cast<unsigned char>(*a)) !=
        tolower(static_cast<unsigned char>(*b)))
      return false;

    ++a;
    ++b;
  }

  return *a == '\0' && *b == '\0';
}

void resetConfiguration()
{
  loadedGroupCount = 0;
  sensorInputCount = 0;

  for (SignalGroup &group : groups)
    group = SignalGroup{};

  for (SensorInput &sensor : sensorInputs)
    sensor = SensorInput{};
}

SensorInput *findSensorInput(uint16_t address)
{
  for (uint8_t i = 0; i < sensorInputCount; ++i)
    if (sensorInputs[i].address == address)
      return &sensorInputs[i];

  return nullptr;
}

void registerSensorInput(uint16_t address)
{
  if (!address ||
      findSensorInput(address) ||
      sensorInputCount >= MAX_SENSOR_INPUTS)
    return;

  sensorInputs[sensorInputCount].address = address;
  sensorInputs[sensorInputCount].simulatedState = -1;
  sensorInputs[sensorInputCount].observedState = -2;
  ++sensorInputCount;
}

int8_t readSensorState(uint16_t address)
{
  Sensor *sensor = Sensor::get(address);

  if (sensor)
    return sensor->active ? 1 : 0;

  SensorInput *input = findSensorInput(address);
  return input ? input->simulatedState : -1;
}

bool readCondition(const SignalCondition &condition)
{
  const int8_t state =
    condition.sensor
      ? readSensorState(condition.address)
      : (
          condition.vpin
            ? (readVpin ? readVpin(condition.address) : -1)
            : (readTurnout ? readTurnout(condition.address) : -1)
        );

  return state >= 0 &&
    (state != 0) == condition.expected;
}

bool isElementType(
  JsonObjectConst element,
  const char *expected)
{
  const char *type =
    element["type"] | "";

  if (!strcmp(expected, "turnout"))
    return !strncmp(type, "trackturnout", 12);

  if (!strcmp(expected, "signal"))
    return !strcmp(type, "tracksignal2") ||
      !strcmp(type, "tracksignal3") ||
      !strcmp(type, "tracksignal4");

  return !strcmp(type, expected);
}

JsonObjectConst findElementById(
  JsonDocument &layout,
  const char *id,
  const char *expectedType)
{
  if (!id || !id[0])
    return JsonObjectConst();

  for (
    JsonObjectConst layer :
    layout["layers"].as<JsonArrayConst>())
  {
    for (
      JsonObjectConst element :
      layer["elements"].as<JsonArrayConst>())
    {
      if (
        !strcmp(element["id"] | "", id) &&
        isElementType(element, expectedType))
      {
        return element;
      }
    }
  }

  return JsonObjectConst();
}

JsonObjectConst findSignalElement(
  JsonDocument &layout,
  const char *id,
  int legacyAddress)
{
  JsonObjectConst byId =
    findElementById(
      layout,
      id,
      "signal");

  if (!byId.isNull())
    return byId;

  if (legacyAddress <= 0)
    return JsonObjectConst();

  for (
    JsonObjectConst layer :
    layout["layers"].as<JsonArrayConst>())
  {
    for (
      JsonObjectConst element :
      layer["elements"].as<JsonArrayConst>())
    {
      if (!isElementType(element, "signal"))
        continue;

      int address =
        element["signalOutput"]["address"] | 0;

      if (address <= 0)
        address = element["address"] | 0;

      if (address == legacyAddress)
        return element;
    }
  }

  return JsonObjectConst();
}

int resolveElementAddress(
  JsonDocument &layout,
  const char *id,
  int legacyAddress,
  const char *expectedType)
{
  JsonObjectConst element =
    findElementById(
      layout,
      id,
      expectedType);

  if (!element.isNull())
  {
    if (!strcmp(expectedType, "turnout"))
    {
      const int turnoutAddress =
        element["turnoutAddress"] | 0;

      return turnoutAddress > 0
        ? turnoutAddress
        : static_cast<int>(
            element["turnout1Address"] | 0);
    }

    return static_cast<int>(
      element["address"] | 0);
  }

  return legacyAddress;
}

bool resolveElementUsesVpin(
  JsonDocument &layout,
  const char *id,
  const char *expectedType)
{
  JsonObjectConst element =
    findElementById(
      layout,
      id,
      expectedType);

  return !element.isNull() &&
    !strcmp(
      element["outputMode"] | "accessory",
      "vpin");
}

int findStateIndexById(
  const SignalGroup &group,
  const char *stateId)
{
  if (!stateId || !stateId[0])
    return -1;

  const uint64_t wanted =
    hashStateId(stateId);

  for (
    uint8_t i = 0;
    i < group.stateCount;
    ++i)
  {
    if (
      group.states[i].idHash ==
      wanted)
      return i;
  }

  return -1;
}

int findStateIndexByLabel(
  JsonObjectConst signalElement,
  const SignalGroup &group,
  const char *label)
{
  if (!label || !label[0])
    return -1;

  const JsonArrayConst states =
    signalElement["signalOutput"]["states"]
      .as<JsonArrayConst>();

  uint8_t index = 0;

  for (JsonObjectConst state : states)
  {
    if (index >= group.stateCount)
      break;

    if (
      equalsIgnoreCase(
        state["label"] | "",
        label))
      return index;

    ++index;
  }

  // Legacy fixed signal layout fallback.
  if (equalsIgnoreCase(label, "red"))
    return group.stateCount > 0 ? 0 : -1;

  if (equalsIgnoreCase(label, "green"))
    return group.stateCount > 1 ? 1 : -1;

  if (equalsIgnoreCase(label, "yellow"))
    return group.stateCount > 2 ? 2 : -1;

  if (equalsIgnoreCase(label, "white"))
    return group.stateCount > 3 ? 3 : -1;

  return -1;
}

void loadLegacySignalDefinition(
  JsonObjectConst element,
  SignalGroup &group)
{
  group.extended = false;

  group.signalAddress =
    static_cast<uint16_t>(
      element["address"] | 0);

  group.outputCount =
    constrain(
      static_cast<int>(
        element["addressLength"] | 5),
      1,
      MAX_SIGNAL_OUTPUTS);

  struct LegacyState
  {
    const char *id;
    const char *label;
    uint32_t bits;
  };

  const LegacyState legacy[] = {
    {
      "legacy-red",
      "red",
      element["valueRed"] | 0U
    },
    {
      "legacy-green",
      "green",
      element["valueGreen"] | 0U
    },
    {
      "legacy-yellow",
      "yellow",
      element["valueYellow"] | 0U
    },
    {
      "legacy-white",
      "white",
      element["valueWhite"] | 0U
    },
  };

  const int lampCount =
    constrain(
      static_cast<int>(
        element["aspect"] | 2),
      2,
      4);

  group.stateCount =
    static_cast<uint8_t>(lampCount);

  for (
    uint8_t i = 0;
    i < group.stateCount;
    ++i)
  {
    group.states[i].idHash =
      hashStateId(
        legacy[i].id);

    group.states[i].dccBits =
      static_cast<uint16_t>(
        legacy[i].bits & 0xFFFFU);
  }
}

void loadDynamicSignalDefinition(
  JsonObjectConst element,
  SignalGroup &group)
{
  JsonObjectConst config =
    element["signalOutput"]
      .as<JsonObjectConst>();

  group.extended =
    !strcmp(
      config["protocol"] | "dcc",
      "dccext");

  group.vpin = false;

  int configuredAddress =
    config["address"] | 0;

  if (configuredAddress <= 0)
    configuredAddress =
      element["address"] | 0;

  group.signalAddress =
    static_cast<uint16_t>(
      configuredAddress);

  group.outputCount =
    group.extended
      ? 1
      : constrain(
          static_cast<int>(
            config["outputCount"] | 1),
          1,
          MAX_SIGNAL_OUTPUTS);

  const JsonArrayConst states =
    config["states"].as<JsonArrayConst>();

  for (JsonObjectConst state : states)
  {
    if (
      group.stateCount >=
      MAX_SIGNAL_STATES)
      break;

    SignalState &target =
      group.states[
        group.stateCount];

    target.idHash =
      hashStateId(
        state["id"] | "");

    target.extendedAspect =
      constrain(
        static_cast<int>(
          state["aspect"] | 0),
        0,
        255);

    uint16_t bits = 0;
    uint8_t outputIndex = 0;

    for (
      JsonVariantConst direction :
      state["dccOutputs"]
        .as<JsonArrayConst>())
    {
      if (
        outputIndex >=
        group.outputCount)
        break;

      const char *value =
        direction | "R";

      if (!strcmp(value, "G"))
        bits |=
          static_cast<uint16_t>(
            1U << outputIndex);

      ++outputIndex;
    }

    target.dccBits = bits;
    ++group.stateCount;
  }
}

bool loadSignalDefinition(
  JsonObjectConst element,
  SignalGroup &group)
{
  if (element.isNull())
    return false;

  if (
    element["signalOutput"]
      .is<JsonObjectConst>())
  {
    loadDynamicSignalDefinition(
      element,
      group);
  }
  else
  {
    loadLegacySignalDefinition(
      element,
      group);
  }

  return
    group.signalAddress > 0 &&
    group.stateCount > 0 &&
    (
      group.extended ||
      group.outputCount > 0
    );
}

uint8_t resolveRuleStateIndex(
  JsonObjectConst jsonRule,
  JsonObjectConst signalElement,
  const SignalGroup &group)
{
  const int byId =
    findStateIndexById(
      group,
      jsonRule["stateId"] | "");

  if (byId >= 0)
    return static_cast<uint8_t>(byId);

  const char *legacy =
    jsonRule["aspect"] | "";

  const int byLabel =
    findStateIndexByLabel(
      signalElement,
      group,
      legacy);

  if (byLabel >= 0)
    return static_cast<uint8_t>(byLabel);

  return group.defaultStateIndex;
}

uint8_t resolveDefaultStateIndex(
  JsonObjectConst jsonGroup,
  JsonObjectConst signalElement,
  const SignalGroup &group)
{
  const int byId =
    findStateIndexById(
      group,
      jsonGroup["defaultStateId"] | "");

  if (byId >= 0)
    return static_cast<uint8_t>(byId);

  const char *legacy =
    jsonGroup["defaultAspect"] | "red";

  const int byLabel =
    findStateIndexByLabel(
      signalElement,
      group,
      legacy);

  if (byLabel >= 0)
    return static_cast<uint8_t>(byLabel);

  return 0;
}

uint8_t evaluateGroup(
  const SignalGroup &group)
{
  for (
    uint8_t ruleIndex = 0;
    ruleIndex < group.ruleCount;
    ++ruleIndex)
  {
    const SignalRule &rule =
      group.rules[ruleIndex];

    bool matches = true;

    for (
      uint8_t conditionIndex = 0;
      conditionIndex <
        rule.conditionCount;
      ++conditionIndex)
    {
      if (
        readCondition(
          rule.conditions[
            conditionIndex]))
        continue;

      matches = false;
      break;
    }

    if (matches)
      return rule.stateIndex;
  }

  return group.defaultStateIndex;
}

void evaluateAll()
{
  for (
    uint8_t i = 0;
    i < loadedGroupCount;
    ++i)
  {
    SignalGroup &group =
      groups[i];

    if (
      !group.signalAddress ||
      !group.stateCount)
      continue;

    const uint8_t nextState =
      evaluateGroup(group);

    if (
      group.lastStateValid &&
      group.lastStateIndex ==
        nextState &&
      !group.outputPending)
      continue;

    if (
      group.outputPending &&
      group.desiredStateIndex ==
        nextState)
      continue;

    group.desiredStateIndex =
      nextState;

    group.outputIndex = 0;
    group.outputPending = true;
  }
}

void processOneOutput()
{
  for (
    uint8_t i = 0;
    i < loadedGroupCount;
    ++i)
  {
    SignalGroup &group =
      groups[i];

    if (!group.outputPending)
      continue;

    if (
      group.desiredStateIndex >=
      group.stateCount)
    {
      group.outputPending = false;
      continue;
    }

    const SignalState &state =
      group.states[
        group.desiredStateIndex];

    if (group.extended)
    {
      DCC::setExtendedAccessory(
        static_cast<int16_t>(
          group.signalAddress),
        static_cast<int16_t>(
          state.extendedAspect));

      group.lastStateIndex =
        group.desiredStateIndex;

      group.lastStateValid = true;
      group.outputPending = false;
      return;
    }

    if (!writeOutput)
      return;

    const uint8_t bit =
      group.outputIndex;

    const bool active =
      ((state.dccBits >> bit) & 1U) != 0;

    writeOutput(
      group.signalAddress + bit,
      active,
      group.vpin);

    ++group.outputIndex;

    if (
      group.outputIndex >=
      group.outputCount)
    {
      group.lastStateIndex =
        group.desiredStateIndex;

      group.lastStateValid = true;
      group.outputPending = false;
    }

    return;
  }
}

void pollSensors()
{
  const uint32_t now = millis();

  if (
    now - lastSensorPollAtMs <
    SENSOR_POLL_INTERVAL_MS)
    return;

  lastSensorPollAtMs = now;

  for (
    uint8_t i = 0;
    i < sensorInputCount;
    ++i)
  {
    SensorInput &input =
      sensorInputs[i];

    const int8_t state =
      readSensorState(input.address);

    if (
      state ==
      input.observedState)
      continue;

    input.observedState = state;
    evaluationPending = true;
  }
}

void parseRuleGroup(
  JsonObjectConst jsonGroup,
  JsonDocument &layout)
{
  if (loadedGroupCount >= MAX_GROUPS)
    return;

  JsonObjectConst signalElement =
    findSignalElement(
      layout,
      jsonGroup["signalId"] | "",
      jsonGroup["signalAddress"] | 0);

  if (signalElement.isNull())
    return;

  SignalGroup candidate;

  if (!loadSignalDefinition(signalElement, candidate))
    return;

  candidate.defaultStateIndex =
    resolveDefaultStateIndex(
      jsonGroup,
      signalElement,
      candidate);

  for (
    JsonObjectConst jsonRule :
    jsonGroup["rules"].as<JsonArrayConst>())
  {
    if (candidate.ruleCount >= MAX_RULES_PER_GROUP)
      break;

    SignalRule rule;

    rule.stateIndex =
      resolveRuleStateIndex(
        jsonRule,
        signalElement,
        candidate);

    bool valid = true;

    for (
      JsonObjectConst jsonCondition :
      jsonRule["conditions"].as<JsonArrayConst>())
    {
      if (rule.conditionCount >= MAX_CONDITIONS_PER_RULE)
        break;

      SignalCondition condition;

      const char *type =
        jsonCondition["type"] | "turnout";

      condition.sensor =
        !strcmp(type, "sensor");

      condition.vpin =
        !condition.sensor &&
        resolveElementUsesVpin(
          layout,
          jsonCondition["turnoutId"] | "",
          "turnout");

      const int address =
        condition.sensor
          ? resolveElementAddress(
              layout,
              jsonCondition["sensorId"] | "",
              jsonCondition["sensorAddress"] | 0,
              "tracksensor")
          : resolveElementAddress(
              layout,
              jsonCondition["turnoutId"] | "",
              jsonCondition["turnoutAddress"] | 0,
              "turnout");

      const int maxAddress =
        condition.vpin ? 32767 : 2048;

      if (address < 1 || address > maxAddress)
      {
        valid = false;
        break;
      }

      condition.address =
        static_cast<uint16_t>(address);

      condition.expected =
        condition.sensor
          ? (jsonCondition["active"] | false)
          : (jsonCondition["closed"] | false);

      rule.conditions[rule.conditionCount++] =
        condition;

      if (condition.sensor)
        registerSensorInput(condition.address);
    }

    if (valid)
      candidate.rules[candidate.ruleCount++] = rule;
  }

  groups[loadedGroupCount++] = candidate;
}

void parseRules(
  JsonDocument &document,
  JsonDocument &layout)
{
  enabled =
    document["enabled"].is<bool>()
      ? document["enabled"].as<bool>()
      : document["autostart"].as<bool>();

  for (
    JsonObjectConst jsonGroup :
    document["groups"].as<JsonArrayConst>())
  {
    parseRuleGroup(jsonGroup, layout);

    if (loadedGroupCount >= MAX_GROUPS)
      break;
  }
}

bool parseJsonlRules(
  File &rulesFile,
  JsonDocument &layout)
{
  bool sawMeta = false;

  while (rulesFile.available())
  {
    while (rulesFile.available())
    {
      const int next = rulesFile.peek();
      if (next != '\r' && next != '\n' && next != ' ' && next != '\t')
        break;
      rulesFile.read();
    }

    if (!rulesFile.available())
      break;

    JsonDocument row;
    const DeserializationError error =
      deserializeJson(row, rulesFile);

    if (error)
    {
      Serial.printf(
        "Signal logic: invalid JSONL row: %s\n",
        error.c_str());
      return false;
    }

    const char *kind = row["kind"] | "";

    if (!strcmp(kind, "meta"))
    {
      enabled = row["enabled"] | false;
      sawMeta = true;
      continue;
    }

    if (!strcmp(kind, "group"))
    {
      JsonObjectConst group =
        row["group"].is<JsonObjectConst>()
          ? row["group"].as<JsonObjectConst>()
          : row.as<JsonObjectConst>();

      parseRuleGroup(group, layout);

      if (loadedGroupCount >= MAX_GROUPS)
        break;
    }
  }

  if (!sawMeta)
  {
    Serial.println(
      F("Signal logic: JSONL meta row is missing."));
    return false;
  }

  return true;
}}

void DCCExpressLiteSignalLogic::begin(
  TurnoutStateReader turnoutReader,
  VpinStateReader vpinReader,
  OutputWriter outputWriter)
{
  readTurnout = turnoutReader;
  readVpin = vpinReader;
  writeOutput = outputWriter;

  if (!LittleFS.exists(RULES_PATH) &&
      !LittleFS.exists(LEGACY_RULES_PATH))
  {
    File file = LittleFS.open(RULES_PATH, "w");

    if (file)
    {
      file.println(
        F("{\"kind\":\"meta\",\"version\":3,\"enabled\":false}"));
      file.close();
    }
  }

  reload();
  rememberRulesFingerprint();
}

bool DCCExpressLiteSignalLogic::reload()
{
  resetConfiguration();

  enabled = false;
  running = false;

  File layoutFile = LittleFS.open(LAYOUT_PATH, "r");

  if (!layoutFile)
  {
    Serial.println(
      F("Signal logic: layout file is unavailable."));
    return false;
  }

  JsonDocument layoutDocument;
  const DeserializationError layoutError =
    deserializeJson(layoutDocument, layoutFile);
  layoutFile.close();

  if (layoutError)
  {
    Serial.printf(
      "Signal logic: invalid layout JSON: %s\n",
      layoutError.c_str());
    return false;
  }

  bool loaded = false;

  if (LittleFS.exists(RULES_PATH))
  {
    File rulesFile = LittleFS.open(RULES_PATH, "r");

    if (!rulesFile)
    {
      Serial.println(
        F("Signal logic: JSONL rules file is unavailable."));
      return false;
    }

    loaded = parseJsonlRules(rulesFile, layoutDocument);
    rulesFile.close();
  }
  else
  {
    File rulesFile = LittleFS.open(LEGACY_RULES_PATH, "r");

    if (!rulesFile)
    {
      Serial.println(
        F("Signal logic: legacy rules file is unavailable."));
      return false;
    }

    JsonDocument rulesDocument;
    const DeserializationError rulesError =
      deserializeJson(rulesDocument, rulesFile);
    rulesFile.close();

    if (rulesError)
    {
      Serial.printf(
        "Signal logic: invalid legacy rules JSON: %s\n",
        rulesError.c_str());
      return false;
    }

    parseRules(rulesDocument, layoutDocument);
    loaded = true;
  }

  if (!loaded)
    return false;

  running = enabled;
  evaluationPending = running;
  lastSensorPollAtMs = 0;

  rememberRulesFingerprint();

  Serial.printf(
    "Signal logic: %s, %u group(s), %u sensor input(s), format=%s.\n",
    running ? "enabled" : "disabled",
    loadedGroupCount,
    sensorInputCount,
    LittleFS.exists(RULES_PATH) ? "jsonl" : "legacy-json");

  return true;
}

void DCCExpressLiteSignalLogic::loop()
{
  if (rulesFileChanged())
  {
    reload();
  }

  if (!running)
    return;

  pollSensors();

  if (evaluationPending)
  {
    evaluationPending = false;
    evaluateAll();
  }

  processOneOutput();
}

void DCCExpressLiteSignalLogic::setEnabled(
  bool nextEnabled)
{
  enabled = nextEnabled;
  running = nextEnabled;

  if (!running)
  {
    for (
      uint8_t i = 0;
      i < loadedGroupCount;
      ++i)
    {
      groups[i].outputPending =
        false;
    }

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

void DCCExpressLiteSignalLogic::notifyTurnout(
  uint16_t,
  bool)
{
  if (running)
    evaluationPending = true;
}

void DCCExpressLiteSignalLogic::notifySensor(
  uint16_t address,
  bool active)
{
  SensorInput *input =
    findSensorInput(address);

  if (!input)
    return;

  input->simulatedState =
    active ? 1 : 0;

  if (running)
    evaluationPending = true;
}

void DCCExpressLiteSignalLogic::forceEvaluate()
{
  for (
    uint8_t i = 0;
    i < loadedGroupCount;
    ++i)
  {
    groups[i].lastStateValid =
      false;

    groups[i].outputPending =
      false;
  }

  if (running)
    evaluationPending = true;
}
