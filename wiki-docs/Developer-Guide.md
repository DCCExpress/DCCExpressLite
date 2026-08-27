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
cd C:\ChatGPT\GitRepo\DCCExpressLite\web-ui
npm ci
npm run embed

cd ..
pio run -e ESP32
pio run -e ESP32 -t buildfs
pwsh ./tools/New-MergedFirmware.ps1 -Version 0.1.0-alpha.3
```

The merged image must be exactly 4,194,304 bytes. See the repository's `DEVELOPING.md` for prerequisites, upload commands, flash offsets, checklist, and release procedure. See `UPSTREAM.md` for the integrated CommandStation-EX version and the intentionally small upstream modification surface.

## Release model

A `v*` tag builds and publishes one merged firmware asset, creates a GitHub prerelease for alpha/beta versions, and updates the GitHub Pages version selector. Separate Windows, Linux, macOS, or split firmware downloads are not produced.

