# Developer guide

The complete source is contained in the `DCCExpressLite` repository. It does not reference sibling DCCExpress projects.

## Main directories

- `CommandStation-EX/` — upstream DCC-EX core plus small integration hooks and separate DCCExpressLite modules
- `web-ui/src/` — React/TypeScript embedded UI
- `data/` — generated embedded UI and release LittleFS content
- `default-data/` — clean-checkout starter layout, locomotives, images, and rules
- `tools/` — default-data staging and merged-image packaging
- `webinstaller/` — Web Serial installer source
- `.github/workflows/` — CI, merged release, and GitHub Pages deployment

## Build

```powershell
cd <your-path>\DCCExpressLite\web-ui
npm ci
npm run embed

cd ..
pio run -e ESP32
pio run -e ESP32 -t buildfs
pwsh ./tools/New-MergedFirmware.ps1 -Version 0.1.0-alpha.3
```

The merged image must be exactly 4,194,304 bytes. See the repository's `DEVELOPING.md` for prerequisites, upload commands, flash offsets, checklist, and release procedure. See `UPSTREAM.md` for the integrated CommandStation-EX version and the intentionally small upstream modification surface.

## Layout output synchronization

Layout elements never use the generic raw-command API for normal VPIN control. The browser sends either `setBasicAccessory` or `setVpin`; the EX-CSB1 executes the DCC-EX command and broadcasts `accessoryChanged` or `vpinChanged` to all clients. Browsers update their runtime model and invalidate the canvas only from that authoritative event.

The firmware keeps a bounded 96-entry VPIN state cache. A newly connected client receives cached VPIN states through the staged initial snapshot, avoiding a synchronous WebSocket burst. Signal Logic uses the same output writer and broadcast path for accessory and VPIN bits.

The Log tab observes both incoming and successfully transmitted WebSocket messages in the browser. Its 200-entry buffer and filters are client-side only; no persistent log is written to LittleFS.

## Release model

A `v*` tag builds and publishes one merged firmware asset, creates a GitHub prerelease for alpha/beta versions, and updates the GitHub Pages version selector. Separate Windows, Linux, macOS, or split firmware downloads are not produced.
