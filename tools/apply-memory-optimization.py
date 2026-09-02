#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
PENDING: dict[Path, str] = {}

def replace_once(path: Path, old: str, new: str, label: str) -> None:
    text = PENDING.get(path)
    if text is None:
        text = path.read_text(encoding="utf-8")

    count = text.count(old)
    if count != 1:
        raise RuntimeError(
            f"{label}: expected exactly one source block in {path}, found {count}"
        )

    PENDING[path] = text.replace(old, new, 1)

def main() -> int:
    http = ROOT / "CommandStation-EX" / "HTTPServer.cpp"
    ws_api = ROOT / "web-ui" / "src" / "services" / "wsApi.ts"
    ws_client = ROOT / "web-ui" / "src" / "services" / "wsClient.ts"
    ws_types = ROOT / "web-ui" / "src" / "domain" / "wsTypes.ts"

    for path in (http, ws_api, ws_client, ws_types):
        if not path.exists():
            raise RuntimeError(f"Missing expected file: {path}")

    replace_once(
        http,
'''struct WsInboundItem
{
  volatile bool ready;
  WsInboundKind kind;
  uint32_t clientId;
  uint32_t freeHeapBytes;
  size_t length;
  char payload[WS_MAX_COMMAND_BYTES + 1];
};''',
'''struct WsInboundItem
{
  volatile bool ready;
  WsInboundKind kind;
  uint32_t clientId;
  uint32_t freeHeapBytes;
  size_t length;

  // Allocate only the actual inbound message size.
  // The old fixed char[7169] in every one of the 6 queue slots consumed
  // roughly 43 KB of DRAM even while the WebSocket was idle.
  char *payload = nullptr;
};''',
        "dynamic WS queue struct",
    )

    replace_once(
        http,
'''static bool enqueueWsInbound(WsInboundKind kind, uint32_t clientId,
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
  slot.payload[length] = '\\0';

  portENTER_CRITICAL(&wsInboundMux);
  slot.ready = true;
  portEXIT_CRITICAL(&wsInboundMux);
  return true;
}''',
'''static bool enqueueWsInbound(WsInboundKind kind, uint32_t clientId,
                             const uint8_t *payload = nullptr, size_t length = 0)
{
  if (length > WS_MAX_COMMAND_BYTES)
  {
    ++droppedWsCommands;
    return false;
  }

  char *payloadCopy = nullptr;

  if (payload && length)
  {
    payloadCopy = static_cast<char *>(malloc(length + 1));

    if (!payloadCopy)
    {
      ++droppedWsCommands;
      ++droppedWsLowMemory;
      return false;
    }

    memcpy(payloadCopy, payload, length);
    payloadCopy[length] = '\\0';
  }

  uint8_t slotIndex;

  portENTER_CRITICAL(&wsInboundMux);

  if (wsInboundCount >= WS_INBOUND_QUEUE_SIZE)
  {
    ++droppedWsCommands;
    portEXIT_CRITICAL(&wsInboundMux);

    if (payloadCopy)
      free(payloadCopy);

    return false;
  }

  slotIndex = wsInboundTail;
  wsInboundTail = (wsInboundTail + 1) % WS_INBOUND_QUEUE_SIZE;
  ++wsInboundCount;
  wsInboundQueue[slotIndex].ready = false;

  portEXIT_CRITICAL(&wsInboundMux);

  WsInboundItem &slot = wsInboundQueue[slotIndex];

  if (slot.payload)
  {
    free(slot.payload);
    slot.payload = nullptr;
  }

  slot.kind = kind;
  slot.clientId = clientId;
  slot.freeHeapBytes = ESP.getFreeHeap();
  slot.length = length;
  slot.payload = payloadCopy;

  portENTER_CRITICAL(&wsInboundMux);
  slot.ready = true;
  portEXIT_CRITICAL(&wsInboundMux);

  return true;
}''',
        "dynamic WS enqueue",
    )

    replace_once(
        http,
'''static void popWsInbound()
{
  portENTER_CRITICAL(&wsInboundMux);
  if (wsInboundCount)
  {
    wsInboundQueue[wsInboundHead].ready = false;
    wsInboundHead = (wsInboundHead + 1) % WS_INBOUND_QUEUE_SIZE;
    --wsInboundCount;
  }
  portEXIT_CRITICAL(&wsInboundMux);
}''',
'''static void popWsInbound()
{
  char *payloadToFree = nullptr;

  portENTER_CRITICAL(&wsInboundMux);

  if (wsInboundCount)
  {
    WsInboundItem &slot = wsInboundQueue[wsInboundHead];

    payloadToFree = slot.payload;
    slot.payload = nullptr;
    slot.length = 0;
    slot.ready = false;

    wsInboundHead = (wsInboundHead + 1) % WS_INBOUND_QUEUE_SIZE;
    --wsInboundCount;
  }

  portEXIT_CRITICAL(&wsInboundMux);

  if (payloadToFree)
    free(payloadToFree);
}''',
        "dynamic WS pop",
    )

    replace_once(
        http,
'''  if (uuid)
  {
    json += ",\\"uuid\\":\\"";
    json += escapeJson(String(uuid));
    json += "\\"";
  }
  else
  {
    json += ",\\"uuid\\":null";
  }

  json += "}";''',
'''  if (uuid)
  {
    json += ",\\"uuid\\":\\"";
    json += escapeJson(String(uuid));
    json += "\\"";
  }

  json += "}";''',
        "omit null UUID",
    )

    replace_once(
        ws_api,
'''    const message: TypedClientWsMessage<TType> = {
      type,
      data,
      uuid: this.uuid,
    };''',
'''    const message: TypedClientWsMessage<TType> = {
      type,
      data,
    };''',
        "remove per-message client UUID",
    )

    replace_once(
        ws_types,
'''export type WsMessage<T = unknown> = {
  type: string;
  data?: T;
  uuid: string | null;
};

export type ClientWsMessage<T = unknown> = {
  type: string;
  data?: T;
  uuid: string;
};''',
'''export type WsMessage<T = unknown> = {
  type: string;
  data?: T;
  uuid?: string | null;
};

export type ClientWsMessage<T = unknown> = {
  type: string;
  data?: T;
  uuid?: string;
};''',
        "optional base WS UUID",
    )

    replace_once(
        ws_types,
'''    type: K;
    data: ClientWsPayloadMap[K];
    uuid: string;''',
'''    type: K;
    data: ClientWsPayloadMap[K];
    uuid?: string;''',
        "optional typed client UUID",
    )

    replace_once(
        ws_client,
'''const HEARTBEAT_UUID = "__dccexpresslite_ws_heartbeat__";
const HEARTBEAT_INTERVAL_MS = 5000;''',
'''const HEARTBEAT_INTERVAL_MS = 5000;''',
        "remove heartbeat UUID constant",
    )

    replace_once(
        ws_client,
'''                if (message.uuid === HEARTBEAT_UUID) {
                    if (WS_DEBUG) {
                        console.debug("[WS] heartbeat response");
                    }

                    return;
                }''',
'''                if (message.type === "heartbeatAck") {
                    if (WS_DEBUG) {
                        console.debug("[WS] heartbeat response");
                    }

                    return;
                }''',
        "type-based heartbeat response",
    )

    replace_once(
        ws_client,
'''        const heartbeatMessage: ClientWsMessage = {
            type: "heartbeat",
            data: {},
            uuid: HEARTBEAT_UUID,
        };''',
'''        const heartbeatMessage: ClientWsMessage = {
            type: "heartbeat",
            data: {},
        };''',
        "heartbeat without UUID",
    )

    # Transactional commit: source files are written only after every
    # expected current-main block matched successfully.
    for path, text in PENDING.items():
        path.write_text(text, encoding="utf-8")

    print("Memory optimization applied successfully.")
    print("Expected static DRAM saving from WS inbound queue: about 43 KB.")
    print("Normal WS messages no longer carry client UUID / uuid:null.")
    return 0

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise
