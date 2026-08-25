# DCCExpressLite

[![Build and release](https://github.com/DCCExpress/DCCExpressLite/actions/workflows/release-lite.yml/badge.svg)](https://github.com/DCCExpress/DCCExpressLite/actions/workflows/release-lite.yml)
[![License: GPL v3+](https://img.shields.io/badge/License-GPLv3%2B-blue.svg)](LICENSE.md)

DCCExpressLite is an ESP32/EX-CSB1 focused DCC-EX firmware fork with a built-in HTTP server, LittleFS web UI hosting, and a DCCExpressNext-compatible lightweight WebSocket bridge.

It is the successor to [DCCExpress-Mini](https://github.com/DCCExpress/DCCExpress-Mini), with a substantially expanded layout editor/controller and a graphical cross-platform installer.

The goal is simple: open the command station from a phone browser, connect to `ws://<device-ip>/ws`, and control locomotives, functions, track power, emergency stop, turnouts, basic accessories, sensors, and raw DCC-EX commands without a separate Node.js server.

## What Is Included

- EX-CSB1 default motor driver configuration.
- HTTP server on port `80`.
- WebSocket endpoint at `/ws`.
- LittleFS static file hosting from `data/`.
- DCCExpressNext Lite WebSocket commands:
  - `setTrackPower`
  - `setProgrammingPower`
  - `emergencyStop`
  - `setLoco`
  - `getLoco`
  - `setLocoFunction`
  - `setTurnout`
  - `setBasicAccessory`
  - `setSensor`
  - `writeDccExDirectCommand`
  - `dccexraw` for backward compatibility with DCCExpress-Mini
  - `locosCommand` load/save backed by `/locos.json`
  - `fileCommand` read/write backed by LittleFS
- DCCExpressNext-style server events:
  - `ws:welcome`
  - `commandCenterInfo`
  - `powerInfo`
  - `commandCenterLockChanged`
  - `locoState`
  - `turnoutChanged`
  - `accessoryChanged`
  - `sensorChanged`
  - `dccExDirectCommandResponse`
  - `locosResponse`
  - `fileResponse`
  - `rawInfo`
  - `error`

## Project Layout

- `CommandStation-EX/` - ESP32 firmware source.
- `CommandStation-EX/HTTPServer.cpp` - DCCExpressLite HTTP/WebSocket bridge.
- `data/` - files uploaded to LittleFS.
- `data/locos.json` - Next-compatible default locomotive list.
- `installer/` - C# / Avalonia graphical firmware installer for Windows, macOS, and Linux.
- `DCCExpress/` - original DCCExpress-Mini web client source retained for compatibility.

## Build Firmware

Install PlatformIO, then run from this folder:

```bash
pio run -e ESP32
pio run -e ESP32 --target upload
pio run -e ESP32 --target uploadfs
```

`platformio.ini` already enables `-DHTTP`, LittleFS, `ESPAsyncWebServer`, `AsyncTCP`, and `ArduinoJson`.

## Graphical Installer

The `installer/` project discovers the EX-CSB1 USB serial port, reads firmware releases from GitHub, verifies SHA-256 hashes, flashes all ESP32 partitions, and can preserve layout and locomotive data across a LittleFS update. It publishes self-contained applications for Windows x64, macOS Intel/Apple Silicon, and Linux x64/ARM64.

See [`installer/README.md`](installer/README.md) for development, firmware manifest, and publishing instructions. GitHub releases can be produced automatically by `.github/workflows/release-lite.yml`.

## EX-CSB1 Defaults

`CommandStation-EX/config.example.h` defaults to:

```cpp
#define MOTOR_SHIELD_TYPE EXCSB1
```

If you create your own `config.h`, keep that setting for EX-CSB1 hardware and adjust WiFi credentials as needed.

## Using DCCExpressNext Mobile

The current `data/` folder already contains a built DCCExpressNext mobile UI in `index.html` and `assets/`.

To refresh it later, build the mobile client from `DCCExpressNext/mobile`, then copy its build output into this project's `data/` folder before `uploadfs`.

```bash
cd ../DCCExpressNext/mobile
npm install
npm run build
```

Then copy the generated `dist/` contents into:

```text
DCCExpressLite/data/
```

The mobile client already defaults to:

```text
ws://<same-host>/ws
```

so it will connect directly to the ESP32 when served from LittleFS.

## Notes

This is intentionally a Lite bridge. The full DCCExpressNext Node.js runtime features, such as route reservation, task manager, script runtime, automation services, and full layout runtime graph, are not ported to ESP32. Those remain better suited to the full Node server.
