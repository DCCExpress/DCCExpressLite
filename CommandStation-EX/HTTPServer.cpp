// HTTPServer.cpp

#include "DCCEXParser.h"
#include "DCC.h"
#include "HTTPServer.h"
#include <esp_freertos_hooks.h>

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include "DIAG.h"
#include "I2CManager.h"
#include "IODevice.h"
#include "NetworkSettings.h"
#include "TrackManager.h"
#include "version.h"

AsyncWebServer httpServer(80);
AsyncWebSocket ws("/ws");

static volatile uint32_t cpuIdleCounters[2] = {0, 0};
static uint32_t cpuIdleBaseline[2] = {1, 1};
static uint32_t cpuIdlePrevious[2] = {0, 0};
static uint8_t cpuUsagePercent[2] = {0, 0};
static unsigned long lastCpuSampleAtMs = 0;

static bool cpuIdleHook0()
{
  ++cpuIdleCounters[0];
  return false;
}

static bool cpuIdleHook1()
{
  ++cpuIdleCounters[1];
  return false;
}

static void updateCpuUsage()
{
  const unsigned long now = millis();
  if (now - lastCpuSampleAtMs < 1000)
    return;

  lastCpuSampleAtMs = now;
  for (uint8_t core = 0; core < 2; ++core)
  {
    const uint32_t current = cpuIdleCounters[core];
    const uint32_t idleDelta = current - cpuIdlePrevious[core];
    cpuIdlePrevious[core] = current;
    if (idleDelta > cpuIdleBaseline[core])
      cpuIdleBaseline[core] = idleDelta;
    const uint32_t measuredIdlePercent = (idleDelta * 100ULL) / cpuIdleBaseline[core];
    const uint32_t idlePercent = measuredIdlePercent > 100 ? 100 : measuredIdlePercent;
    cpuUsagePercent[core] = 100 - idlePercent;
  }
}

static bool trackPowerOn = false;
static bool emergencyStopActive = false;
static bool programmingModeActive = false;
static unsigned long restartAtMs = 0;
static unsigned long lastDccExStatusAtMs = 0;
static File layoutUploadFile;
static bool layoutUploadOk = false;
static File locosUploadFile;
static bool locosUploadOk = false;

static const int MAX_RUNTIME_LOCOS = 32;
static int locoAddresses[MAX_RUNTIME_LOCOS] = {0};
static int locoSpeeds[MAX_RUNTIME_LOCOS] = {0};
static bool locoDirectionsForward[MAX_RUNTIME_LOCOS] = {true};
static uint32_t locoFunctionBits[MAX_RUNTIME_LOCOS] = {0};
static int configuredLocoAddresses[MAX_RUNTIME_LOCOS] = {0};
static bool configuredLocoInverted[MAX_RUNTIME_LOCOS] = {false};

static const int MAX_LINEAR_ACCESSORY_ADDRESS = 2048;
// -1 means that this server has not seen a state for the address yet.
// Turnouts and generic accessories are kept separately because the UI handles
// them through different events even when they share the same DCC address.
static int8_t turnoutStateCache[MAX_LINEAR_ACCESSORY_ADDRESS + 1];
static int8_t basicAccessoryStateCache[MAX_LINEAR_ACCESSORY_ADDRESS + 1];

static String escapeJson(const String &input)
{
  String output;
  output.reserve(input.length() + 8);

  for (unsigned int i = 0; i < input.length(); ++i)
  {
    const char c = input.charAt(i);

    if (c == '\\')
      output += "\\\\";
    else if (c == '"')
      output += "\\\"";
    else if (c == '\n')
      output += "\\n";
    else if (c == '\r')
      output += "\\r";
    else if (c == '\t')
      output += "\\t";
    else
      output += c;
  }

  return output;
}

static String makeMessage(const char *type, const String &data, const char *uuid = nullptr)
{
  String json = "{\"type\":\"";
  json += type;
  json += "\",\"data\":";
  json += data.length() ? data : "{}";

  if (uuid)
  {
    json += ",\"uuid\":\"";
    json += escapeJson(String(uuid));
    json += "\"";
  }
  else
  {
    json += ",\"uuid\":null";
  }

  json += "}";
  return json;
}

static void sendToClient(AsyncWebSocketClient *client, const char *type, const String &data, const char *uuid = nullptr)
{
  if (client && client->status() == WS_CONNECTED)
  {
    client->text(makeMessage(type, data, uuid));
  }
}

static void broadcastMessage(const char *type, const String &data, const char *uuid = nullptr)
{
  ws.textAll(makeMessage(type, data, uuid));
}

static void sendError(AsyncWebSocketClient *client, const String &message, const char *uuid = nullptr)
{
  sendToClient(client, "error", "{\"message\":\"" + escapeJson(message) + "\"}", uuid);
}

