# Gamepad control

DCCExpressLite can use a Bluetooth or USB game controller to operate locomotive controls from the browser. Controller selection and button assignments belong to the current browser only; they are not written to the EX-CSB1 and are not included in backup exports.

## Connect and select a controller

1. Pair the controller with the phone, tablet, or computer, or connect it through USB.
2. Open **Gamepad** from the DCCExpressLite home page.
3. Press a physical gamepad button so the browser exposes the controller.
4. Choose it in **Active controller**.
5. Open the Mobile Controller or a locomotive panel and select the locomotive to drive.

The Gamepad page shows the controller name, browser index, axes, buttons, pressed-button values, and an input-event log. These diagnostics make it possible to identify the physical button numbers used by a particular controller.

Browser and mobile operating-system support varies. If no controller appears, confirm that it is paired at operating-system level, press one of its buttons, keep the DCCExpressLite page active, and reopen the Gamepad page.

## Multiple controllers

Every connected controller appears in the **Active controller** list. Only the selected controller is allowed to issue commands on that client.

The browser saves the selected controller using its reported device name and index. It restores the selection after a reload and also recognizes the same controller if the browser assigns it a different index. If the selected controller disconnects, DCCExpressLite waits for it to return instead of silently handing control to another controller. Select another device manually if it should take over.

The browser Gamepad API does not expose a reliable hardware serial number. Two completely identical controllers may report the same name, so their browser index is also shown in the list to help distinguish them during the current session.

## Button assignments

Each physical button has a read-only selection list. Choose one of these actions:

- Speed +
- Speed −
- Forward
- Reverse
- Stop
- Emergency stop
- Function F0–F3

One function can belong to only one physical button. Assigning an already-used function to another button removes it from the old button. Clear a selection to disable that button. **Reset to defaults** restores the original mapping.

The default assignments are:

| Button | Action |
| --- | --- |
| B0 | Function F0 |
| B1 | Function F1 |
| B2 | Function F2 |
| B3 | Function F3 |
| B8 | Emergency stop |
| B9 | Stop |
| B12 | Speed + |
| B13 | Speed − |
| B14 | Reverse |
| B15 | Forward |

Button numbering is supplied by the browser and can differ between controller models. Use the live button display or input log instead of relying on the labels printed on the controller.

## Command behaviour

Gamepad commands trigger when a mapped button changes from released to pressed. Holding a button does not continuously repeat the action.

- **Speed +** and **Speed −** change speed by five steps and respect zero and the locomotive's configured maximum speed.
- **Forward** and **Reverse** use the same direction handling as the on-screen controls, including server-side direction inversion.
- **Stop** stops the locomotive selected in the receiving throttle panel.
- **Emergency stop** uses the same global emergency state as the other emergency buttons.
- **Function F0–F3** toggle the matching locomotive functions.

If both locomotive panels are visible in the Layout Editor, both panels currently receive gamepad actions. Close the panel that must not be controlled, or use the Mobile Controller when only one locomotive should receive commands.

## Storage and reset

The following settings are stored in browser `localStorage`:

- selected controller identity;
- button-to-action assignments.

This makes the setup client-specific: a phone, tablet, and desktop computer can use different controllers and mappings. Clearing the browser's site data also clears these settings. The **Reset to defaults** button resets button assignments but does not delete layout, locomotive, or EX-CSB1 data.
