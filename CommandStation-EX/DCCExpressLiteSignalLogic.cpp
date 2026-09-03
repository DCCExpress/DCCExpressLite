#include "DCCExpressLiteSignalLogic.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "DCC.h"
#include "DCCExpressLiteSignalLogicApi.h"
#include "Sensors.h"

namespace
{
constexpr const char *RULES_PATH = "/signal-rules.jsonl";

constexpr uint8_t MAX_SIGNALS = 24;
constexpr uint8_t MAX_RULES_PER_SIGNAL = 6;
constexpr uint8_t MAX_CONDITIONS_PER_RULE = 6;
constexpr uint8_t MAX_SENSOR_INPUTS = 32;
constexpr uint8_t MAX_BASIC_OUTPUTS = 16;

constexpr uint32_t SENSOR_POLL_INTERVAL_MS = 100;

enum class ConditionSource : uint8_t
{
  Turnout,
  Sensor,
  Vpin
};

struct Condition
{
  ConditionSource source = ConditionSource::Turnout;
  uint16_t address = 0;
  bool value = false;
};

struct Rule
{
  uint16_t resultValue = 0;
  uint8_t conditionCount = 0;
  Condition conditions[MAX_CONDITIONS_PER_RULE];
};

struct Signal
{
  uint16_t address = 0;
  bool extended = true;
  uint8_t outputCount = 1;

  uint16_t defaultValue = 0;
  uint8_t ruleCount = 0;
  Rule rules[MAX_RULES_PER_SIGNAL];

  bool lastValueValid = false;
  uint16_t lastValue = 0;

