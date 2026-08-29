// HTTPServer.cpp

#include "DCCEXParser.h"
#include "DCC.h"
#include "DCCExpressLite.h"
#include "DCCExpressLiteRuntimeState.h"
#include "DCCExpressLiteSignalLogic.h"
#include "HTTPSerialWrapper.h"
#include "HTTPServer.h"
#include <esp_freertos_hooks.h>
#include <vector>

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include "DIAG.h"
#include "I2CManager.h"
#include "IODevice.h"
#include "NetworkSettings.h"
#include "TrackManager.h"
#include "version.h"

AsyncWebServer httpServer(80);
AsyncWebSocket ws("/ws");
static HTTPSerialWrapper httpCommandStream(&Serial);

static const uint8_t WS_INBOUND_QUEUE_SIZE = 6;
static const size_t WS_MAX_COMMAND_BYTES = 7168;
static const uint8_t WS_MAX_TRACKED_CLIENTS = 8;
static const uint8_t WS_PENDING_CONTROL_SIZE = 24;
static const uint32_t WS_MIN_FREE_HEAP_BYTES = 80 * 1024;
static const uint32_t WS_MIN_ALLOC_BLOCK_BYTES = 12 * 1024;

enum class WsInboundKind : uint8_t { Connect, Disconnect, Data };

struct WsInboundItem
{
  volatile bool ready;
  WsInboundKind kind;
  uint32_t clientId;
  uint32_t freeHeapBytes;
  size_t length;
  char payload[WS_MAX_COMMAND_BYTES + 1];
};

struct PendingWsMessage
{
  bool used = false;
  uint32_t clientId = 0;
  String payload;
};

struct ClientSnapshotState
{
  bool active = false;
  uint32_t clientId = 0;
  uint8_t stage = 0;
  int accessoryAddress = 1;
};

static WsInboundItem wsInboundQueue[WS_INBOUND_QUEUE_SIZE] = {};
static portMUX_TYPE wsInboundMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint8_t wsInboundHead = 0;
static volatile uint8_t wsInboundTail = 0;
static volatile uint8_t wsInboundCount = 0;
static uint32_t connectedClientIds[WS_MAX_TRACKED_CLIENTS] = {};
static uint8_t connectedClientCount = 0;
static PendingWsMessage pendingWsMessages[WS_PENDING_CONTROL_SIZE];
static ClientSnapshotState clientSnapshots[WS_MAX_TRACKED_CLIENTS];
static volatile uint32_t droppedWsCommands = 0;
static uint32_t droppedWsTelemetry = 0;
static uint32_t droppedWsControl = 0;
static uint32_t droppedWsLowMemory = 0;
static uint32_t minFreeHeapBytes = UINT32_MAX;
static unsigned long lastWsCleanupAtMs = 0;
static esp_reset_reason_t bootResetReason = ESP_RST_UNKNOWN;

static const char *resetReasonName(esp_reset_reason_t reason);
static int getInt(JsonVariantConst value, int fallback = 0);
static bool getBool(JsonVariantConst value, bool fallback = false);
static void sendPowerInfo(uint32_t clientId = 0);

static bool enqueueWsInbound(WsInboundKind kind, uint32_t clientId,
                             const uint8_t *payload = nullptr, size_t length = 0)
{
  if (length > WS_MAX_COMMAND_BYTES)
  {
    ++droppedWsCommands;
    return false;
  }

  uint8_t slotIndex;
  portENTER_CRITICAL(&wsInboundMux);
  if (wsInboundCount >= WS_INBOUND_QUEUE_SIZE)
  {
    ++droppedWsCommands;
    portEXIT_CRITICAL(&wsInboundMux);
    return false;
  }
  slotIndex = wsInboundTail;
  wsInboundTail = (wsInboundTail + 1) % WS_INBOUND_QUEUE_SIZE;
  ++wsInboundCount;
  wsInboundQueue[slotIndex].ready = false;
  portEXIT_CRITICAL(&wsInboundMux);

  WsInboundItem &slot = wsInboundQueue[slotIndex];
  slot.kind = kind;
  slot.clientId = clientId;
  slot.freeHeapBytes = ESP.getFreeHeap();
  slot.length = length;
  if (payload && length)
    memcpy(slot.payload, payload, length);
  slot.payload[length] = '\0';

  portENTER_CRITICAL(&wsInboundMux);
  slot.ready = true;
  portEXIT_CRITICAL(&wsInboundMux);
  return true;
}

static WsInboundItem *peekWsInbound()
{
  WsInboundItem *item = nullptr;
  portENTER_CRITICAL(&wsInboundMux);
  if (wsInboundCount && wsInboundQueue[wsInboundHead].ready)
    item = &wsInboundQueue[wsInboundHead];
  portEXIT_CRITICAL(&wsInboundMux);
  return item;
}

static void popWsInbound()
{
  portENTER_CRITICAL(&wsInboundMux);
  if (wsInboundCount)
  {
    wsInboundQueue[wsInboundHead].ready = false;
    wsInboundHead = (wsInboundHead + 1) % WS_INBOUND_QUEUE_SIZE;
    --wsInboundCount;
  }
  portEXIT_CRITICAL(&wsInboundMux);
}

static uint8_t wsInboundLength()
{
  portENTER_CRITICAL(&wsInboundMux);
  const uint8_t length = wsInboundCount;
  portEXIT_CRITICAL(&wsInboundMux);
  return length;
}

static bool hasWsAllocationHeadroom(size_t messageLength = 0)
{
  const uint32_t requiredBlock = max(static_cast<uint32_t>(messageLength + 2048), WS_MIN_ALLOC_BLOCK_BYTES);
  return ESP.getFreeHeap() >= WS_MIN_FREE_HEAP_BYTES && ESP.getMaxAllocHeap() >= requiredBlock;
}

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
static volatile bool signalLogicReloadPending = false;
static volatile bool runtimeStatePrunePending = false;

struct ProgrammingRequestState
{
  bool active = false;
  uint32_t clientId = 0;
  String requestId;
  String action;
  int cv = -1;
  int value = -1;
  unsigned long deadlineMs = 0;
};

static ProgrammingRequestState programmingRequest;
static const unsigned long PROGRAMMING_TIMEOUT_MS = 20000;

static const int MAX_RUNTIME_LOCOS = 32;
static int locoAddresses[MAX_RUNTIME_LOCOS] = {0};
static int locoSpeeds[MAX_RUNTIME_LOCOS] = {0};
static bool locoDirectionsForward[MAX_RUNTIME_LOCOS] = {true};
static uint32_t locoFunctionBits[MAX_RUNTIME_LOCOS] = {0};
static int configuredLocoAddresses[MAX_RUNTIME_LOCOS] = {0};
static bool configuredLocoInverted[MAX_RUNTIME_LOCOS] = {false};

