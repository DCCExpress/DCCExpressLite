# DCCExpressLite Web Installer

The static ESP Web Tools installer is published to GitHub Pages by the release workflow. Its generated `manifest.json` and `merged-firmware.bin` are produced by `tools/New-MergedFirmware.ps1`.

Web Serial requires a Chromium-based desktop browser and a secure HTTPS origin. Opening `index.html` directly from disk is therefore not sufficient for flashing.

After flashing, the page can reopen the EX-CSB1 serial port and send the saved-network command:

```text
<WIFI "Home SSID" "password">
```

The firmware stores the credentials in NVS and restarts automatically.
