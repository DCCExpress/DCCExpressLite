# Layout elements

## Track

- Straight
- End
- Corner
- Curve
- Crossing
- Left turnout
- Right turnout
- Double turnout

Track elements define the visible railway. Rotation changes their geometric connections. Turnout elements additionally store the physical accessory address and logical closed/thrown mapping.

## Control and information

- **Sensor** — reads a DCC-EX sensor address and can be used by signal rules.
- **Signal** — displays an aspect and writes an aspect bit pattern to consecutive accessory addresses.
- **Block** — represents an occupancy/reservation section and can hold a locomotive assignment.
- **Route Button** — stores an ordered set of turnout element IDs and required physical states.
- **Automatic Route** — requests a path between start and destination block IDs.
- **Label** — centered text with offsets measured from the element centre.
- **Level Crossing** — runtime crossing representation.

Every element has a stable generated ID. Automation references the ID rather than copying the DCC address. Changing a turnout address therefore preserves route and signal references.

## Block occupancy

In runtime mode, select a Block element to open the locomotive picker. Assigning a locomotive to a new block automatically removes it from its previous block, so one locomotive cannot occupy two blocks at the same time. Use the remove action to clear the selected block.

Block assignments are shared with every connected browser and saved by the EX-CSB1 in `/runtime-state.json`. The saved assignments are restored after a restart. References to blocks that no longer exist in `layout.json` are removed automatically during startup. Cached turnout states are stored in the same runtime-state file.

Block occupancy is runtime data and is deliberately not written into `layout.json`. The normal release-independent export currently backs up layout configuration, locomotives, images, and Signal Logic rules; copy `/runtime-state.json` separately through the Files page only when the live block and turnout state must also be preserved.

## Address rules

- DCC accessory elements use linear addresses.
- Signal address length is configurable from 1 to 8; the normal DigiSignal setup uses 5.
- Avoid overlapping signal output ranges unless the hardware is intentionally shared.
- Run the global integrity checker after deleting or replacing elements.