  uint16_t desiredValue = 0;
  bool outputPending = false;
  uint8_t outputIndex = 0;
};

struct SensorInput
{
  uint16_t address = 0;
  int8_t simulatedState = -1;
  int8_t observedState = -2;
};

Signal signals[MAX_SIGNALS];
uint8_t signalCount = 0;

SensorInput sensorInputs[MAX_SENSOR_INPUTS];
uint8_t sensorInputCount = 0;

bool enabled = false;
bool running = false;
bool evaluationPending = false;
uint32_t lastSensorPollAtMs = 0;

DCCExpressLiteSignalLogic::TurnoutStateReader readTurnout = nullptr;
DCCExpressLiteSignalLogic::VpinStateReader readVpin = nullptr;
DCCExpressLiteSignalLogic::OutputWriter writeOutput = nullptr;

void resetConfiguration()
{
  signalCount = 0;
  sensorInputCount = 0;

  for (Signal &signal : signals)
    signal = Signal{};

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

bool registerSensorInput(uint16_t address)
{
  if (!address)
    return false;

  if (findSensorInput(address))
    return true;

  if (sensorInputCount >= MAX_SENSOR_INPUTS)
    return false;

  SensorInput &input = sensorInputs[sensorInputCount++];
  input.address = address;
  input.simulatedState = -1;
  input.observedState = -2;
  return true;
}

int8_t readSensorState(uint16_t address)
{
  Sensor *sensor = Sensor::get(address);

  if (sensor)
    return sensor->active ? 1 : 0;

  SensorInput *input = findSensorInput(address);
  return input ? input->simulatedState : -1;
}

int8_t readConditionState(const Condition &condition)
{
  switch (condition.source)
  {
    case ConditionSource::Sensor:
      return readSensorState(condition.address);

    case ConditionSource::Vpin:
      return readVpin
        ? readVpin(condition.address)
        : -1;

    case ConditionSource::Turnout:
    default:
      return readTurnout
        ? readTurnout(condition.address)
        : -1;
  }
}

bool conditionMatches(const Condition &condition)
{
  const int8_t state =
    readConditionState(condition);

  return state >= 0 &&
    ((state != 0) == condition.value);
}

bool ruleMatches(const Rule &rule)
{
  for (uint8_t i = 0; i < rule.conditionCount; ++i)
    if (!conditionMatches(rule.conditions[i]))
      return false;

  return true;
}

uint16_t evaluateSignal(const Signal &signal)
{
  for (uint8_t i = 0; i < signal.ruleCount; ++i)
    if (ruleMatches(signal.rules[i]))
      return signal.rules[i].resultValue;

  return signal.defaultValue;
}

void evaluateAll()
{
  for (uint8_t i = 0; i < signalCount; ++i)
  {
    Signal &signal = signals[i];

    const uint16_t nextValue =
      evaluateSignal(signal);

    if (signal.lastValueValid &&
        signal.lastValue == nextValue &&
        !signal.outputPending)
      continue;

    if (signal.outputPending &&
        signal.desiredValue == nextValue)
      continue;

    signal.desiredValue = nextValue;
    signal.outputIndex = 0;
    signal.outputPending = true;

    Serial.printf(
      "Signal logic: signal #%u -> value %u.\n",
      signal.address,
      nextValue);
  }
}

void processOneOutput()
{
  for (uint8_t i = 0; i < signalCount; ++i)
  {
    Signal &signal = signals[i];

    if (!signal.outputPending)
      continue;

    if (signal.extended)
    {
      Serial.printf(
        "Signal logic: <A %u %u>\n",
        signal.address,
        signal.desiredValue);

      DCC::setExtendedAccessory(
        static_cast<int16_t>(signal.address),
        static_cast<int16_t>(signal.desiredValue));

      signal.lastValue = signal.desiredValue;
      signal.lastValueValid = true;
      signal.outputPending = false;
      return;
    }

    if (!writeOutput)
      return;

    const uint8_t bit =
      signal.outputIndex;

    const bool active =
      ((signal.desiredValue >> bit) & 1U) != 0;

    Serial.printf(
      "Signal logic: <a %u %u>\n",
      signal.address + bit,
      active ? 1 : 0);

    writeOutput(
      signal.address + bit,
      active,
      false);

    ++signal.outputIndex;

    if (signal.outputIndex >= signal.outputCount)
    {
      signal.lastValue = signal.desiredValue;
      signal.lastValueValid = true;
      signal.outputPending = false;
    }

    return;
  }
}

void pollSensors()
{
  const uint32_t now = millis();

  if (now - lastSensorPollAtMs <
      SENSOR_POLL_INTERVAL_MS)
    return;

  lastSensorPollAtMs = now;

  for (uint8_t i = 0; i < sensorInputCount; ++i)
  {
    SensorInput &input = sensorInputs[i];

    const int8_t state =
      readSensorState(input.address);

    if (state == input.observedState)
      continue;

    input.observedState = state;
    evaluationPending = true;
  }
}

bool parseCondition(
  JsonArrayConst source,
  Condition &condition)
{
  if (source.size() != 3)
    return false;

  const char *type = source[0] | "";
  const int address = source[1] | 0;
  const int value = source[2] | -1;

  if (address < 1 ||
      address > 32767 ||
      (value != 0 && value != 1))
    return false;

  if (!strcmp(type, "turnout"))
  {
    if (address > 2048)
      return false;

    condition.source =
      ConditionSource::Turnout;
  }
  else if (!strcmp(type, "sensor"))
  {
    condition.source =
      ConditionSource::Sensor;

    if (!registerSensorInput(
          static_cast<uint16_t>(address)))
      return false;
  }
  else if (!strcmp(type, "vpin"))
  {
    condition.source =
      ConditionSource::Vpin;
  }
  else
  {
    return false;
  }

  condition.address =
    static_cast<uint16_t>(address);

  condition.value =
    value != 0;

  return true;
}

bool parseSignalRow(JsonObjectConst row)
{
  if (signalCount >= MAX_SIGNALS)
    return false;

  const int address = row["address"] | 0;
  const char *mode = row["mode"] | "";
  const int defaultValue = row["default"] | -1;

  if (address < 1 ||
      address > 2048 ||
      defaultValue < 0 ||
      defaultValue > 65535)
    return false;

  Signal signal;
  signal.address =
    static_cast<uint16_t>(address);

  if (!strcmp(mode, "extended"))
  {
    if (defaultValue > 255)
      return false;

    signal.extended = true;
    signal.outputCount = 1;
  }
  else if (!strcmp(mode, "basic"))
  {
    const int outputs = row["outputs"] | 0;

    if (outputs < 1 ||
        outputs > MAX_BASIC_OUTPUTS)
      return false;

    signal.extended = false;
    signal.outputCount =
      static_cast<uint8_t>(outputs);
  }
  else
  {
    return false;
  }

  signal.defaultValue =
    static_cast<uint16_t>(defaultValue);

  const JsonArrayConst rules =
    row["rules"].as<JsonArrayConst>();

  if (rules.size() >
      MAX_RULES_PER_SIGNAL)
    return false;

  for (JsonObjectConst jsonRule : rules)
  {
    Rule rule;

    const int resultValue =
      jsonRule["value"] | -1;

    if (resultValue < 0 ||
        resultValue > 65535 ||
        (signal.extended &&
         resultValue > 255))
      return false;

    rule.resultValue =
      static_cast<uint16_t>(resultValue);

    const JsonArrayConst conditions =
      jsonRule["conditions"].as<JsonArrayConst>();

    if (conditions.size() >
        MAX_CONDITIONS_PER_RULE)
      return false;

    for (JsonArrayConst jsonCondition :
         conditions)
    {
      Condition condition;

      if (!parseCondition(
            jsonCondition,
            condition))
        return false;

      rule.conditions[
        rule.conditionCount++] =
          condition;
    }

    signal.rules[
      signal.ruleCount++] =
        rule;
  }

  signals[signalCount++] = signal;

  Serial.printf(
    "Signal logic: loaded signal #%u mode=%s default=%u rules=%u.\n",
    signal.address,
    signal.extended ? "extended" : "basic",
    signal.defaultValue,
    signal.ruleCount);

  return true;
}

bool parseRulesFile(File &file)
{
  bool sawMeta = false;

  while (file.available())
  {
    while (file.available())
    {
      const int next = file.peek();

      if (next != '\r' &&
          next != '\n' &&
          next != ' ' &&
          next != '\t')
        break;

      file.read();
    }

    if (!file.available())
      break;

    JsonDocument row;

    const DeserializationError error =
      deserializeJson(row, file);

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
      const int version = row["version"] | 0;

      if (version != 1 ||
          !row["enabled"].is<bool>())
      {
        Serial.println(
          F("Signal logic: invalid meta row."));
        return false;
      }

      enabled = row["enabled"].as<bool>();
      sawMeta = true;
      continue;
    }

    if (!strcmp(kind, "signal"))
    {
      if (!parseSignalRow(
            row.as<JsonObjectConst>()))
      {
        Serial.println(
          F("Signal logic: invalid signal row."));
        return false;
      }

      continue;
    }

    Serial.printf(
      "Signal logic: unsupported row kind '%s'.\n",
      kind);
    return false;
  }

  if (!sawMeta)
  {
    Serial.println(
      F("Signal logic: meta row is missing."));
    return false;
  }

  return true;
}

bool ensureRulesFile()
{
  if (LittleFS.exists(RULES_PATH))
    return true;

  File file =
    LittleFS.open(RULES_PATH, "w");

  if (!file)
    return false;

  file.println(
    F("{\"kind\":\"meta\",\"version\":1,\"enabled\":false}"));

  file.close();
  return true;
}

} // namespace