static const int MAX_LINEAR_ACCESSORY_ADDRESS = 2048;
static int8_t turnoutStateCache[MAX_LINEAR_ACCESSORY_ADDRESS + 1];
static int8_t basicAccessoryStateCache[MAX_LINEAR_ACCESSORY_ADDRESS + 1];

static int8_t readTurnoutStateForSignalLogic(uint16_t address)
{
  return address <= MAX_LINEAR_ACCESSORY_ADDRESS ? turnoutStateCache[address] : -1;
}

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

static bool trySendToClient(uint32_t clientId, const String &message)
{
  AsyncWebSocketClient *client = ws.client(clientId);
  if (!client || client->status() != WS_CONNECTED)
    return true;
  if (client->queueIsFull() || !ws.availableForWrite(clientId))
    return false;
  if (!hasWsAllocationHeadroom(message.length()))
    return false;
  client->text(message);
  return true;
}

static void queueControlMessage(uint32_t clientId, const String &message)
{
  if (!hasWsAllocationHeadroom(message.length()))
  {
    ++droppedWsControl;
    ++droppedWsLowMemory;
    return;
  }
  for (PendingWsMessage &pending : pendingWsMessages)
  {
    if (pending.used) continue;
    pending.used = true;
    pending.clientId = clientId;
    pending.payload = message;
    return;
  }
  ++droppedWsControl;
}

static void sendToClient(uint32_t clientId, const char *type, const String &data,
                         const char *uuid = nullptr, bool droppable = false)
{
  if (!clientId) return;
  if (!hasWsAllocationHeadroom(data.length() + 128))
  {
    if (droppable) ++droppedWsTelemetry;
    else ++droppedWsControl;
    ++droppedWsLowMemory;
    return;
  }
  const String message = makeMessage(type, data, uuid);
  if (trySendToClient(clientId, message)) return;
  if (droppable)
    ++droppedWsTelemetry;
  else
    queueControlMessage(clientId, message);
}

static void broadcastMessage(const char *type, const String &data,
                             const char *uuid = nullptr, bool droppable = false)
{
  if (!hasWsAllocationHeadroom(data.length() + 128))
  {
    if (droppable) droppedWsTelemetry += connectedClientCount;
    else droppedWsControl += connectedClientCount;
    droppedWsLowMemory += connectedClientCount;
    return;
  }
  const String message = makeMessage(type, data, uuid);

  // Telemetry is identical for every client. Use one reference-counted
  // WebSocket buffer instead of allocating a separate copy per browser.
  // This keeps periodic status traffic from multiplying heap usage as
  // clients are added. If any client is already backed up, drop this sample.
  if (droppable)
  {
    if (!ws.availableForWriteAll())
    {
      droppedWsTelemetry += connectedClientCount;
      return;
    }

    AsyncWebSocketMessageBuffer *buffer =
      ws.makeBuffer(reinterpret_cast<uint8_t *>(const_cast<char *>(message.c_str())), message.length());
    if (!buffer)
    {
      droppedWsTelemetry += connectedClientCount;
      ++droppedWsLowMemory;
      return;
    }

    ws.textAll(buffer);
    return;
  }

  for (uint8_t i = 0; i < connectedClientCount; ++i)
  {
    const uint32_t clientId = connectedClientIds[i];
    if (trySendToClient(clientId, message)) continue;
    if (droppable)
      ++droppedWsTelemetry;
    else
      queueControlMessage(clientId, message);
  }
}

static void sendError(uint32_t clientId, const String &message, const char *uuid = nullptr)
{
  sendToClient(clientId, "error", "{\"message\":\"" + escapeJson(message) + "\"}", uuid);
}

static void dccParseRaw(const String &raw)
{
  if (!raw.length())
  {
    return;
  }

  std::vector<byte> command(raw.length() + 1);
  raw.getBytes(command.data(), command.size());
  DCCEXParser::parse(&httpCommandStream, command.data(), nullptr);
}

static void writeBasicAccessoryState(uint16_t address, bool active)
{
  if (address < 1 || address > MAX_LINEAR_ACCESSORY_ADDRESS) return;
  dccParseRaw("<a " + String(address) + " " + String(active ? 1 : 0) + ">");
  basicAccessoryStateCache[address] = active ? 1 : 0;
  broadcastMessage("accessoryChanged", "{\"address\":" + String(address) + ",\"active\":" + String(active ? "true" : "false") + "}");
}

static String programmingResponseData(const String &requestId, const String &action,
                                      bool ok, const String &message,
                                      const String &raw = "", int value = -32768)
{
  String data = "{\"requestId\":\"" + escapeJson(requestId) + "\"";
  data += ",\"action\":\"" + escapeJson(action) + "\"";
  data += ",\"ok\":" + String(ok ? "true" : "false");
  data += ",\"message\":\"" + escapeJson(message) + "\"";
  if (raw.length()) data += ",\"raw\":\"" + escapeJson(raw) + "\"";
  if (value != -32768) data += ",\"value\":" + String(value);
  data += "}";
  return data;
}

static void finishProgrammingRequest(bool ok, const String &message,
                                     const String &raw = "", int value = -32768)
{
  if (!programmingRequest.active) return;
  sendToClient(programmingRequest.clientId, "programmingResponse",
    programmingResponseData(programmingRequest.requestId, programmingRequest.action,
                            ok, message, raw, value));
  dccParseRaw("<0 PROG>");
  programmingModeActive = false;
  sendPowerInfo();
  programmingRequest = ProgrammingRequestState();
}

static bool parseLastInteger(const String &raw, int &value)
{
  int end = raw.lastIndexOf('>');
  if (end < 0) end = raw.length();
  int start = end - 1;
  while (start >= 0 && raw.charAt(start) == ' ') --start;
  end = start + 1;
  while (start >= 0 && (isDigit(raw.charAt(start)) || raw.charAt(start) == '-')) --start;
  if (end <= start + 1) return false;
  value = raw.substring(start + 1, end).toInt();
  return true;
}

static void handleProgrammingOutput(const String &raw)
{
  if (!programmingRequest.active) return;

  bool matches = false;
  if (programmingRequest.action == "readAddress")
    matches = raw.startsWith("<r ");
  else if (programmingRequest.action == "writeAddress")
    matches = raw.startsWith("<w ");
  else if (programmingRequest.action == "readCv")
    matches = raw.startsWith("<v ") || raw.startsWith("<r ");
  else if (programmingRequest.action == "writeCv")
    matches = raw.startsWith("<r ");

  if (!matches) return;

  int result = -1;
  const bool parsed = parseLastInteger(raw, result);
  finishProgrammingRequest(parsed && result >= 0,
    parsed && result >= 0 ? "Programming completed." : "The decoder did not acknowledge the command.",
    raw, parsed ? result : -1);
}

