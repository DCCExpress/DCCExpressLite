# Troubleshooting

## The page loads but shows WS LOST

- Wait for automatic reconnect.
- Verify the browser is still on the same EX-CSB1 address shown on the LCD.
- Check free heap, largest free block, client count, and dropped WebSocket counters in Info.
- Close unused browser tabs if many clients are connected.

## Mobile page does not load

- Confirm the phone is on the same Wi-Fi network.
- Open the numeric address shown by the EX-CSB1 LCD.
- Do not expect the mobile browser to run the USB Web Installer; installation requires desktop Chromium.

## A turnout moves in the UI but not physically

- Confirm MAIN power is on.
- Verify the current turnout element address.
- Verify the decoder uses linear accessory addressing.
- Test direction 0 and direction 1 from Decoder Programming → Accessory.

## A signal does not change

- Confirm Signal Logic is enabled and running.
- Check that the rule's stable turnout/sensor IDs pass validation.
- Verify the signal start address, address length, and aspect bit values.
- Remember that unchanged aspects are not retransmitted continuously.
- Restore MAIN power to force a fresh evaluation.

## Reset reason reports watchdog

- Record the reset reason, free heap, minimum free heap, largest block, client count, and queue counters.
- Reproduce with one, two, and three clients without repeatedly refreshing all pages at once.
- Report the firmware version and serial boot log.

## Integrity errors after editing

An automation still references an element that was deleted, duplicated, or replaced with another type. Recreate the reference in the corresponding Route Button, Automatic Route, or Signal Logic editor; do not manually copy IDs between JSON files.

