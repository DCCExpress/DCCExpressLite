# DCCExpressLite

[![CI](https://github.com/DCCExpress/DCCExpressLite/actions/workflows/ci.yml/badge.svg)](https://github.com/DCCExpress/DCCExpressLite/actions/workflows/ci.yml)
[![Release](https://github.com/DCCExpress/DCCExpressLite/actions/workflows/release-lite.yml/badge.svg)](https://github.com/DCCExpress/DCCExpressLite/actions/workflows/release-lite.yml)
[![License: GPL v3+](https://img.shields.io/badge/License-GPLv3%2B-blue.svg)](LICENSE.md)

DCCExpressLite is a self-contained ESP32/EX-CSB1 command-station firmware with an embedded browser-based layout editor and controller. No separate Node.js server or sibling source repository is required at runtime or during development.

## Fastest installation: Web Installer

For most users the quickest route is the **[DCCExpressLite Web Installer](https://dccexpress.github.io/DCCExpressLite/)**.

1. Open the installer in desktop Chrome or Edge.
2. Connect the EX-CSB1 with USB.
3. Select its serial port and start installation.
4. Keep the USB cable connected until flashing finishes.
5. After installation, enter the home SSID and password on the same installer page and send them over the selected serial port.

The web installer flashes one verified merged image containing the bootloader, partition table, firmware, embedded web application, starter layout, four locomotives and their images. It can then configure the saved home network over USB. Web Serial is not available in Firefox, Safari or mobile browsers.

Windows users can alternatively download the graphical **[DCCExpressLiteInstaller](https://github.com/DCCExpress/DCCExpressLiteInstaller/releases)**. Manual binaries are available under this repository's [releases](https://github.com/DCCExpress/DCCExpressLite/releases).

## Features

- DCC-EX based EX-CSB1 command-station firmware.
- HTTP server on port `80` and WebSocket endpoint at `/ws`.
- Responsive layout editor and runtime controller.
- Locomotive editor, dual throttle panels, functions and per-device image mirroring.
- Track power, emergency stop, turnouts, accessories, sensors, signals and routes.
- Route sequencing, route highlighting and cached accessory states.
- Network configuration stored on the ESP32.
- Runtime, flash, processor, temperature and HAL/I²C device information.
- Layout and locomotive import/export.
- Automatic WebSocket reconnect and refresh-safe static file delivery.

After network setup, open the IP address shown on the EX-CSB1 display. The factory setup address is usually `http://192.168.4.1`.

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
- `webinstaller/` — ESP Web Tools installer page source.
- `tools/` — merged-image and release-manifest build scripts.
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

Version tags automatically build and publish:

- individual ESP32 flash images;
- a 4 MB merged firmware image;
- `firmware-manifest.json` for the desktop installer;
- optional starter data;
- the HTTPS web installer on GitHub Pages.

On a clean checkout, PlatformIO stages the files from `default-data/` into LittleFS automatically. Existing developer/device data files are left untouched.

## License

DCCExpressLite and its DCC-EX-derived firmware are distributed under the GNU GPL v3 or later. See [LICENSE.md](LICENSE.md) and the retained firmware license notices.