static void handleProgrammingCommand(uint32_t clientId, JsonObjectConst payload,
                                     const char *uuid)
{
  const String requestId = payload["requestId"] | "";
  const String action = payload["action"] | "";
  if (!requestId.length())
  {
    sendError(clientId, "missing_programming_request_id", uuid);
    return;
  }

  if (programmingRequest.active)
  {
    sendToClient(clientId, "programmingResponse",
      programmingResponseData(requestId, action, false,
        "Another decoder programming operation is still running."), uuid);
    return;
  }

  const int address = getInt(payload["address"]);
  const int cv = getInt(payload["cv"]);
  const int value = getInt(payload["value"]);

  if (action == "pomWriteCv")
  {
    if (address < 1 || address > 10239 || cv < 1 || cv > 1024 || value < 0 || value > 255)
    {
      sendToClient(clientId, "programmingResponse",
        programmingResponseData(requestId, action, false, "Invalid locomotive address, CV or value."), uuid);
      return;
    }
    if (!trackPowerOn)
    {
      sendToClient(clientId, "programmingResponse",
        programmingResponseData(requestId, action, false, "Main track power is off."), uuid);
      return;
    }
    dccParseRaw("<w " + String(address) + " " + String(cv) + " " + String(value) + ">");
    sendToClient(clientId, "programmingResponse",
      programmingResponseData(requestId, action, true,
        "POM write command sent. POM does not provide an acknowledgement."), uuid);
    return;
  }

  if (action == "accessoryLearn")
  {
    if (address < 1 || address > 2044)
    {
      sendToClient(clientId, "programmingResponse",
        programmingResponseData(requestId, action, false, "Accessory address must be between 1 and 2044."), uuid);
      return;
    }
    if (!trackPowerOn)
    {
      sendToClient(clientId, "programmingResponse",
        programmingResponseData(requestId, action, false, "Main track power is off."), uuid);
      return;
    }
    const bool active = getBool(payload["active"]);
    writeBasicAccessoryState(address, active);
    sendToClient(clientId, "programmingResponse",
      programmingResponseData(requestId, action, true,
        "Accessory command sent to the main track."), uuid);
    return;
  }

  String command;
  if (action == "readAddress") command = "<R>";
  else if (action == "writeAddress")
  {
    if (address < 1 || address > 10239)
    {
      sendToClient(clientId, "programmingResponse",
        programmingResponseData(requestId, action, false, "Locomotive address must be between 1 and 10239."), uuid);
      return;
    }
    command = "<W " + String(address) + ">";
  }
  else if (action == "readCv")
  {
    if (cv < 1 || cv > 1024)
    {
      sendToClient(clientId, "programmingResponse",
        programmingResponseData(requestId, action, false, "CV must be between 1 and 1024."), uuid);
      return;
    }
    command = "<R " + String(cv) + ">";
  }
  else if (action == "writeCv")
  {
    if (cv < 1 || cv > 1024 || value < 0 || value > 255)
    {
      sendToClient(clientId, "programmingResponse",
        programmingResponseData(requestId, action, false, "CV must be 1-1024 and value must be 0-255."), uuid);
      return;
    }
    command = "<W " + String(cv) + " " + String(value) + ">";
  }
  else
  {
    sendToClient(clientId, "programmingResponse",
      programmingResponseData(requestId, action, false, "Unsupported programming action."), uuid);
    return;
  }

  programmingRequest.active = true;
  programmingRequest.clientId = clientId;
  programmingRequest.requestId = requestId;
  programmingRequest.action = action;
  programmingRequest.cv = cv;
  programmingRequest.value = value;
  programmingRequest.deadlineMs = millis() + PROGRAMMING_TIMEOUT_MS;
  programmingModeActive = true;
  dccParseRaw("<1 PROG>");
  dccParseRaw(command);
  sendPowerInfo();
}

static void sendDirectCommandResponse(const String &response, const char *uuid = nullptr)
{
  broadcastMessage("dccExDirectCommandResponse", "{\"response\":\"" + escapeJson(response) + "\"}", uuid);
}

static void sendCommandCenterInfo(uint32_t clientId = 0)
{
  String data = "{";
  data += "\"alive\":true";
  data += ",\"power\":";
  data += trackPowerOn ? "true" : "false";
  data += ",\"type\":\"dcc-ex-esp32-lite\"";
  data += ",\"name\":\"DCCExpressLite\"";
  data += ",\"connectionString\":\"esp32://local\"";
  data += "}";

  if (clientId)
    sendToClient(clientId, "commandCenterInfo", data);
  else
    broadcastMessage("commandCenterInfo", data);
}

static void sendPowerInfo(uint32_t clientId)
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

  if (clientId)
    sendToClient(clientId, "powerInfo", data);
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
  data += ",\"mainCurrentMa\":" + String(DCCExpressLite::getMainCurrentmA());
  data += ",\"progCurrentMa\":" + String(DCCExpressLite::getProgCurrentmA());
  data += ",\"uptimeMs\":" + String(millis());
  data += ",\"freeHeapBytes\":" + String(ESP.getFreeHeap());
  data += ",\"cpuCores\":2";
  data += ",\"cpuFrequencyMhz\":" + String(ESP.getCpuFreqMHz());
  data += ",\"cpuCore0Percent\":" + String(cpuUsagePercent[0]);
  data += ",\"cpuCore1Percent\":" + String(cpuUsagePercent[1]);
  data += ",\"chipTemperatureC\":" + String(temperatureRead(), 1);
  data += ",\"arduinoCore\":" + String(ARDUINO_RUNNING_CORE);
  data += ",\"networkCore\":" + String(CONFIG_ASYNC_TCP_RUNNING_CORE);
  data += ",\"wsClients\":" + String(connectedClientCount);
  data += ",\"wsCommandQueueLength\":" + String(wsInboundLength());
  data += ",\"droppedWsCommands\":" + String(droppedWsCommands);
  data += ",\"droppedWsTelemetry\":" + String(droppedWsTelemetry);
  data += ",\"droppedWsControl\":" + String(droppedWsControl);
  data += ",\"droppedWsLowMemory\":" + String(droppedWsLowMemory);
  data += ",\"droppedWsRawLines\":" + String(httpCommandStream.droppedLines());
  data += ",\"minimumFreeHeapBytes\":" + String(minFreeHeapBytes == UINT32_MAX ? ESP.getFreeHeap() : minFreeHeapBytes);
  data += ",\"largestFreeHeapBlockBytes\":" + String(ESP.getMaxAllocHeap());
  data += ",\"resetReason\":\"" + String(resetReasonName(bootResetReason)) + "\"";
  data += "}";
  return data;
}