static void dccParseRaw(const String &raw)
{
  if (!raw.length())
  {
    return;
  }

  DCCEXParser::parse(raw.c_str());
}

static void sendDirectCommandResponse(const String &response, const char *uuid = nullptr)
{
  broadcastMessage("dccExDirectCommandResponse", "{\"response\":\"" + escapeJson(response) + "\"}", uuid);
}

static void sendCommandCenterInfo(AsyncWebSocketClient *client = nullptr)
{
  String data = "{";
  data += "\"alive\":true";
  data += ",\"power\":";
  data += trackPowerOn ? "true" : "false";
  data += ",\"type\":\"dcc-ex-esp32-lite\"";
  data += ",\"name\":\"DCCExpressLite\"";
  data += ",\"connectionString\":\"esp32://local\"";
  data += "}";

  if (client)
    sendToClient(client, "commandCenterInfo", data);
  else
    broadcastMessage("commandCenterInfo", data);
}

static void sendPowerInfo(AsyncWebSocketClient *client = nullptr)
{
  String data = "{";
  data += "\"emergencyStop\":";
  data += emergencyStopActive ? "true" : "false";
  data += ",\"trackVoltageOn\":";
  data += trackPowerOn ? "true" : "false";
  data += ",\"trackVoltageOff\":";
  data += trackPowerOn ? "false" : "true";
  data += ",\"shortCircuit\":false";
  data += ",\"programmingModeActive\":";
  data += programmingModeActive ? "true" : "false";
  data += "}";

  if (client)
    sendToClient(client, "powerInfo", data);
  else
    broadcastMessage("powerInfo", data);
}

static String makeDccExStatusData()
{
  String data = "{";
  data += "\"version\":\"" + escapeJson(String(VERSION)) + "\"";
  data += ",\"hardware\":\"" + escapeJson(String(DCC::getMotorShieldName())) + "\"";
  data += ",\"trackVoltageOn\":" + String(trackPowerOn ? "true" : "false");
  data += ",\"voltageMeasured\":false";
  data += ",\"trackVoltageV\":null";
  data += ",\"mainCurrentMa\":" + String(TrackManager::getMainCurrentmA());
  data += ",\"progCurrentMa\":" + String(TrackManager::getProgCurrentmA());
  data += ",\"uptimeMs\":" + String(millis());
  data += ",\"freeHeapBytes\":" + String(ESP.getFreeHeap());
  data += ",\"cpuCores\":2";
  data += ",\"cpuFrequencyMhz\":" + String(ESP.getCpuFreqMHz());
  data += ",\"cpuCore0Percent\":" + String(cpuUsagePercent[0]);
  data += ",\"cpuCore1Percent\":" + String(cpuUsagePercent[1]);
  data += ",\"chipTemperatureC\":" + String(temperatureRead(), 1);
  data += ",\"arduinoCore\":" + String(ARDUINO_RUNNING_CORE);
  data += ",\"networkCore\":" + String(CONFIG_ASYNC_TCP_RUNNING_CORE);
  data += "}";
  return data;
}

static void sendDccExStatus(AsyncWebSocketClient *client = nullptr)
{
  const String data = makeDccExStatusData();
  if (client)
    sendToClient(client, "dccExStatus", data);
  else
    broadcastMessage("dccExStatus", data);
}

static void sendAccessorySnapshots(AsyncWebSocketClient *client)
{
  for (int address = 1; address <= MAX_LINEAR_ACCESSORY_ADDRESS; ++address)
  {
    if (turnoutStateCache[address] >= 0)
    {
      sendToClient(client, "turnoutChanged",
        "{\"address\":" + String(address) +
        ",\"closed\":" + String(turnoutStateCache[address] ? "true" : "false") + "}");
    }

    if (basicAccessoryStateCache[address] >= 0)
    {
      sendToClient(client, "accessoryChanged",
        "{\"address\":" + String(address) +
        ",\"active\":" + String(basicAccessoryStateCache[address] ? "true" : "false") + "}");
    }
  }
}

static void sendInitialSnapshots(AsyncWebSocketClient *client)
{
  sendToClient(client, "ws:welcome", "{\"message\":\"Connected\"}");
  sendCommandCenterInfo(client);
  sendPowerInfo(client);
  sendDccExStatus(client);
  sendToClient(client, "commandCenterLockChanged", "{\"locked\":false,\"lockOwner\":null,\"reason\":null}");
  sendToClient(client, "runtimeVariablesSnapshot", "{\"variables\":{}}");
  sendToClient(client, "serverRuntimeStatsChanged", "{\"uptimeMs\":0,\"wsClients\":0}");
  sendAccessorySnapshots(client);
}

