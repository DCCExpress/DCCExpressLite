# DCCExpressLite

[![Build and release](https://github.com/DCCExpress/DCCExpressLite/actions/workflows/release-lite.yml/badge.svg)](https://github.com/DCCExpress/DCCExpressLite/actions/workflows/release-lite.yml)
[![License: GPL v3+](https://img.shields.io/badge/License-GPLv3%2B-blue.svg)](LICENSE.md)

DCCExpressLite is an ESP32/EX-CSB1 focused DCC-EX firmware fork with a built-in HTTP server, LittleFS web UI hosting, and a DCCExpressNext-compatible lightweight WebSocket bridge.

It is the successor to [DCCExpress-Mini](https://github.com/DCCExpress/DCCExpress-Mini), with a substantially expanded layout editor/controller. Firmware installation is handled by the separate [DCCExpressLiteInstaller](https://github.com/DCCExpress/DCCExpressLiteInstaller) application.

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
- `default-data/` - optional starter layout, locomotive list and matching images used by the installer.
- `tools/` - firmware release manifest tooling.
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

Download the separate [DCCExpressLiteInstaller](https://github.com/DCCExpress/DCCExpressLiteInstaller) Windows application. It discovers the available releases from this repository, lets you choose a version, verifies its firmware assets, and flashes the EX-CSB1 without PlatformIO or Python.

Firmware GitHub releases are produced automatically by `.github/workflows/release-lite.yml` and contain only the firmware, LittleFS, optional starter data and their manifest.

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