static void sendDccExStatus(uint32_t clientId = 0)
{
  if (!hasWsAllocationHeadroom(1024))
  {
    if (clientId) ++droppedWsTelemetry;
    else droppedWsTelemetry += connectedClientCount;
    ++droppedWsLowMemory;
    return;
  }
  const String data = makeDccExStatusData();
  if (clientId)
    sendToClient(clientId, "dccExStatus", data);
  else
    broadcastMessage("dccExStatus", data, nullptr, true);
}

static void sendAccessorySnapshot(uint32_t clientId, int address)
{
  if (turnoutStateCache[address] >= 0)
  {
    sendToClient(clientId, "turnoutChanged",
      "{\"address\":" + String(address) +
      ",\"closed\":" + String(turnoutStateCache[address] ? "true" : "false") + "}");
  }
  if (basicAccessoryStateCache[address] >= 0)
  {
    sendToClient(clientId, "accessoryChanged",
      "{\"address\":" + String(address) +
      ",\"active\":" + String(basicAccessoryStateCache[address] ? "true" : "false") + "}");
  }
}

static void sendBlockState(uint32_t clientId = 0)
{
  const String data = DCCExpressLiteRuntimeState::blockStateJson();
  if (clientId) sendToClient(clientId, "blockStateChanged", data);
  else broadcastMessage("blockStateChanged", data);
}

static void startInitialSnapshots(uint32_t clientId)
{
  for (ClientSnapshotState &snapshot : clientSnapshots)
  {
    if (snapshot.active && snapshot.clientId != clientId) continue;
    snapshot.active = true;
    snapshot.clientId = clientId;
    snapshot.stage = 0;
    snapshot.accessoryAddress = 1;
    return;
  }
  ++droppedWsControl;
}

static void stopInitialSnapshots(uint32_t clientId)
{
  for (ClientSnapshotState &snapshot : clientSnapshots)
  {
    if (snapshot.clientId != clientId) continue;
    snapshot.active = false;
  }
}

static void processInitialSnapshots()
{
  for (ClientSnapshotState &snapshot : clientSnapshots)
  {
    if (!snapshot.active) continue;
    if (!ws.client(snapshot.clientId))
    {
      snapshot.active = false;
      continue;
    }

    switch (snapshot.stage)
    {
      case 0:
        sendToClient(snapshot.clientId, "ws:welcome", "{\"message\":\"Connected\"}");
        ++snapshot.stage;
        break;
      case 1:
        sendCommandCenterInfo(snapshot.clientId);
        ++snapshot.stage;
        break;
      case 2:
        sendPowerInfo(snapshot.clientId);
        ++snapshot.stage;
        break;
      case 3:
        sendDccExStatus(snapshot.clientId);
        ++snapshot.stage;
        break;
      case 4:
        sendToClient(snapshot.clientId, "commandCenterLockChanged", "{\"locked\":false,\"lockOwner\":null,\"reason\":null}");
        ++snapshot.stage;
        break;
      case 5:
        sendToClient(snapshot.clientId, "runtimeVariablesSnapshot", "{\"variables\":{}}");
        ++snapshot.stage;
        break;
      case 6:
        sendToClient(snapshot.clientId, "serverRuntimeStatsChanged",
          "{\"uptimeMs\":" + String(millis()) +
          ",\"wsClients\":" + String(connectedClientCount) +
          ",\"queueLength\":" + String(wsInboundLength()) + "}");
        ++snapshot.stage;
        break;
      case 7:
        sendBlockState(snapshot.clientId);
        ++snapshot.stage;
        break;
      default:
      {
        bool sent = false;
        while (snapshot.accessoryAddress <= MAX_LINEAR_ACCESSORY_ADDRESS)
        {
          const int address = snapshot.accessoryAddress++;
          if (turnoutStateCache[address] < 0 && basicAccessoryStateCache[address] < 0) continue;
          sendAccessorySnapshot(snapshot.clientId, address);
          sent = true;
          break;
        }
        if (!sent) snapshot.active = false;
        break;
      }
    }
  }
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

static int getInt(JsonVariantConst value, int fallback)
{
  return value.is<int>() ? value.as<int>() : fallback;
}

static bool getBool(JsonVariantConst value, bool fallback)
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

static void handleFileCommand(uint32_t clientId, JsonDocument &doc, const char *uuid)
{
  JsonObjectConst data = doc["data"].as<JsonObjectConst>();
  const String requestId = data["requestId"] | "";
  const String action = data["action"] | "";
  const String fileName = data["fileName"] | "";

  if (!fileName.length())
  {
    sendToClient(clientId, "fileResponse", "{\"requestId\":\"" + escapeJson(requestId) + "\",\"action\":\"" + escapeJson(action) + "\",\"ok\":false,\"message\":\"Missing fileName\"}", uuid);
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
    sendToClient(clientId, "fileResponse", response, uuid);
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
    sendToClient(clientId, "fileResponse", "{\"requestId\":\"" + escapeJson(requestId) + "\",\"action\":\"" + escapeJson(action) + "\",\"ok\":" + String(ok ? "true" : "false") + ",\"fileName\":\"" + escapeJson(fileName) + "\"}", uuid);
    return;
  }

  sendToClient(clientId, "fileResponse", "{\"requestId\":\"" + escapeJson(requestId) + "\",\"action\":\"" + escapeJson(action) + "\",\"ok\":false,\"message\":\"Unsupported file action\"}", uuid);
}

static void handleLocosCommand(uint32_t clientId, JsonDocument &doc, const char *uuid)
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

    sendToClient(clientId, "locosResponse", "{\"requestId\":\"" + escapeJson(requestId) + "\",\"action\":\"load\",\"ok\":true,\"locos\":" + content + "}", uuid);
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

    sendToClient(clientId, "locosResponse", "{\"requestId\":\"" + escapeJson(requestId) + "\",\"action\":\"save\",\"ok\":" + String(ok ? "true" : "false") + ",\"count\":" + String(locos.size()) + "}", uuid);
    return;
  }

  sendToClient(clientId, "locosResponse", "{\"requestId\":\"" + escapeJson(requestId) + "\",\"action\":\"" + escapeJson(action) + "\",\"ok\":false,\"message\":\"Unsupported locos action\"}", uuid);
}

