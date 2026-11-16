# DinMeter harness: colours

DinMeter sits in its pod on the tunnel side wall, the board box sits by the compressor.
Between them run two cables, both about 2 m, together in one braided sleeve (10 mm):

1. **Grove cable 2 m** (M5Stack A034-E) from PORT.B. The plug goes into the DinMeter, the other end is cut off and goes into the board terminals.
2. **Power pair 2 × 0.5 mm², red and black** (silicone) from the DinMeter power terminal to J2b.

PORT.A (G13, K4, deflate valve) is not used yet: no deflate valve for now. Leave a draw string in the sleeve for a second Grove cable later.

## Grove cable, PORT.B

| Grove wire | Signal | Board end |
|---|---|---|
| red | +5V from the DinMeter to the board (sensor supply) | J2 row 2 (5V) |
| white | G1, sensor signal (analogue) | J2 row 4 (G1) |
| yellow | G2, K1 control (compressor relay, later the inflate valve) | J2 row 6 (G2) |
| black | GND, signal ground | J2b row 10 (GND) |

## Power pair

| Wire | Signal | DinMeter end | Board end |
|---|---|---|---|
| red | +12V AUX1 | green power terminal, "+" (upper) | J2b row 12 (+12V) |
| black | DinMeter supply return | green power terminal, "−" (lower) | J2b row 14 (DC−) |

The green terminal is the 2-pin pluggable screw plug (2.54 mm) that comes in the DinMeter box. It takes wires up to 0.5 mm².

## Two reds, two blacks

The red Grove wire is +5V, the red power wire is +12V. They end up next to each other in the board box.
Put a small label "5V" on the Grove red and "12V" on the power red before stripping them, and land them one at a time.
+12V on the 5V row would damage the DinMeter port and the sensor.

## Not connected in stage 1-2

| Where | Why |
|---|---|
| DinMeter PORT.A | deflate valve not fitted yet |
| J2 row 8 (G13) | same; the firmware keeps G13 LOW |
| J1 row 13 (→K4) | same |

## Rules

- One wire per terminal hole, ferrule on every wire at the board end.
- Grove plug in the DinMeter: secure with a dab of hot glue or a cable tie (M5 Grove cables have no latch).
- Strain relief (cable tie through the pod wall) before the cables leave through the back of the pod.
- Label both ends "M5".

Grove cable colours are not always standard on third-party cables. Go by the pin position (pin 1 GND, 2 5V, 3 yellow pin G2, 4 white pin G1, as printed on the DinMeter), not by the colour of a bought cable. The M5Stack cable is standard.