void DCCExpressLiteSignalLogic::begin(
  TurnoutStateReader turnoutReader,
  VpinStateReader vpinReader,
  OutputWriter outputWriter)
{
  readTurnout = turnoutReader;
  readVpin = vpinReader;
  writeOutput = outputWriter;

  if (!ensureRulesFile())
  {
    Serial.println(
      F("Signal logic: could not create rules file."));
  }

  DCCExpressLiteSignalLogicApi::begin();
  reload();
}

bool DCCExpressLiteSignalLogic::reload()
{
  resetConfiguration();

  enabled = false;
  running = false;
  evaluationPending = false;

  if (!ensureRulesFile())
    return false;

  File file =
    LittleFS.open(RULES_PATH, "r");

  if (!file)
  {
    Serial.println(
      F("Signal logic: rules file could not be opened."));
    return false;
  }

  const bool loaded =
    parseRulesFile(file);

  file.close();

  if (!loaded)
  {
    resetConfiguration();
    enabled = false;
    running = false;
    return false;
  }

  running = enabled;
  evaluationPending = running;
  lastSensorPollAtMs = 0;

  Serial.printf(
    "Signal logic: %s, %u signal(s), %u sensor input(s), format=compiled-jsonl-v1.\n",
    running ? "enabled" : "disabled",
    signalCount,
    sensorInputCount);

  return true;
}

void DCCExpressLiteSignalLogic::loop()
{
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
    for (uint8_t i = 0;
         i < signalCount;
         ++i)
      signals[i].outputPending = false;

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
  return signalCount;
}

void DCCExpressLiteSignalLogic::notifyTurnout(
  uint16_t address,
  bool physicalValue)
{
  if (!running)
    return;

  Serial.printf(
    "Signal logic: turnout #%u runtime physical=%u -> evaluate.\n",
    address,
    physicalValue ? 1 : 0);

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
  for (uint8_t i = 0;
       i < signalCount;
       ++i)
  {
    signals[i].lastValueValid = false;
    signals[i].outputPending = false;
  }

  if (running)
    evaluationPending = true;
}