static int getLocoSlot(int address)
{
  int freeSlot = -1;

  for (int i = 0; i < MAX_RUNTIME_LOCOS; ++i)
  {
    if (locoAddresses[i] == address)
      return i;

    if (freeSlot < 0 && locoAddresses[i] == 0)
      freeSlot = i;
  }

  if (freeSlot >= 0)
  {
    locoAddresses[freeSlot] = address;
    locoSpeeds[freeSlot] = 0;
    locoDirectionsForward[freeSlot] = true;
    locoFunctionBits[freeSlot] = 0;
    return freeSlot;
  }

  locoAddresses[0] = address;
  locoSpeeds[0] = 0;
  locoDirectionsForward[0] = true;
  locoFunctionBits[0] = 0;
  return 0;
}

static String makeLocoStateData(int slot)
{
  String functions = "{";
  bool first = true;

  for (int fn = 0; fn < 32; ++fn)
  {
    if ((locoFunctionBits[slot] & (1UL << fn)) == 0)
      continue;

    if (!first)
      functions += ",";

    functions += "\"";
    functions += String(fn);
    functions += "\":true";
    first = false;
  }

  functions += "}";

  String data = "{\"loco\":{\"address\":";
  data += String(locoAddresses[slot]);
  data += ",\"speed\":";
  data += String(locoSpeeds[slot]);
  data += ",\"direction\":\"";
  data += locoDirectionsForward[slot] ? "forward" : "reverse";
  data += "\",\"functions\":";
  data += functions;
  data += "}}";

  return data;
}

static int getInt(JsonVariantConst value, int fallback = 0)
{
  return value.is<int>() ? value.as<int>() : fallback;
}

static bool getBool(JsonVariantConst value, bool fallback = false)
{
  return value.is<bool>() ? value.as<bool>() : fallback;
}

static String readFileText(const String &fileName)
{
  String path = fileName.startsWith("/") ? fileName : "/" + fileName;
  File file = LittleFS.open(path, "r");

  if (!file)
  {
    return "";
  }

  String content = file.readString();
  file.close();
  return content;
}

static bool writeFileText(const String &fileName, const String &content)
{
  String path = fileName.startsWith("/") ? fileName : "/" + fileName;
  File file = LittleFS.open(path, "w");

  if (!file)
  {
    return false;
  }

  file.print(content);
  file.close();
  return true;
}

static void loadLocoConfiguration()
{
  memset(configuredLocoAddresses, 0, sizeof(configuredLocoAddresses));
  memset(configuredLocoInverted, 0, sizeof(configuredLocoInverted));

  File file = LittleFS.open("/locos.json", "r");
  if (!file)
    return;

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error || !doc.is<JsonArray>())
  {
    Serial.println("Could not load locomotive direction configuration.");
    return;
  }

  int slot = 0;
  for (JsonObjectConst loco : doc.as<JsonArrayConst>())
  {
    if (slot >= MAX_RUNTIME_LOCOS)
      break;

    const int address = loco["address"] | 0;
    if (address <= 0)
      continue;

    configuredLocoAddresses[slot] = address;
    configuredLocoInverted[slot] = loco["invert"] | false;
    ++slot;
  }

  Serial.printf("Loaded %d locomotive direction settings.\n", slot);
}

static bool isLocoDirectionInverted(int address)
{
  for (int i = 0; i < MAX_RUNTIME_LOCOS; ++i)
  {
    if (configuredLocoAddresses[i] == address)
      return configuredLocoInverted[i];
  }

  return false;
}

static void handleFileCommand(AsyncWebSocketClient *client, JsonDocument &doc, const char *uuid)
{
  JsonObjectConst data = doc["data"].as<JsonObjectConst>();
  const String requestId = data["requestId"] | "";
  const String action = data["action"] | "";
  const String fileName = data["fileName"] | "";

  if (!fileName.length())
  {
    sendToClient(client, "fileResponse", "{\"requestId\":\"" + escapeJson(requestId) + "\",\"action\":\"" + escapeJson(action) + "\",\"ok\":false,\"message\":\"Missing fileName\"}", uuid);
    return;
  }

  if (action == "readText" || action == "readJson")
  {
    const String content = readFileText(fileName);
    String response = "{\"requestId\":\"" + escapeJson(requestId) + "\",\"action\":\"" + escapeJson(action) + "\",\"ok\":true,\"fileName\":\"" + escapeJson(fileName) + "\"";

    if (action == "readJson" && content.length())
      response += ",\"data\":" + content;
    else
      response += ",\"content\":\"" + escapeJson(content) + "\"";

    response += "}";
    sendToClient(client, "fileResponse", response, uuid);
    return;
  }

  if (action == "writeText" || action == "writeJson")
  {
    String content;

    if (action == "writeJson")
      serializeJson(data["data"], content);
    else
      content = data["content"] | "";

    const bool ok = writeFileText(fileName, content);
    sendToClient(client, "fileResponse", "{\"requestId\":\"" + escapeJson(requestId) + "\",\"action\":\"" + escapeJson(action) + "\",\"ok\":" + String(ok ? "true" : "false") + ",\"fileName\":\"" + escapeJson(fileName) + "\"}", uuid);
    return;
  }

  sendToClient(client, "fileResponse", "{\"requestId\":\"" + escapeJson(requestId) + "\",\"action\":\"" + escapeJson(action) + "\",\"ok\":false,\"message\":\"Unsupported file action\"}", uuid);
}

