# Turnouts and routes

## Turnout operation

A normal turnout click sends the current element's linear accessory address through the server-side DCC-EX parser as an `<a address 0|1>` command. The physical DCC packet is placed on the MAIN output; the UI is not merely simulating the state.

The turnout state is cached on the EX-CSB1 and broadcast to all connected clients. A newly connected browser receives the cached state during its staged initial snapshot.

## Basic Route Button

A Route Button contains stable turnout IDs plus a required **Closed** or **Thrown** state for each turnout.

1. Add a Route Button.
2. Select it in Edit mode.
3. Use the turnout-selection property.
4. Select each left/right turnout and its required physical state.
5. Save the layout.

At runtime, pressing the button shows a route-progress overlay with emergency stop. Turnout commands are issued sequentially rather than as one blocking burst. The active route is coloured on the layout when its turnout states match.

## Automatic Route

An Automatic Route stores start and destination **block IDs**. The runtime graph finds a compatible path and calculates the required turnout states. Both referenced blocks must still exist and must not be the same block.

## Integrity protection

The global **CHECK** command reports:

- deleted or wrong-type turnout references;
- duplicate turnout references inside one route button;
- route buttons with no assigned turnouts;
- missing automatic-route start or destination blocks;
- identical start and destination blocks.

