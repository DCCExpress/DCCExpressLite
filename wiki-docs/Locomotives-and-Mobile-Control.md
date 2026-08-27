# Locomotives and mobile control

The locomotive editor stores each locomotive's stable ID, DCC address, name, maximum speed, image, functions, direction inversion, and optional actions.

![Mobile locomotive controller](images/mobile-controller.png)

## Driving

- Select a locomotive.
- Use the slider or preset speed buttons.
- Select Forward or Reverse.
- Use Stop for the selected locomotive.
- Use Emergency Stop for the complete command station.
- Function buttons F0–F27 support normal and momentary operation.

Direction inversion is applied server-side before DCC throttle commands are generated. Image mirroring is client-side and stored per browser, allowing phones on opposite sides of the layout to show the locomotive nose correctly for their viewpoint.

## Mobile layout overlay

The floating layout button opens a runtime-only track panel without editor or property panels.

![Mobile layout overlay](images/mobile-layout.png)

The floating controls provide:

- Center view
- Fit view
- Emergency stop
- Close layout panel

Route buttons use the same progress overlay and emergency-stop behaviour as the desktop layout.

## Multi-client operation

Multiple browsers can be connected simultaneously. Locomotive, turnout, accessory, sensor, signal, power, and emergency states are broadcast through WebSocket updates. A lost WebSocket connection is shown in red and reconnects automatically.

