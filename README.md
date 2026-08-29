# DCCExpressLite

### Browser-based DCC model railway command center for the DCC-EX EX-CSB1

[![CI](https://github.com/DCCExpress/DCCExpressLite/actions/workflows/ci.yml/badge.svg)](https://github.com/DCCExpress/DCCExpressLite/actions/workflows/ci.yml)
[![Release](https://github.com/DCCExpress/DCCExpressLite/actions/workflows/release-lite.yml/badge.svg)](https://github.com/DCCExpress/DCCExpressLite/actions/workflows/release-lite.yml)
[![License: GPL v3+](https://img.shields.io/badge/License-GPLv3%2B-blue.svg)](LICENSE.md)

**DCCExpressLite is a complete browser-controlled digital model railway command center for the [DCC-EX EX-CSB1](https://dcc-ex.com/ex-commandstation/rtr-manual__included-esb1.html).** It combines the proven [DCC-EX EX-CommandStation](https://dcc-ex.com/ex-commandstation/index.html) firmware with an embedded layout editor, locomotive throttle, turnout and signal control, routes, automation, decoder programming, diagnostics, and mobile control.

The EX-CSB1 generates the DCC track signal and controls locomotives and accessory decoders. Its ESP32 also serves the complete DCCExpressLite web application directly over Wi-Fi, making the EX-CSB1 a self-contained command center for a model railway. No separate desktop application, Node.js server, cloud service, or sibling source repository is required at runtime or during development.

## Fastest installation: Web Installer

For most users the quickest route is the **[DCCExpressLite Web Installer](https://dccexpress.github.io/DCCExpressLite/)**.

1. Open the installer in desktop Chrome or Edge.
2. Select the DCCExpressLite version to install.
3. Connect the EX-CSB1 with USB.
4. Select its serial port and start installation.
5. Keep the USB cable connected until flashing finishes.
6. After installation, enter the home SSID and password on the same installer page and send them over the selected serial port.

The web installer is the only supported installation method. It flashes one verified merged image containing the bootloader, partition table, firmware, embedded web application, starter layout, four locomotives and their images. It can then configure the saved home network over USB. Web Serial is not available in Firefox, Safari or mobile browsers.



## Features

![Screenshot](wiki-docs/images/Screenshot%202026-08-29%20063942.png)

- Built on the open-source [DCC-EX](https://dcc-ex.com/) model railway control platform.
- Designed specifically for the integrated ESP32-based [EX-CSB1 Command Station / Booster](https://dcc-ex.com/ex-commandstation/rtr-manual__included-esb1.html).
- CommandStation-EX `5.6.1-Prod` core with a small, documented integration patch surface.
- HTTP server on port `80` and WebSocket endpoint at `/ws`.
- Responsive layout editor and runtime controller.
- Locomotive editor, dual throttle panels, functions and per-device image mirroring.
- Configurable Bluetooth/USB gamepad control with multiple-controller selection and client-local mappings.
- Track power, emergency stop, turnouts, accessories, sensors, signals and routes.
- DCC accessory and DCC-EX VPIN outputs for turnouts, signals and configurable Toggle/Push/Momentary layout buttons.
- Route sequencing, route highlighting and cached accessory states.
- Multi-client accessory/VPIN state synchronization and a filterable, bounded live I/O log.
- Persistent block occupancy and turnout runtime state shared between clients.
- Network configuration stored on the ESP32.
- Runtime, flash, processor, temperature and HAL/I²C device information.
- Layout, locomotive and locomotive-image import/export.
- Automatic WebSocket reconnect and refresh-safe static file delivery.

After network setup, open `http://dccex.local` or the IP address shown on the EX-CSB1 display.

The complete user guide covers the layout editor, elements, turnouts, routes, signals, automation, decoder programming, backup/restore and diagnostics in the **[DCCExpressLite Wiki](https://github.com/DCCExpress/DCCExpressLite/wiki)**.

The same network setup is available in any 115200-baud serial monitor:

```text
<WIFI "Home SSID" "password">
```

The command saves the credentials and restarts the EX-CSB1. Use `<WIFI?>` to check whether credentials are stored or `<WIFI CLEAR>` to erase them and return to setup-hotspot mode.

## Repository layout

- `CommandStation-EX/` — firmware and embedded HTTP/WebSocket server source.
- `web-ui/src/` — complete React/TypeScript web application source.
- `data/` — generated production UI and device data uploaded to LittleFS.
- `default-data/` — versioned starter layout, locomotive list and matching images.
- `wiki-docs/` — version-controlled source pages and screenshots published to the project Wiki.
- `webinstaller/` — ESP Web Tools installer page source.
- `tools/` — merged-image build and LittleFS staging scripts.
- `UPSTREAM.md` — integrated DCC-EX version, local hook list and future upgrade procedure.
- `.github/workflows/` — CI, release and GitHub Pages deployment.

## Development

All prerequisites, commands, upload steps, partition offsets and release instructions are documented in **[DEVELOPING.md](DEVELOPING.md)**.

Quick local build:

```powershell
npm ci --prefix web-ui
npm run embed --prefix web-ui
pio run -e ESP32
pio run -e ESP32 -t buildfs
```

## Releases

Version tags automatically build and publish exactly one firmware asset: the 4 MB merged image used by the HTTPS web installer on GitHub Pages. Separate Windows, Linux, macOS and individual ESP32 firmware downloads are not produced.

On a clean checkout, PlatformIO stages the files from `default-data/` into LittleFS automatically. Existing developer/device data files are left untouched.

## License

DCCExpressLite and its DCC-EX-derived firmware are distributed under the GNU GPL v3 or later. See [LICENSE.md](LICENSE.md) and the retained firmware license notices.