static void handleLocosCommand(AsyncWebSocketClient *client, JsonDocument &doc, const char *uuid)
{
  JsonObjectConst data = doc["data"].as<JsonObjectConst>();
  const String requestId = data["requestId"] | "";
  const String action = data["action"] | "";

  if (action == "load")
  {
    String content = readFileText("locos.json");

    if (!content.length())
    {
      content = "[]";
    }

    sendToClient(client, "locosResponse", "{\"requestId\":\"" + escapeJson(requestId) + "\",\"action\":\"load\",\"ok\":true,\"locos\":" + content + "}", uuid);
    return;
  }

  if (action == "save")
  {
    String content;
    serializeJson(data["locos"], content);
    const bool ok = writeFileText("locos.json", content.length() ? content : "[]");
    const JsonArrayConst locos = data["locos"].as<JsonArrayConst>();

    if (ok)
      loadLocoConfiguration();

    sendToClient(client, "locosResponse", "{\"requestId\":\"" + escapeJson(requestId) + "\",\"action\":\"save\",\"ok\":" + String(ok ? "true" : "false") + ",\"count\":" + String(locos.size()) + "}", uuid);
    return;
  }

  sendToClient(client, "locosResponse", "{\"requestId\":\"" + escapeJson(requestId) + "\",\"action\":\"" + escapeJson(action) + "\",\"ok\":false,\"message\":\"Unsupported locos action\"}", uuid);
}

