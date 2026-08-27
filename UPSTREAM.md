# CommandStation-EX upstream integration

DCCExpressLite currently embeds the official DCC-EX **CommandStation-EX
v5.6.1-Prod** release:

- upstream repository: <https://github.com/DCC-EX/CommandStation-EX>
- release tag: `v5.6.1-Prod`
- upstream commit: `822a54977263b621541a70a3546d38f387ac7294`

The normal upstream sketch, including `CommandStation-EX.ino`, is retained
unchanged. Board and product configuration lives in `CommandStation-EX/config.h`.
DCCExpressLite starts from the upstream `mySetup.h` extension point.

## DCCExpressLite-owned modules

- `DCCExpressLite.cpp/.h` — integration lifecycle, NVS-backed Wi-Fi startup,
  HAL device serialization and current measurements.
- `HTTPServer.cpp/.h` — HTTP routes, WebSocket API and LittleFS access.
- `HTTPSerialWrapper.cpp/.h` — captures direct DCC-EX parser responses for
  WebSocket clients without replacing the upstream USB serial stream.
- `NetworkSettings.cpp/.h` — saved Wi-Fi credentials and serial commands.
- `mySetup.h` — calls `DCCExpressLite::begin()` through the supported upstream
  startup extension point.

## Small upstream patch surface

Only four upstream files contain DCCExpressLite integration hooks:

1. `DCCEXParser.cpp` calls an optional weak `myRawCommand()` hook before
   tokenisation. This preserves quoted `<WIFI "ssid" "password">` values.
2. `CommandDistributor.cpp` calls an optional weak `myCommandBroadcast()`
   observer after normal serial broadcasting.
3. `IODevice.h` grants `DCCExpressLite` read-only inspection access for the
   Devices tab.
4. `TrackManager.h` grants `DCCExpressLite` read-only current-measurement
   access for the Info tab.

All product behaviour is implemented in the DCCExpressLite-owned modules; the
hooks contain no HTTP, WebSocket, JSON, LittleFS or Wi-Fi implementation.

## Future upgrade procedure

1. Download or check out the desired official production tag in a temporary
   directory.
2. Copy the upstream top-level `.ino`, `.cpp` and `.h` files plus `LICENSE`
   into `CommandStation-EX/`. Do not overwrite the DCCExpressLite-owned files
   listed above or `config.h`/`mySetup.h`.
3. Reapply the four small hooks listed above. A failed context is intentional:
   review the changed upstream API rather than silently accepting it.
4. Confirm that `CommandStation-EX.ino` is byte-for-byte identical to the
   selected upstream tag.
5. Run the frontend typecheck and embedded build, followed by both PlatformIO
   targets:

   ```powershell
   npm run typecheck --prefix web-ui
   npm run embed --prefix web-ui
   pio run -e ESP32
   pio run -e ESP32 -t buildfs
   ```

6. Verify that LittleFS contains `layout.json`, `locos.json` and all locomotive
   images under `/images/`. Test on an EX-CSB1 before committing or publishing.
7. Update the version, tag and commit recorded at the top of this file.
