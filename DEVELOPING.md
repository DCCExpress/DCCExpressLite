# DCCExpressLite development guide

This repository contains every project-owned source file required to build the EX-CSB1 firmware, embedded web UI, LittleFS image, merged firmware and web installer. External package managers only provide the compiler/toolchain and declared third-party libraries.

## Prerequisites

- Git
- Node.js 22 or newer
- Python 3.12 or newer
- PlatformIO Core
- PowerShell 7 for release packaging
- An EX-CSB1 connected by USB for hardware upload/testing

Install PlatformIO:

```powershell
python -m pip install platformio
```

## Initial checkout

```powershell
git clone https://github.com/DCCExpress/DCCExpressLite.git
cd DCCExpressLite
npm ci --prefix web-ui
```

No `DCCExpressNext`, `DCCExpress-Mini` or installer checkout is needed.

## Web UI development

Start the Vite development server. API and WebSocket requests are proxied to the EX-CSB1 address configured in `web-ui/vite.config.ts`:

```powershell
npm run dev --prefix web-ui
```

Type-check only:

```powershell
npm run typecheck --prefix web-ui
```

Build and embed the compressed UI into `data/`:

```powershell
npm run embed --prefix web-ui
```

The embed step creates content-hashed asset URLs in `data/index.html` and gzip-compressed JavaScript/CSS files in `data/assets/`.

## Firmware configuration

The PlatformIO environment is `ESP32`. Its source, include and library root is `CommandStation-EX/`. The EX-CSB1 motor shield default is defined in `CommandStation-EX/config.example.h`.

For a local configuration:

```powershell
Copy-Item CommandStation-EX/config.example.h CommandStation-EX/config.h
```

Keep the following hardware selection:

```cpp
#define MOTOR_SHIELD_TYPE EXCSB1
```

`config.h` may contain machine-specific settings and should not be committed.

## Firmware and LittleFS builds

```powershell
pio run -e ESP32
pio run -e ESP32 -t buildfs
```

Build outputs are written to:

```text
CommandStation-EX/.pio/build/ESP32/
```

Relevant files:

- `bootloader.bin`
- `partitions.bin`
- `firmware.bin`
- `littlefs.bin`

## Upload to EX-CSB1

Replace `COM6` with the correct serial port:

```powershell
pio run -e ESP32 -t upload --upload-port COM6
pio run -e ESP32 -t uploadfs --upload-port COM6
pio device monitor --port COM6 --baud 115200
```

Upload firmware whenever C++ or partition configuration changes. Upload LittleFS whenever `data/` or the embedded web UI changes.

## Serial Wi-Fi provisioning

DCCExpressLite adds three human-readable commands to the DCC-EX serial protocol. Use 115200 baud:

```text
<WIFI "Home SSID" "password">
<WIFI?>
<WIFI CLEAR>
```

The first command validates and saves the credentials in ESP32 NVS, replies with `<WIFI SAVED RESTARTING>`, then restarts after one second. SSIDs may contain 1–32 characters and passwords 8–63 characters; quoted values may contain spaces but not a double-quote character. `<WIFI?>` reports only whether a configuration exists and never exposes the password. `<WIFI CLEAR>` erases the saved network and restarts into the configured setup-hotspot mode.

The GitHub Pages installer exposes the same command through Web Serial after flashing. Its source is `webinstaller/index.html`.

## Production build sequence

Run these commands from the repository root:

```powershell
npm ci --prefix web-ui
npm run embed --prefix web-ui
pio run -e ESP32
pio run -e ESP32 -t buildfs
```

## Merged firmware and installer artifacts

Generate the 4 MB image used by ESP Web Tools:

```powershell
./tools/New-MergedFirmware.ps1 -Version 0.1.0-alpha.6
```

Generate every desktop-installer/release artifact:

```powershell
./tools/New-FirmwareManifest.ps1 `
  -Version 0.1.0-alpha.6 `
  -BaseUrl https://github.com/DCCExpress/DCCExpressLite/releases/download/v0.1.0-alpha.6
```

Outputs:

```text
artifacts/firmware/     Release binaries and firmware-manifest.json
artifacts/webinstaller/ GitHub Pages installer, manifest and merged-firmware.bin
```

The merged image uses these ESP32 flash offsets:

| Offset | Content |
| ---: | --- |
| `0x1000` | bootloader |
| `0x8000` | partition table |
| `0xE000` | boot application selector |
| `0x10000` | DCCExpressLite firmware |
| `0x290000` | LittleFS web/data image |

## CI and release process

Every push and pull request runs the web UI, firmware and LittleFS builds.

To publish a prerelease after updating and testing the sources:

```powershell
git status
git add --all
git commit -m "Prepare DCCExpressLite alpha release"
git push origin main
git tag v0.1.0-alpha.6
git push origin v0.1.0-alpha.6
```

The tag starts `.github/workflows/release-lite.yml`, which builds the UI from source, creates the firmware assets and merged image, creates the GitHub prerelease, and updates the GitHub Pages web installer.

## Verification checklist

Before tagging a release:

1. `npm run embed --prefix web-ui` succeeds.
2. `pio run -e ESP32` succeeds.
3. `pio run -e ESP32 -t buildfs` succeeds.
4. Firmware and LittleFS upload successfully to a real EX-CSB1.
5. Home, Settings and `/#layout` load after repeated refreshes.
6. WebSocket reconnect, power, emergency stop, locomotive and turnout control work.
7. The merged image is exactly 4,194,304 bytes.
8. The web installer manifest references `merged-firmware.bin` at offset `0`.
