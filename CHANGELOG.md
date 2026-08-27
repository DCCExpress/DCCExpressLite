# Changelog

All notable changes to DCCExpressLite will be documented in this file.

## 0.1.0-alpha.3 - 2026-08-27

- Added locomotive, accessory and DigiTools decoder programming from the embedded UI.
- Added ID-based automatic signal logic with turnout and sensor conditions, safe RED fallback and sequential physical accessory output.
- Added a project-wide integrity checker for layout IDs, route-button turnout references, automatic routes, signal rules and locomotives.
- Added temperature status levels and a one-shot critical-temperature warning with hysteresis.
- Added a Layout Editor HELP link to the online GitHub Wiki.
- Fixed a small heap leak after LittleFS file uploads.
- Synchronized the release starter data with the tested device: current layout, turnout address, three signals, four locomotives, images and active signal rules.
- Added illustrated online documentation for installation, layout editing, routes, signals, locomotive control, decoder programming, files, diagnostics and development.

## 0.1.0-alpha.2 - 2026-08-27

- Upgraded and isolated the CommandStation-EX integration for easier future upstream updates.
- Made multi-client HTTP/WebSocket handling bounded and watchdog-safe.
- Added a root-based LittleFS file manager with uploads, deletion, image previews and text-source viewing.
- Added the mobile runtime layout overlay with center, fit, route progress and emergency-stop controls.
- Made backup export/import release-independent and included locomotive images.
- Refined the home, layout, property, information and device panels.
- Synchronized the starter layout, locomotive data and images with the tested EX-CSB1 LittleFS contents.
- Simplified distribution to the browser installer and one merged firmware image only.

## 0.1.0-alpha.1 - 2026-08-27

- First public alpha release for ESP32 / EX-CSB1.
- Integrated the stable CommandStation-EX `5.6.1-Prod` core with a small, documented upgrade surface.
- Added the embedded responsive layout editor and runtime controller.
- Added locomotive control, turnouts, accessories, sensors, signals and routes.
- Added the HTTP server, WebSocket API and automatic client reconnection.
- Added saved Wi-Fi configuration through the web UI and serial commands.
- Added the browser-based USB web installer and selectable release support.
- Added a verified merged 4 MB firmware image containing firmware, LittleFS and starter data.
- Added starter layout, locomotives and locomotive images under LittleFS `/images/`.
- Added layout, locomotive and locomotive-image export/import.
- Added DCC-EX, flash, CPU, temperature and connected-device information panels.
