# Installation and first start

The supported installation method is the [DCCExpressLite Web Installer](https://dccexpress.github.io/DCCExpressLite/). It uses Web Serial and therefore requires desktop Chrome, Edge, or another Chromium-based browser. Firefox, Safari, and mobile browsers cannot flash the device.

## Install

1. Connect the EX-CSB1 to the computer over USB.
2. Open the Web Installer over HTTPS.
3. Select a DCCExpressLite release.
4. Connect to the serial port offered by the browser.
5. Install the merged firmware image.
6. Optionally enter the home Wi-Fi SSID and password over the installer serial connection.

The merged 4 MB image contains the bootloader, partition table, application firmware, web UI, starter layout, four locomotives, locomotive images, and signal rules.

## Network modes

- With saved home-network credentials, the EX-CSB1 joins that Wi-Fi network.
- Without usable credentials, it starts its own access point.
- The LCD shows the address to open.
- Network settings are stored in ESP32 Preferences, not in exported layout files or the public firmware data set.

## First safety checks

- Keep the programming track electrically isolated from MAIN.
- Start with track power off.
- Verify turnout and signal addresses before operating physical accessories.
- Create a backup after initial configuration.

