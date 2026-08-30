# Device configuration

The **Device Configuration** page defines external I²C hardware that DCC-EX creates during EX-CSB1 startup. The configuration is stored in LittleFS as `/devices.json`, included in Export/Import backups, and restored before browser clients begin controlling the layout.

Supported dynamic devices in alpha6:

| Device | Channels | Typical I²C range | Purpose |
| --- | ---: | --- | --- |
| PCA9685 | 16 | `0x40`–`0x7F` | PWM and servo outputs |
| MCP23017 | 16 | `0x20`–`0x27` | Digital inputs and outputs |
| PCF8574 | 8 | `0x20`–`0x27` | Digital inputs and outputs |
| PCF8575 | 16 | `0x20`–`0x27` | Digital inputs and outputs |

## Device list and status

Select a device in the left list to open its pins in the wider right panel. The page shows:

- the I²C address and allocated VPIN range;
- configuration enabled/disabled state;
- WebSocket connection state;
- live HAL online/offline state from the firmware;
- unsaved changes that require **Apply & restart**.

Use the scan button to list currently detected I²C addresses. A saved device may be valid but offline when its address is not detected, wiring is incorrect, or the board has no power.

## PCA9685 channels

Each channel has independently saved:

- **OFF position** and **ON position** from `0` to `4095`;
- movement time in milliseconds;
- **Keep PWM active after movement**;
- manual **OFF** and **ON** test buttons.

Testing uses the configured analog servo endpoints rather than a Boolean VPIN command. Save and restart after changing channel configuration before relying on the test result.

When Signal Logic is enabled, the page displays a warning. Automation may legitimately write the same VPIN immediately after a manual test and therefore appear to switch the output back.

## Digital input pins

Choose **Input / DCC-EX sensor**, then assign a unique Sensor ID. At boot the firmware creates the equivalent of:

```text
<S SENSOR_ID VPIN PULLUP>
```

MCP23017 inputs can enable their internal pull-up. PCF8574 and PCF8575 inputs use the device's weak pull-up behavior.

The pin card displays a live state badge:

- **ACTIVE** — DCC-EX reported `<Q SENSOR_ID>`;
- **INACTIVE** — DCC-EX reported `<q SENSOR_ID>`;
- **UNKNOWN** — no state has been received yet or WebSocket is disconnected.

Sensor states are cached by the firmware as compact 16-bit groups. A new or refreshed client receives one bounded snapshot, then individual `sensorChanged` events. This keeps the layout, mobile overlay, and Device Configuration page synchronized.

## Digital output pins

Choose **Output / DCC-EX output**, assign a unique Output ID, and configure inversion and initial state. The firmware creates the output during boot and the page tests it with:

```text
<Z OUTPUT_ID 0>
<Z OUTPUT_ID 1>
```

## Applying changes safely

The firmware validates I²C address conflicts, VPIN overlaps, duplicate sensor/output IDs, channel ranges, and driver-specific settings. **Apply & restart** saves the document atomically and restarts the EX-CSB1 so HAL devices are built in a predictable order.

Before applying:

1. turn track power off if outputs can move physical hardware;
2. create an Export/Import backup;
3. verify I²C addresses and non-overlapping VPIN ranges;
4. remember that changing an ID can invalidate layout or Signal Logic references.