static bool layoutTypeMatches(const char *actual, const char *expected)
{
  if (!strcmp(expected, "turnout")) return !strncmp(actual, "trackturnout", 12);
  return !strcmp(actual, expected);
}

static uint8_t countLayoutElementsById(JsonDocument &layout, const char *id, const char *expectedType)
{
  if (!id || !id[0]) return 0;
  uint8_t totalCount = 0;
  uint8_t matchingTypeCount = 0;
  for (JsonObjectConst layer : layout["layers"].as<JsonArrayConst>())
    for (JsonObjectConst element : layer["elements"].as<JsonArrayConst>())
      if (!strcmp(element["id"] | "", id))
      {
        ++totalCount;
        if (layoutTypeMatches(element["type"] | "", expectedType)) ++matchingTypeCount;
      }
  return totalCount == 1 && matchingTypeCount == 1 ? 1 : (totalCount ? 2 : 0);
}

static bool isValidLayoutBlockId(const char *blockId)
{
  File file = LittleFS.open("/layout.json", "r");
  if (!file) return false;
  JsonDocument layout;
  const DeserializationError error = deserializeJson(layout, file);
  file.close();
  return !error && countLayoutElementsById(layout, blockId, "trackblock") == 1;
}

static bool validateSignalLogicLimits(JsonVariantConst document, String &message)
{
  JsonDocument layout;
  const String layoutText = readFileText("layout.json");
  if (!layoutText.length() || deserializeJson(layout, layoutText) || !layout.is<JsonObject>())
  {
    message = "The layout cannot be loaded for integrity validation.";
    return false;
  }

  const JsonArrayConst groups = document["groups"].as<JsonArrayConst>();
  if (groups.size() > 24)
  {
    message = "A maximum of 24 signal groups is supported.";
    return false;
  }

  for (JsonObjectConst group : groups)
  {
    const char *signalId = group["signalId"] | "";
    if (countLayoutElementsById(layout, signalId, "tracksignal2") != 1)
    {
      message = "A signal rule references a missing, duplicate or invalid signal element ID.";
      return false;
    }

    const JsonArrayConst rules = group["rules"].as<JsonArrayConst>();
    if (rules.size() > 6)
    {
      message = "A maximum of 6 rules per signal is supported.";
      return false;
    }

    for (JsonObjectConst rule : rules)
    {
      const JsonArrayConst conditions = rule["conditions"].as<JsonArrayConst>();
      if (conditions.size() > 6)
      {
        message = "A maximum of 6 conditions per rule is supported.";
        return false;
      }
      for (JsonObjectConst condition : conditions)
      {
        const char *type = condition["type"] | "turnout";
        const bool sensor = !strcmp(type, "sensor");
        const char *elementId = sensor
          ? (condition["sensorId"] | "")
          : (condition["turnoutId"] | "");
        if (countLayoutElementsById(layout, elementId, sensor ? "tracksensor" : "turnout") != 1)
        {
          message = sensor
            ? "A rule references a missing, duplicate or invalid sensor element ID."
            : "A rule references a missing, duplicate or invalid turnout element ID.";
          return false;
        }
      }
    }
  }
  return true;
}

static bool persistSignalLogicDocument(JsonVariantConst document)
{
  LittleFS.remove("/signal-rules.tmp");
  File file = LittleFS.open("/signal-rules.tmp", "w");
  if (!file) return false;
  const size_t written = serializeJson(document, file);
  file.flush();
  file.close();
  if (!written)
  {
    LittleFS.remove("/signal-rules.tmp");
    return false;
  }

  LittleFS.remove("/signal-rules.json.bak");
  if (LittleFS.exists("/signal-rules.json") &&
      !LittleFS.rename("/signal-rules.json", "/signal-rules.json.bak"))
  {
    LittleFS.remove("/signal-rules.tmp");
    return false;
  }

  if (LittleFS.rename("/signal-rules.tmp", "/signal-rules.json")) return true;
  if (LittleFS.exists("/signal-rules.json.bak"))
    LittleFS.rename("/signal-rules.json.bak", "/signal-rules.json");
  LittleFS.remove("/signal-rules.tmp");
  return false;
}

static String signalLogicResponseData(const String &requestId, const String &action,
                                      bool ok, const String &message = "", bool created = false)
{
  String document = readFileText("signal-rules.json");
  if (!document.length()) document = "{\"version\":2,\"enabled\":false,\"groups\":[]}";

  String response = "{\"requestId\":\"" + escapeJson(requestId) +
    "\",\"action\":\"" + escapeJson(action) +
    "\",\"ok\":" + String(ok ? "true" : "false");
  if (message.length()) response += ",\"message\":\"" + escapeJson(message) + "\"";
  response += ",\"created\":" + String(created ? "true" : "false");
  response += ",\"document\":" + document;
  response += ",\"issues\":[]";
  response += ",\"state\":{\"running\":" + String(DCCExpressLiteSignalLogic::isRunning() ? "true" : "false") +
    ",\"enabled\":" + String(DCCExpressLiteSignalLogic::isEnabled() ? "true" : "false") + "}";
  response += "}";
  return response;
}

static void handleSignalLogicCommand(uint32_t clientId, JsonDocument &doc, const char *uuid)
{
  JsonObject data = doc["data"].as<JsonObject>();
  const String requestId = data["requestId"] | "";
  const String action = data["action"] | "";

  if (action == "load" || action == "state")
  {
    sendToClient(clientId, "signalLogicResponse",
      signalLogicResponseData(requestId, action, true), uuid);
    return;
  }

  if (action == "save")
  {
    JsonVariantConst document = data["document"];
    String validationMessage;
    if (!document.is<JsonObjectConst>() || !validateSignalLogicLimits(document, validationMessage))
    {
      sendToClient(clientId, "signalLogicResponse",
        signalLogicResponseData(requestId, action, false,
          validationMessage.length() ? validationMessage : "Missing signal logic document."), uuid);
      return;
    }

    if (!persistSignalLogicDocument(document) || !DCCExpressLiteSignalLogic::reload())
    {
      sendToClient(clientId, "signalLogicResponse",
        signalLogicResponseData(requestId, action, false, "Could not save signal logic rules."), uuid);
      return;
    }

    sendToClient(clientId, "signalLogicResponse",
      signalLogicResponseData(requestId, action, true), uuid);
    return;
  }

  if (action == "start" || action == "stop")
  {
    JsonDocument rules;
    const String current = readFileText("signal-rules.json");
    if (deserializeJson(rules, current) || !rules.is<JsonObject>())
    {
      sendToClient(clientId, "signalLogicResponse",
        signalLogicResponseData(requestId, action, false, "Signal logic rules are invalid."), uuid);
      return;
    }

    const bool nextEnabled = action == "start";
    rules["enabled"] = nextEnabled;
    if (!persistSignalLogicDocument(rules.as<JsonVariantConst>()) || !DCCExpressLiteSignalLogic::reload())
    {
      sendToClient(clientId, "signalLogicResponse",
        signalLogicResponseData(requestId, action, false, "Could not update signal logic state."), uuid);
      return;
    }

    sendToClient(clientId, "signalLogicResponse",
      signalLogicResponseData(requestId, action, true), uuid);
    return;
  }

  sendToClient(clientId, "signalLogicResponse",
    signalLogicResponseData(requestId, action, false, "Unsupported signal logic action."), uuid);
}