static void handleWsMessage(AsyncWebSocket *server, AsyncWebSocketClient *client, uint8_t *data, size_t len)
{
  String msg;
  msg.reserve(len + 1);

  for (size_t i = 0; i < len; ++i)
  {
    msg += (char)data[i];
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, msg);

  if (error)
  {
    sendError(client, "invalid_json");
    return;
  }

  const String type = doc["type"] | "";
  const String uuidString = doc["uuid"] | "";
  const char *uuid = uuidString.length() ? uuidString.c_str() : nullptr;
  JsonObjectConst payload = doc["data"].as<JsonObjectConst>();

  if (type == "dccexraw")
  {
    const String raw = payload["raw"] | "";
    dccParseRaw(raw);
    server->textAll("{\"type\":\"ack\",\"data\":\"" + escapeJson(raw) + "\"}");
    sendDirectCommandResponse(raw, uuid);
    return;
  }

  if (type == "writeDccExDirectCommand")
  {
    const String command = payload["command"] | "";
    dccParseRaw(command);
    sendDirectCommandResponse(command, uuid);
    return;
  }

  if (type == "setTrackPower")
  {
    trackPowerOn = getBool(payload["on"]);
    emergencyStopActive = false;
    dccParseRaw(trackPowerOn ? "<1 MAIN>" : "<0>");
    sendPowerInfo();
    sendCommandCenterInfo();
    return;
  }

  if (type == "setProgrammingPower")
  {
    programmingModeActive = getBool(payload["on"]);
    dccParseRaw(programmingModeActive ? "<1 PROG>" : "<0 PROG>");
    sendPowerInfo();
    return;
  }

  if (type == "emergencyStop")
  {
    emergencyStopActive = true;
    dccParseRaw("<!>");

    for (int i = 0; i < MAX_RUNTIME_LOCOS; ++i)
    {
      if (locoAddresses[i] == 0)
        continue;

      locoSpeeds[i] = 0;
      broadcastMessage("locoState", makeLocoStateData(i), uuid);
    }

    sendPowerInfo();
    return;
  }

  if (type == "setLoco")
  {
    const int address = getInt(payload["locoAddress"]);
    const int speed = constrain(getInt(payload["speed"]), 0, 126);
    const String direction = payload["direction"] | "forward";
    const bool logicalDirectionForward = direction != "reverse";
    const bool dccDirectionForward = isLocoDirectionInverted(address)
      ? !logicalDirectionForward
      : logicalDirectionForward;
    const int dccDirection = dccDirectionForward ? 1 : 0;

    dccParseRaw("<t " + String(address) + " " + String(speed) + " " + String(dccDirection) + ">");
    emergencyStopActive = false;

    const int slot = getLocoSlot(address);
    locoSpeeds[slot] = speed;
    locoDirectionsForward[slot] = logicalDirectionForward;
    broadcastMessage("locoState", makeLocoStateData(slot), uuid);
    sendPowerInfo();
    return;
  }

  if (type == "getLoco")
  {
    const int address = getInt(payload["locoAddress"]);
    dccParseRaw("<t " + String(address) + ">");
    sendToClient(client, "locoState", makeLocoStateData(getLocoSlot(address)), uuid);
    return;
  }

  if (type == "setLocoFunction")
  {
    const int address = getInt(payload["locoAddress"]);
    const int fn = getInt(payload["functionNumber"]);
    const bool active = getBool(payload["active"]);

    dccParseRaw("<F " + String(address) + " " + String(fn) + " " + String(active ? 1 : 0) + ">");
    const int slot = getLocoSlot(address);

    if (fn >= 0 && fn < 32)
    {
      if (active)
        locoFunctionBits[slot] |= (1UL << fn);
      else
        locoFunctionBits[slot] &= ~(1UL << fn);
    }

    broadcastMessage("locoState", makeLocoStateData(slot), uuid);
    return;
  }

  if (type == "setTurnout")
  {
    const int address = getInt(payload["address"]);
    const bool closed = getBool(payload["closed"]);

    if (address < 1 || address > 2048)
    {
      sendError(client, "turnout_address_out_of_range", uuid);
      return;
    }

    if (!trackPowerOn)
    {
      sendError(client, "track_power_off", uuid);
      sendPowerInfo(client);
      return;
    }

    // Layout turnouts use a linear DCC accessory address directly. The state
    // selects the decoder output/gate; DCC-EX sends the activation pulse and
    // its matching deactivation packet for <a LINEARADDRESS ACTIVATE>.
    dccParseRaw("<a " + String(address) + " " + String(closed ? 1 : 0) + ">");
    turnoutStateCache[address] = closed ? 1 : 0;
    broadcastMessage("turnoutChanged", "{\"address\":" + String(address) + ",\"closed\":" + String(closed ? "true" : "false") + "}", uuid);
    return;
  }

  if (type == "setBasicAccessory")
  {
    const int address = getInt(payload["address"]);
    const bool active = getBool(payload["active"]);

    dccParseRaw("<a " + String(address) + " " + String(active ? 1 : 0) + ">");
    if (address >= 1 && address <= MAX_LINEAR_ACCESSORY_ADDRESS)
      basicAccessoryStateCache[address] = active ? 1 : 0;
    broadcastMessage("accessoryChanged", "{\"address\":" + String(address) + ",\"active\":" + String(active ? "true" : "false") + "}", uuid);
    return;
  }

  if (type == "setSensor")
  {
    const int address = getInt(payload["address"]);
    const bool on = getBool(payload["on"]);

    broadcastMessage("sensorChanged", "{\"address\":" + String(address) + ",\"on\":" + String(on ? "true" : "false") + "}", uuid);
    return;
  }

  if (type == "getLayoutRuntimeSnapshot")
  {
    sendCommandCenterInfo(client);
    sendPowerInfo(client);
    sendDccExStatus(client);
    sendAccessorySnapshots(client);
    return;
  }

  if (type == "fileCommand")
  {
    handleFileCommand(client, doc, uuid);
    return;
  }

  if (type == "locosCommand")
  {
    handleLocosCommand(client, doc, uuid);
    return;
  }

  sendError(client, "unknown_command: " + type, uuid);
}

void sendFormattedInfo(String s)
{
  broadcastMessage("rawInfo", "{\"raw\":\"" + escapeJson(s) + "\"}");
  broadcastMessage("dccExDirectCommandResponse", "{\"response\":\"" + escapeJson(s) + "\"}");
}

static void scheduleRestart()
{
  restartAtMs = millis() + 2000;
}

static String networkSettingsJson()
{
  const wifi_mode_t wifiMode = WiFi.getMode();
  const bool apMode = wifiMode == WIFI_MODE_AP || wifiMode == WIFI_MODE_APSTA;
  const String currentIp = (apMode ? WiFi.softAPIP() : WiFi.localIP()).toString();

  String json = "{";
  json += "\"configured\":";
  json += NetworkSettings::hasStationConfig() ? "true" : "false";
  json += ",\"ssid\":\"" + escapeJson(NetworkSettings::ssid()) + "\"";
  json += ",\"hasPassword\":";
  json += NetworkSettings::hasStationConfig() ? "true" : "false";
  json += ",\"hostname\":\"dccex\"";
  json += ",\"mode\":\"";
  json += apMode ? "access-point" : "station";
  json += "\"";
  json += ",\"currentIp\":\"" + escapeJson(currentIp) + "\"";
  json += ",\"connectionUrl\":\"http://" + escapeJson(currentIp) + "\"";
  json += ",\"restartPending\":";
  json += restartAtMs ? "true" : "false";
  json += "}";
  return json;
}

