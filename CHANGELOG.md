# Changelog

All notable changes to DCCExpressLite will be documented in this file.

## 0.1.0-alpha.5 - 2026-08-26

- Always stage the versioned starter layout, locomotive roster and locomotive images into clean LittleFS builds.
- Include those starter files in the merged firmware used by the browser installer.
- Preserve existing local/device data during ordinary developer builds by copying defaults only when files are missing.

## 0.1.0-alpha.4 - 2026-08-26

- Made the repository self-contained with the complete web UI source under `web-ui/src/`.
- Added a browser-based ESP Web Tools installer deployed through GitHub Pages.
- Added USB/Web Serial home-network provisioning with the `<WIFI "ssid" "password">` DCC-EX command.
- Added a verified 4 MB merged firmware image containing bootloader, partitions, firmware and LittleFS.
- Added automatic merged-firmware and web-installer packaging to tagged releases.
- Fixed repeated `/#layout` refresh stalls and improved HTTP connection cleanup.
- Added concise user installation instructions and a complete developer guide.
- Reduced the web UI dependency set and simplified the ESP32-only PlatformIO configuration.

## 0.1.0 - 2026-08-25

- Initial DCCExpressLite release for EX-CSB1.
- Embedded responsive web interface with WebSocket reconnection.
- Layout editor and runtime control with locomotives, turnouts and routes.
- Device, DCC-EX and flash information panels.
- Wi-Fi configuration stored on the device.
- Layout and locomotive import/export.
- Cross-platform Avalonia installer for Windows, macOS and Linux.
- Optional installer starter package with a sample layout, locomotives and images.
- Release installers automatically load the firmware manifest bundled for that exact GitHub release.
- GitHub Actions workflows for firmware, filesystem and installer builds.