static void handleWsMessage(uint32_t clientId, const char *data, size_t len)
{
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data, len);

  if (error)
  {
    sendError(clientId, "invalid_json");
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
    broadcastMessage("ack", "\"" + escapeJson(raw) + "\"");
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
    if (trackPowerOn) DCCExpressLiteSignalLogic::forceEvaluate();
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
    sendToClient(clientId, "locoState", makeLocoStateData(getLocoSlot(address)), uuid);
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
      sendError(clientId, "turnout_address_out_of_range", uuid);
      return;
    }

    if (!trackPowerOn)
    {
      sendError(clientId, "track_power_off", uuid);
      sendPowerInfo(clientId);
      return;
    }

    dccParseRaw("<a " + String(address) + " " + String(closed ? 1 : 0) + ">");
    turnoutStateCache[address] = closed ? 1 : 0;
    DCCExpressLiteRuntimeState::setTurnout(address, closed);
    DCCExpressLiteSignalLogic::notifyTurnout(address, closed);
    broadcastMessage("turnoutChanged", "{\"address\":" + String(address) + ",\"closed\":" + String(closed ? "true" : "false") + "}", uuid);
    return;
  }

  if (type == "setBlock")
  {
    const String blockId = payload["blockId"] | "";
    const int locoAddress = getInt(payload["locoAddress"]);
    if (!isValidLayoutBlockId(blockId.c_str()) ||
        !DCCExpressLiteRuntimeState::setBlock(blockId.c_str(), locoAddress))
    {
      sendError(clientId, "invalid_block_or_locomotive", uuid);
      return;
    }
    sendBlockState();
    return;
  }

  if (type == "setBlockRemove")
  {
    const String blockId = payload["blockId"] | "";
    if (!isValidLayoutBlockId(blockId.c_str()) ||
        !DCCExpressLiteRuntimeState::removeBlock(blockId.c_str()))
    {
      sendError(clientId, "invalid_block", uuid);
      return;
    }
    sendBlockState();
    return;
  }

  if (type == "setBlocksReset")
  {
    DCCExpressLiteRuntimeState::resetBlocks();
    sendBlockState();
    return;
  }

  if (type == "getBlocks")
  {
    sendBlockState(clientId);
    return;
  }

  if (type == "setBasicAccessory")
  {
    const int address = getInt(payload["address"]);
    const bool active = getBool(payload["active"]);

    writeBasicAccessoryState(address, active);
    return;
  }

  if (type == "setSensor")
  {
    const int address = getInt(payload["address"]);
    const bool on = getBool(payload["on"]);

    DCCExpressLiteSignalLogic::notifySensor(address, on);
    broadcastMessage("sensorChanged", "{\"address\":" + String(address) + ",\"on\":" + String(on ? "true" : "false") + "}", uuid);
    return;
  }

  if (type == "getLayoutRuntimeSnapshot")
  {
    startInitialSnapshots(clientId);
    return;
  }

  if (type == "fileCommand")
  {
    handleFileCommand(clientId, doc, uuid);
    return;
  }

  if (type == "locosCommand")
  {
    handleLocosCommand(clientId, doc, uuid);
    return;
  }

  if (type == "signalLogicCommand")
  {
    handleSignalLogicCommand(clientId, doc, uuid);
    return;
  }

  if (type == "programmingCommand")
  {
    handleProgrammingCommand(clientId, payload, uuid);
    return;
  }

  sendError(clientId, "unknown_command: " + type, uuid);
}

void sendFormattedInfo(String s)
{
  broadcastMessage("rawInfo", "{\"raw\":\"" + escapeJson(s) + "\"}", nullptr, true);
  broadcastMessage("dccExDirectCommandResponse", "{\"response\":\"" + escapeJson(s) + "\"}");
}

static void addConnectedClient(uint32_t clientId)
{
  for (uint8_t i = 0; i < connectedClientCount; ++i)
    if (connectedClientIds[i] == clientId) return;
  if (connectedClientCount >= WS_MAX_TRACKED_CLIENTS)
  {
    ++droppedWsControl;
    return;
  }
  connectedClientIds[connectedClientCount++] = clientId;
}

static void removeConnectedClient(uint32_t clientId)
{
  for (uint8_t i = 0; i < connectedClientCount; ++i)
  {
    if (connectedClientIds[i] != clientId) continue;
    connectedClientIds[i] = connectedClientIds[connectedClientCount - 1];
    connectedClientIds[connectedClientCount - 1] = 0;
    --connectedClientCount;
    break;
  }
  stopInitialSnapshots(clientId);
  for (PendingWsMessage &pending : pendingWsMessages)
  {
    if (!pending.used || pending.clientId != clientId) continue;
    pending.used = false;
    pending.payload = "";
  }
}

static void flushPendingWsMessages()
{
  uint8_t sent = 0;
  for (PendingWsMessage &pending : pendingWsMessages)
  {
    if (!pending.used) continue;
    if (!trySendToClient(pending.clientId, pending.payload)) continue;
    pending.used = false;
    pending.payload = "";
    if (++sent >= 4) break;
  }
}