static const char *i2cDeviceTypeGuess(uint8_t address)
{
  if (address == 0x1a) return "Piicodev radio transceiver";
  if (address == 0x1c) return "QMC6310 magnetometer";
  if (address >= 0x20 && address <= 0x26) return "GPIO expander";
  if (address == 0x27) return "GPIO expander or LCD display";
  if (address == 0x29) return "Time-of-flight sensor";
  if (address == 0x34) return "TCA8418 keypad scanner";
  if (address >= 0x3c && address <= 0x3d) return "OLED display or magnetometer";
  if (address >= 0x40 && address <= 0x47) return "PWM / servo controller";
  if (address >= 0x48 && address <= 0x4f) return "Analogue input, PWM, or UART";
  if (address >= 0x50 && address <= 0x5f) return "EEPROM or UART";
  if (address >= 0x60 && address <= 0x67) return "NeoPixel driver";
  if (address == 0x68) return "Real-time clock or NeoPixel driver";
  if (address >= 0x70 && address <= 0x77) return "I2C multiplexer";
  return "Unknown I2C device";
}

static String hexI2CAddress(uint8_t address)
{
  const char digits[] = "0123456789ABCDEF";
  String result = "0x";
  result += digits[(address >> 4) & 0x0f];
  result += digits[address & 0x0f];
  return result;
}

static String hardwareDevicesJson()
{
  String json;
  json.reserve(2400);
  json += "{\"scannedAtMs\":" + String(millis());
  json += ",\"configuredDevices\":";
  IODevice::appendDeviceJson(json);
  json += ",\"i2cDevices\":[";

  bool first = true;
  for (uint8_t address = 0x08; address < 0x78; ++address)
  {
    if (!I2CManager.exists(I2CAddress(address))) continue;
    if (!first) json += ',';
    first = false;
    json += "{\"address\":" + String(address);
    json += ",\"addressHex\":\"" + hexI2CAddress(address) + "\"";
    json += ",\"typeGuess\":\"";
    json += i2cDeviceTypeGuess(address);
    json += "\",\"detected\":true}";
  }

  json += "]}";
  return json;
}

static void sendIndex(AsyncWebServerRequest *request)
{
  AsyncWebServerResponse *response = request->beginResponse(
    LittleFS, "/index.html", "text/html; charset=utf-8", false);
  response->addHeader("Cache-Control", "no-store");
  response->addHeader("Connection", "close");
  request->send(response);
}

static void sendCompressedAsset(AsyncWebServerRequest *request,
                                const char *compressedPath,
                                const char *publicPath,
                                const char *contentType)
{
  File file = LittleFS.open(compressedPath, "r");

  if (!file)
  {
    request->send(404, "text/plain", "Asset not found");
    return;
  }

  // publicPath deliberately has no .gz suffix. AsyncFileResponse therefore
  // emits Content-Encoding: gzip while retaining the explicit browser MIME.
  AsyncWebServerResponse *response = request->beginResponse(
    file, publicPath, contentType, false);
  // The generated index uses a content hash query parameter. Assets can stay
  // cached indefinitely while every changed build receives a new URL.
  response->addHeader("Cache-Control", "public, max-age=31536000, immutable");
  response->addHeader("Vary", "Accept-Encoding");
  response->addHeader("Connection", "close");
  request->send(response);
}

