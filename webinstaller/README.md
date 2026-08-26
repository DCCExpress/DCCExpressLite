# DCCExpressLite Web Installer

The static ESP Web Tools installer is published to GitHub Pages by the release workflow. Its generated `manifest.json` and `merged-firmware.bin` are produced by `tools/New-MergedFirmware.ps1`.

The release workflow publishes a same-origin `releases.json` index for the version selector. It offers every GitHub release that contains a merged firmware asset without consuming the visitor's GitHub API quota. Each selectable merged image is mirrored under the same GitHub Pages origin so ESP Web Tools can download it without cross-origin restrictions. The public API is used as a secondary source, and the latest published image remains available as a final fallback.

Web Serial requires a Chromium-based desktop browser and a secure HTTPS origin. Opening `index.html` directly from disk is therefore not sufficient for flashing.

After flashing, the page can reopen the EX-CSB1 serial port and send the saved-network command:

```text
<WIFI "Home SSID" "password">
```

The firmware stores the credentials in NVS and restarts automatically.