static void processWsInbound()
{
  for (uint8_t processed = 0; processed < 2; ++processed)
  {
    WsInboundItem *item = peekWsInbound();
    if (!item) return;

    if (item->kind == WsInboundKind::Connect)
    {
      addConnectedClient(item->clientId);
      const uint32_t heapAfter = ESP.getFreeHeap();
      Serial.printf("WS connect id=%lu clients=%u heapBefore=%lu heapAfter=%lu minHeap=%lu queue=%u\n",
        static_cast<unsigned long>(item->clientId), connectedClientCount,
        static_cast<unsigned long>(item->freeHeapBytes), static_cast<unsigned long>(heapAfter),
        static_cast<unsigned long>(ESP.getMinFreeHeap()), wsInboundLength());
      startInitialSnapshots(item->clientId);
    }
    else if (item->kind == WsInboundKind::Disconnect)
    {
      removeConnectedClient(item->clientId);
      Serial.printf("WS disconnect id=%lu clients=%u heap=%lu minHeap=%lu queue=%u\n",
        static_cast<unsigned long>(item->clientId), connectedClientCount,
        static_cast<unsigned long>(ESP.getFreeHeap()),
        static_cast<unsigned long>(ESP.getMinFreeHeap()), wsInboundLength());
    }
    else
    {
      if (!hasWsAllocationHeadroom(item->length + 2048))
        return;
      handleWsMessage(item->clientId, item->payload, item->length);
    }

    popWsInbound();
  }
}

static const char *resetReasonName(esp_reset_reason_t reason)
{
  switch (reason)
  {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_EXT: return "external";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt-watchdog";
    case ESP_RST_TASK_WDT: return "task-watchdog";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep-sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    default: return "unknown";
  }
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
  DCCExpressLite::appendDeviceJson(json);
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

  AsyncWebServerResponse *response = request->beginResponse(
    file, publicPath, contentType, false);
  response->addHeader("Cache-Control", "public, max-age=31536000, immutable");
  response->addHeader("Vary", "Accept-Encoding");
  response->addHeader("Connection", "close");
  request->send(response);
}

static bool normalizeFsPath(String input, String &normalized, bool allowRoot = true)
{
  input.trim();
  input.replace("\\", "/");

  if (!input.startsWith("/"))
    input = "/" + input;

  normalized = "";
  int start = 0;
  while (start < static_cast<int>(input.length()))
  {
    while (start < static_cast<int>(input.length()) && input[start] == '/')
      ++start;
    if (start >= static_cast<int>(input.length())) break;

    int end = input.indexOf('/', start);
    if (end < 0) end = input.length();
    String segment = input.substring(start, end);
    segment.trim();

    if (segment == "..") return false;
    if (segment.length() && segment != ".")
    {
      for (size_t i = 0; i < segment.length(); ++i)
      {
        if (static_cast<uint8_t>(segment[i]) < 32) return false;
      }
      normalized += "/" + segment;
    }
    start = end + 1;
  }

  if (!normalized.length()) normalized = "/";
  if (!allowRoot && normalized == "/") return false;
  return normalized.length() <= 240;
}

static String sanitizeFsFileName(String filename)
{
  filename.replace("\\", "/");
  const int slash = filename.lastIndexOf('/');
  if (slash >= 0) filename = filename.substring(slash + 1);
  filename.trim();

  if (!filename.length() || filename == "." || filename == ".." || filename.length() > 96)
    return "";

  for (size_t i = 0; i < filename.length(); ++i)
  {
    const uint8_t value = static_cast<uint8_t>(filename[i]);
    if (value < 32 || filename[i] == '/' || filename[i] == '\\') return "";
  }
  return filename;
}

static String joinFsPath(const String &directory, const String &name)
{
  return directory == "/" ? "/" + name : directory + "/" + name;
}