void setupHTTPServer()
{
  memset(turnoutStateCache, -1, sizeof(turnoutStateCache));
  memset(basicAccessoryStateCache, -1, sizeof(basicAccessoryStateCache));

  esp_register_freertos_idle_hook_for_cpu(cpuIdleHook0, 0);
  esp_register_freertos_idle_hook_for_cpu(cpuIdleHook1, 1);
  delay(250);
  for (uint8_t core = 0; core < 2; ++core)
  {
    cpuIdleBaseline[core] = cpuIdleCounters[core] ? cpuIdleCounters[core] * 4 : 1;
    cpuIdlePrevious[core] = cpuIdleCounters[core];
  }
  lastCpuSampleAtMs = millis();

  if (!LittleFS.begin(true))
  {
    Serial.println("LittleFS mount error!");
    return;
  }

  Serial.println("LittleFS started!");
  LCD(4, F("HTTP: FS OK"));
  loadLocoConfiguration();

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  httpServer.on("/api/settings/network", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  request->send(200, "application/json", networkSettingsJson());
                });

  httpServer.on("/api/devices", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  request->send(200, "application/json", hardwareDevicesJson());
                });

  httpServer.on("/api/settings/network", HTTP_POST, [](AsyncWebServerRequest *request)
                {
                  if (!request->hasParam("ssid", true))
                  {
                    request->send(400, "application/json", "{\"ok\":false,\"message\":\"Missing SSID.\"}");
                    return;
                  }

                  String ssid = request->getParam("ssid", true)->value();
                  ssid.trim();
                  String password = request->hasParam("password", true)
                    ? request->getParam("password", true)->value()
                    : "";

                  if (!password.length() && NetworkSettings::hasStationConfig() && ssid == NetworkSettings::ssid())
                    password = NetworkSettings::password();

                  String error;

                  if (!NetworkSettings::save(ssid, password, error))
                  {
                    request->send(400, "application/json", "{\"ok\":false,\"message\":\"" + escapeJson(error) + "\"}");
                    return;
                  }

                  request->send(200, "application/json", "{\"ok\":true,\"message\":\"Network saved. Device is restarting.\"}");
                  scheduleRestart();
                });

  httpServer.on("/api/settings/network/reset", HTTP_POST, [](AsyncWebServerRequest *request)
                {
                  String error;

                  if (!NetworkSettings::clear(error))
                  {
                    request->send(500, "application/json", "{\"ok\":false,\"message\":\"" + escapeJson(error) + "\"}");
                    return;
                  }

                  request->send(200, "application/json", "{\"ok\":true,\"message\":\"Network cleared. Device is restarting in hotspot mode.\"}");
                  scheduleRestart();
                });

  httpServer.on("/api/layout", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  if (LittleFS.exists("/layout.json"))
                  {
                    request->send(LittleFS, "/layout.json", "application/json; charset=utf-8", false);
                    return;
                  }

                  request->send(200, "application/json; charset=utf-8",
                    "{\"gridSize\":40,\"layers\":[]}");
                });

  httpServer.on("/api/layout", HTTP_POST,
                [](AsyncWebServerRequest *request)
                {
                  if (layoutUploadFile)
                    layoutUploadFile.close();

                  if (!layoutUploadOk)
                  {
                    LittleFS.remove("/layout.tmp");
                    request->send(500, "application/json", "{\"ok\":false,\"message\":\"Layout write failed.\"}");
                    return;
                  }

                  LittleFS.remove("/layout.json");
                  const bool renamed = LittleFS.rename("/layout.tmp", "/layout.json");
                  layoutUploadOk = false;
                  request->send(renamed ? 200 : 500, "application/json",
                    renamed
                      ? "{\"ok\":true,\"message\":\"Layout saved.\"}"
                      : "{\"ok\":false,\"message\":\"Could not commit layout.\"}");
                },
                nullptr,
                [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                {
                  (void)request;

                  if (index == 0)
                  {
                    if (layoutUploadFile)
                      layoutUploadFile.close();

                    LittleFS.remove("/layout.tmp");
                    layoutUploadFile = LittleFS.open("/layout.tmp", "w");
                    layoutUploadOk = (bool)layoutUploadFile;
                  }

                  if (layoutUploadOk && len && layoutUploadFile.write(data, len) != len)
                    layoutUploadOk = false;

                  if (index + len == total)
                  {
                    if (layoutUploadFile)
                    {
                      layoutUploadFile.flush();
                      layoutUploadFile.close();
                    }
                  }
                });

  httpServer.on("/api/locos", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  if (LittleFS.exists("/locos.json"))
                  {
                    request->send(LittleFS, "/locos.json", "application/json; charset=utf-8", false);
                    return;
                  }

                  request->send(200, "application/json; charset=utf-8", "[]");
                });

  httpServer.on("/api/locos", HTTP_POST,
                [](AsyncWebServerRequest *request)
                {
                  if (locosUploadFile)
                    locosUploadFile.close();

                  if (!locosUploadOk)
                  {
                    LittleFS.remove("/locos.tmp");
                    request->send(500, "application/json", "{\"ok\":false,\"message\":\"Locomotive write failed.\"}");
                    return;
                  }

                  LittleFS.remove("/locos.json");
                  const bool renamed = LittleFS.rename("/locos.tmp", "/locos.json");
                  locosUploadOk = false;

                  if (renamed)
                    loadLocoConfiguration();

                  request->send(renamed ? 200 : 500, "application/json",
                    renamed
                      ? "{\"ok\":true,\"message\":\"Locomotives saved.\"}"
                      : "{\"ok\":false,\"message\":\"Could not commit locomotives.\"}");
                },
                nullptr,
                [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                {
                  (void)request;

                  if (index == 0)
                  {
                    if (locosUploadFile)
                      locosUploadFile.close();

                    LittleFS.remove("/locos.tmp");
                    locosUploadFile = LittleFS.open("/locos.tmp", "w");
                    locosUploadOk = (bool)locosUploadFile;
                  }

                  if (locosUploadOk && len && locosUploadFile.write(data, len) != len)
                    locosUploadOk = false;

                  if (index + len == total && locosUploadFile)
                  {
                    locosUploadFile.flush();
                    locosUploadFile.close();
                  }
                });

  httpServer.on("/", HTTP_GET, sendIndex);
  httpServer.on("/index.html", HTTP_GET, sendIndex);
  httpServer.on("/assets/app-v2.js", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  sendCompressedAsset(request, "/assets/app-v2.js.gz", "/assets/app-v2.js", "application/javascript; charset=utf-8");
                });
  httpServer.on("/assets/index-v2.css", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  sendCompressedAsset(request, "/assets/index-v2.css.gz", "/assets/index-v2.css", "text/css; charset=utf-8");
                });
  httpServer.serveStatic("/", LittleFS, "/")
    .setDefaultFile("index.html")
    .setCacheControl("no-cache");

  httpServer.on("/list", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  File root = LittleFS.open("/");
                  File file = root.openNextFile();

                  String json = "[";
                  bool first = true;
                  while (file)
                  {
                    if (!first)
                      json += ",";
                    json += "{\"name\":\"" + String(file.name()) + "\",";
                    json += "\"size\":" + String(file.size()) + "}";
                    file = root.openNextFile();
                    first = false;
                  }
                  json += "]";
                  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  request->send(response); });

  httpServer.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  if (request->hasParam("fn"))
                  {
                    String path = "/" + request->getParam("fn")->value();
                    LittleFS.remove(path);
                  }
                  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  request->send(response); });

  httpServer.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request)
                { request->send(200, "text/plain", "OK"); }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
                {
                  static File uploadFile;
                  if (!index)
                    uploadFile = LittleFS.open("/" + filename, "w");
                  if (uploadFile)
                    uploadFile.write(data, len);
                  if (final && uploadFile)
                    uploadFile.close(); });

  httpServer.on("/fsinfo", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  size_t total = LittleFS.totalBytes();
                  size_t used = LittleFS.usedBytes();
                  const esp_partition_t *runningPartition = esp_ota_get_running_partition();
                  const size_t firmwarePartitionBytes = runningPartition ? runningPartition->size : 0;
                  const size_t otaPartitionBytes = ESP.getFreeSketchSpace();
                  const size_t flashChipBytes = ESP.getFlashChipSize();
                  const size_t systemReservedBytes = flashChipBytes > firmwarePartitionBytes + otaPartitionBytes + total
                    ? flashChipBytes - firmwarePartitionBytes - otaPartitionBytes - total
                    : 0;

                  String json = "{";
                  json += "\"total\":" + String(total / 1024);
                  json += ",\"used\":" + String(used / 1024);
                  json += ",\"free\":" + String((total - used) / 1024);
                  json += ",\"totalBytes\":" + String(total);
                  json += ",\"usedBytes\":" + String(used);
                  json += ",\"freeBytes\":" + String(total - used);
                  json += ",\"flashChipBytes\":" + String(flashChipBytes);
                  json += ",\"firmwareBytes\":" + String(ESP.getSketchSize());
                  json += ",\"firmwarePartitionBytes\":" + String(firmwarePartitionBytes);
                  json += ",\"otaPartitionBytes\":" + String(otaPartitionBytes);
                  json += ",\"systemReservedBytes\":" + String(systemReservedBytes);
                  json += "}";
                  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  request->send(response); });

  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                AwsEventType type, void *arg, uint8_t *data, size_t len)
             {
               (void)arg;

               if (type == WS_EVT_CONNECT)
               {
                 Serial.println("WebSocket connect");
                 sendInitialSnapshots(client);
               }
               else if (type == WS_EVT_DISCONNECT)
               {
                 Serial.println("WebSocket disconnect");
               }
               else if (type == WS_EVT_DATA)
               {
                 handleWsMessage(server, client, data, len);
               } });

  httpServer.addHandler(&ws);
  httpServer.begin();
  Serial.println("Webserver started!");
  LCD(4, F("HTTP: OK"));
}

void loopHTTPServer()
{
  updateCpuUsage();
  ws.cleanupClients();

  const unsigned long now = millis();
  if (ws.count() > 0 && now - lastDccExStatusAtMs >= 1000)
  {
    lastDccExStatusAtMs = now;
    sendDccExStatus();
  }

  if (restartAtMs && static_cast<int32_t>(now - restartAtMs) >= 0)
  {
    restartAtMs = 0;
    ESP.restart();
  }
}
