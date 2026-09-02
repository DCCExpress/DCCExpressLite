from pathlib import Path
import shutil, sys

def fail(msg):
    print('ERROR:', msg)
    sys.exit(1)

def patch(path, replacements):
    p = Path(path)
    if not p.exists(): fail(f'Missing file: {p}')
    text = p.read_text(encoding='utf-8')
    new = text
    for old, repl in replacements:
        n = new.count(old)
        if n != 1:
            fail(f'Anchor mismatch in {p}: expected 1, got {n}. No write performed for this file.')
        new = new.replace(old, repl, 1)
    bak = p.with_suffix(p.suffix + '.v6.bak')
    if not bak.exists(): shutil.copy2(p, bak)
    p.write_text(new, encoding='utf-8')
    print('Patched:', p)

http = Path('CommandStation-EX/HTTPServer.cpp')
client = Path('web-ui/src/domain/clientWsCommands.ts')
wsapi = Path('web-ui/src/services/wsApi.ts')

patch(http, [
('''static String readFileText(const String &fileName)
{
  String path = fileName.startsWith("/") ? fileName : "/" + fileName;
  File file = LittleFS.open(path, "r");

  if (!file)
  {
    return "";
  }
''', '''static String readFileText(const String &fileName)
{
  String path = fileName.startsWith("/") ? fileName : "/" + fileName;
  if (!LittleFS.exists(path)) return "";
  File file = LittleFS.open(path, "r");

  if (!file)
  {
    return "";
  }
'''),
('''  if (type == "setSignalAspect")
  {
    const int address = getInt(payload["address"]);
    const int aspect = getInt(payload["aspect"]);

    if (address < 1 || address > MAX_LINEAR_ACCESSORY_ADDRESS ||
        aspect < 0 || aspect > 255)
    {
      sendError(clientId, "signal_aspect_out_of_range", uuid);
      return;
    }

    if (!trackPowerOn)
    {
      sendError(clientId, "track_power_off", uuid);
      sendPowerInfo(clientId);
      return;
    }

    dccParseRaw("<A " + String(address) + " " + String(aspect) + ">");
    broadcastMessage("signalAspectChanged",
      "{\\\"address\\\":" + String(address) +
      ",\\\"aspect\\\":" + String(aspect) + "}", uuid);
    return;
  }
''', '''  if (type == "setSignalAspect")
  {
    const int address = getInt(payload["address"]);
    const int aspect = getInt(payload["aspect"]);

    if (address < 1 || address > MAX_LINEAR_ACCESSORY_ADDRESS ||
        aspect < 0 || aspect > 255)
    {
      sendError(clientId, "signal_aspect_out_of_range", uuid);
      return;
    }

    if (!trackPowerOn)
    {
      sendError(clientId, "track_power_off", uuid);
      sendPowerInfo(clientId);
      return;
    }

    dccParseRaw("<A " + String(address) + " " + String(aspect) + ">");

    if (payload["turnoutPhysicalValue"].is<bool>())
    {
      const bool physicalValue = payload["turnoutPhysicalValue"].as<bool>();
      turnoutStateCache[address] = physicalValue ? 1 : 0;
      DCCExpressLiteRuntimeState::setTurnout(static_cast<uint16_t>(address), physicalValue);
      DCCExpressLiteSignalLogic::notifyTurnout(static_cast<uint16_t>(address), physicalValue);
      Serial.printf("Extended turnout #%d aspect=%d runtime physical=%u.\\n",
                    address, aspect, physicalValue ? 1 : 0);
      broadcastMessage("turnoutChanged",
        "{\\\"address\\\":" + String(address) +
        ",\\\"closed\\\":" + String(physicalValue ? "true" : "false") + "}", uuid);
    }

    broadcastMessage("signalAspectChanged",
      "{\\\"address\\\":" + String(address) +
      ",\\\"aspect\\\":" + String(aspect) + "}", uuid);
    return;
  }
'''),
('''static void sendCompressedAsset(AsyncWebServerRequest *request,
                                const char *compressedPath,
                                const char *publicPath,
                                const char *contentType)
{
  File file = LittleFS.open(compressedPath, "r");
''', '''static void sendCompressedAsset(AsyncWebServerRequest *request,
                                const char *compressedPath,
                                const char *publicPath,
                                const char *contentType)
{
  if (!LittleFS.exists(compressedPath))
  {
    request->send(404, "text/plain", "Asset not found");
    return;
  }

  File file = LittleFS.open(compressedPath, "r");
''')
])

patch(client, [
('''export type SetSignalAspectCommandPayload = {
  address: number;
  aspect: number;
};
''', '''export type SetSignalAspectCommandPayload = {
  address: number;
  aspect: number;
  turnoutPhysicalValue?: boolean;
};
''')
])

patch(wsapi, [
('''  setSignalAspect(address: number, aspect: number): boolean {
    return this.send("setSignalAspect", { address, aspect });
  }
''', '''  setSignalAspect(
    address: number,
    aspect: number,
    turnoutPhysicalValue?: boolean
  ): boolean {
    return this.send("setSignalAspect", {
      address,
      aspect,
      ...(turnoutPhysicalValue === undefined ? {} : { turnoutPhysicalValue }),
    });
  }
''')
])

print('v6 patch applied successfully.')