void setupHTTPServer()
{
  bootResetReason = esp_reset_reason();
  Serial.printf("Boot reset reason: %s (%d), freeHeap=%lu, minHeap=%lu\n",
    resetReasonName(bootResetReason), static_cast<int>(bootResetReason),
    static_cast<unsigned long>(ESP.getFreeHeap()),
    static_cast<unsigned long>(ESP.getMinFreeHeap()));

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

  if (!LittleFS.exists("/images"))
    LittleFS.mkdir("/images");

  Serial.println("LittleFS started!");
  LCD(4, F("HTTP: FS OK"));
  DCCExpressLiteRuntimeState::begin();
  for (int address = 1; address <= MAX_LINEAR_ACCESSORY_ADDRESS; ++address)
  {
    const int8_t state = DCCExpressLiteRuntimeState::getTurnout(address);
    if (state >= 0) turnoutStateCache[address] = state;
  }
  loadLocoConfiguration();
  DCCExpressLiteSignalLogic::begin(readTurnoutStateForSignalLogic, writeBasicAccessoryState);

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  httpServer.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                  request->send(204);
                });

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
                  if (renamed)
                  {
                    signalLogicReloadPending = true;
                    runtimeStatePrunePending = true;
                  }
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

  httpServer.on("/api/files/text", HTTP_GET, [](AsyncWebServerRequest *request)
                 {
                  String path;
                  if (!request->hasParam("path") ||
                      !normalizeFsPath(request->getParam("path")->value(), path, false))
                  {
                    request->send(400, "text/plain; charset=utf-8", "Invalid path.");
                    return;
                  }

                  File target = LittleFS.open(path);
                  if (!target || target.isDirectory())
                  {
                    if (target) target.close();
                    request->send(404, "text/plain; charset=utf-8", "File not found.");
                    return;
                  }
                  target.close();

                  AsyncWebServerResponse *response = request->beginResponse(
                    LittleFS, path, "text/plain; charset=utf-8", false);
                  response->addHeader("X-Content-Type-Options", "nosniff");
                  response->addHeader("Cache-Control", "no-store");
                  request->send(response);
                 });

  httpServer.on("/list", HTTP_GET, [](AsyncWebServerRequest *request)
                 {
                  String directory;
                  const String requestedPath = request->hasParam("path")
                    ? request->getParam("path")->value()
                    : "/";

                  if (!normalizeFsPath(requestedPath, directory))
                  {
                    request->send(400, "application/json", "{\"ok\":false,\"message\":\"Invalid path.\"}");
                    return;
                  }

                  File root = LittleFS.open(directory);
                  if (!root || !root.isDirectory())
                  {
                    if (root) root.close();
                    request->send(404, "application/json", "{\"ok\":false,\"message\":\"Directory not found.\"}");
                    return;
                  }

                  File file = root ? root.openNextFile() : File();

                  String json = "{\"path\":\"" + escapeJson(directory) + "\",\"entries\":[";
                  bool first = true;
                  while (file)
                  {
                    String name = String(file.name());
                    name.replace("\\", "/");
                    const int slash = name.lastIndexOf('/');
                    if (slash >= 0) name = name.substring(slash + 1);

                    if (name.length())
                    {
                      if (!first) json += ",";

                      json += "{\"name\":\"" + escapeJson(name) + "\",";
                      json += "\"path\":\"" + escapeJson(joinFsPath(directory, name)) + "\",";
                      json += "\"type\":\"" + String(file.isDirectory() ? "directory" : "file") + "\",";
                      json += "\"size\":" + String(file.isDirectory() ? 0 : file.size()) + "}";
                      first = false;
                    }

                    file.close();
                    file = root.openNextFile();
                  }

                  root.close();
                  json += "]}";
                  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  request->send(response);
                 });

  httpServer.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request)
                 {
                  String path;
                  bool valid = false;

                  if (request->hasParam("path"))
                  {
                    valid = normalizeFsPath(request->getParam("path")->value(), path, false);
                  }
                  else if (request->hasParam("fn"))
                  {
                    const String filename = sanitizeFsFileName(request->getParam("fn")->value());
                    if (filename.length())
                    {
                      path = "/images/" + filename;
                      valid = true;
                    }
                  }

                  if (!valid)
                  {
                    request->send(400, "application/json", "{\"ok\":false,\"message\":\"Invalid path.\"}");
                    return;
                  }

                  File target = LittleFS.open(path);
                  if (!target)
                  {
                    request->send(404, "application/json", "{\"ok\":false,\"message\":\"File or directory not found.\"}");
                    return;
                  }

                  const bool isDirectory = target.isDirectory();
                  target.close();
                  const bool removed = isDirectory ? LittleFS.rmdir(path) : LittleFS.remove(path);
                  AsyncWebServerResponse *response = request->beginResponse(
                    removed ? 200 : 409, "application/json",
                    removed
                      ? "{\"ok\":true}"
                      : "{\"ok\":false,\"message\":\"Directory must be empty before it can be deleted.\"}");
                  response->addHeader("Access-Control-Allow-Origin", "*");
                  request->send(response);
                 });

  httpServer.on("/upload", HTTP_POST,
                 [](AsyncWebServerRequest *request)
                 {
                  const bool ok = request->_tempObject && *static_cast<bool *>(request->_tempObject);
                  if (request->_tempObject)
                  {
                    free(request->_tempObject);
                    request->_tempObject = nullptr;
                  }
                  request->send(ok ? 200 : 500, "application/json",
                    ok ? "{\"ok\":true}" : "{\"ok\":false,\"message\":\"Upload failed.\"}");
                 },
                 [](AsyncWebServerRequest *request, String filename, size_t index,
                    uint8_t *data, size_t len, bool final)
                 {
                  if (index == 0)
                  {
                    request->_tempObject = malloc(sizeof(bool));
                    if (!request->_tempObject) return;
                    bool &uploadOk = *static_cast<bool *>(request->_tempObject);
                    uploadOk = false;

                    String directory;
                    const String requestedPath = request->hasParam("path")
                      ? request->getParam("path")->value()
                      : "/images";
                    if (!normalizeFsPath(requestedPath, directory)) return;

                    File targetDirectory = LittleFS.open(directory);
                    const bool directoryExists = targetDirectory && targetDirectory.isDirectory();
                    if (targetDirectory) targetDirectory.close();
                    if (!directoryExists) return;

                    filename = sanitizeFsFileName(filename);
                    if (!filename.length()) return;

                    request->_tempFile = LittleFS.open(joinFsPath(directory, filename), "w");
                    uploadOk = static_cast<bool>(request->_tempFile);
                  }

                  bool *uploadOk = static_cast<bool *>(request->_tempObject);
                  if (uploadOk && *uploadOk && len)
                  {
                    if (request->_tempFile.write(data, len) != len)
                      *uploadOk = false;
                  }

                  if (final)
                  {
                    if (request->_tempFile)
                    {
                      request->_tempFile.flush();
                      request->_tempFile.close();
                    }

                    if (!uploadOk || !*uploadOk)
                      Serial.println("File upload failed.");
                  }
                 });

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
                   request->send(response);
                 });

  // Keep static handlers last. Otherwise the catch-all filesystem handler
  // probes API paths (for example /fsinfo) as files before their real route,
  // causing needless LittleFS work and heap churn for every connected client.
  httpServer.serveStatic("/images/", LittleFS, "/images/")
    .setCacheControl("no-cache");

  httpServer.serveStatic("/", LittleFS, "/")
    .setDefaultFile("index.html")
    .setCacheControl("no-cache");

  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                AwsEventType type, void *arg, uint8_t *data, size_t len)
             {
               (void)server;

               if (type == WS_EVT_CONNECT)
               {
                 enqueueWsInbound(WsInboundKind::Connect, client->id());
               }
               else if (type == WS_EVT_DISCONNECT)
               {
                 enqueueWsInbound(WsInboundKind::Disconnect, client->id());
               }
               else if (type == WS_EVT_DATA)
               {
                 AwsFrameInfo *info = static_cast<AwsFrameInfo *>(arg);
                 if (!info || info->opcode != WS_TEXT || !info->final ||
                     info->index != 0 || info->len != len || len == 0)
                 {
                   portENTER_CRITICAL(&wsInboundMux);
                   ++droppedWsCommands;
                   portEXIT_CRITICAL(&wsInboundMux);
                   return;
                 }
                 enqueueWsInbound(WsInboundKind::Data, client->id(), data, len);
               }
             });

  httpServer.addHandler(&ws);
  httpServer.begin();
  Serial.println("Webserver started!");
  LCD(4, F("HTTP: OK"));
}

void loopHTTPServer()
{
  updateCpuUsage();
  const uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < minFreeHeapBytes) minFreeHeapBytes = freeHeap;

  processWsInbound();
  if (signalLogicReloadPending)
  {
    signalLogicReloadPending = false;
    DCCExpressLiteSignalLogic::reload();
  }
  if (runtimeStatePrunePending)
  {
    runtimeStatePrunePending = false;
    DCCExpressLiteRuntimeState::pruneBlocksFromLayout();
    sendBlockState();
  }
  DCCExpressLiteSignalLogic::loop();
  DCCExpressLiteRuntimeState::loop();
  flushPendingWsMessages();
  processInitialSnapshots();

  String rawLine;
  if (httpCommandStream.popLine(rawLine))
  {
    handleProgrammingOutput(rawLine);
    broadcastMessage("rawInfo", "{\"raw\":\"" + escapeJson(rawLine) + "\"}", nullptr, true);
  }

  const unsigned long now = millis();
  if (programmingRequest.active && static_cast<int32_t>(now - programmingRequest.deadlineMs) >= 0)
    finishProgrammingRequest(false, "Decoder programming timed out. Check the programming track and decoder connection.");
  if (now - lastWsCleanupAtMs >= 1000)
  {
    lastWsCleanupAtMs = now;
    ws.cleanupClients();
  }

  if (connectedClientCount > 0 && now - lastDccExStatusAtMs >= 2000)
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
