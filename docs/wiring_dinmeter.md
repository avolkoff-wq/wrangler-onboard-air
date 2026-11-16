# DinMeter harness: colours

DinMeter sits in its pod on the tunnel side wall, the board box sits by the compressor.
Everything is ordered in one place, Galaxus.ch, no soldering:

| Part | Galaxus | Price |
|---|---|---|
| M5Stack DinMeter (StampS3) | [45362449](https://www.galaxus.ch/en/s1/product/m5stack-m5dinmeter-with-m5stamps3-single-board-computer-kits-45362449) | CHF 39.90 |
| M5Stack Grove cable 200 cm | [24083800](https://www.galaxus.ch/en/s1/product/m5stack-cable-4-pin-grove-200-cm-single-board-computer-kits-24083800) | CHF 8.05 |
| Donau single flex wire 0.25 mm², 10 m, orange | [21479733](https://www.galaxus.ch/en/s4/product/donau-single-flex-wire-025-10m-orange-10-m-cable-conductors-21479733) | CHF 7.35 |
| Donau single flex wire 0.25 mm², 10 m, blue | [21479728](https://www.galaxus.ch/en/s4/product/donau-single-flex-wire-025-10m-blue-10-m-cable-conductors-21479728) | ≈ CHF 7.35 |

The Grove plug goes into PORT.B, the cable runs 2 m to the board box, the far end is cut off and the 4 wires go into the terminals.
Orange and blue run beside it from the green power plug to J2b. Grove cable and power wires are held together by cable ties every 15 cm.
The Grove plug passes through the 10.7 × 5.4 slot in the back of the pod; the green power plug stays inside the pod.

PORT.A (G13, K4, deflate valve) is not used yet. A second Grove cable can be added later the same way.

## Harness colours

| Wire | Signal | Pod end | Board end |
|---|---|---|---|
| Grove red | +5V from the DinMeter to the board (sensor supply) | PORT.B | J2 row 2 (5V) |
| Grove white | G1, sensor signal (analogue) | PORT.B | J2 row 4 (G1) |
| Grove yellow | G2, K1 control (compressor relay, later the inflate valve) | PORT.B | J2 row 6 (G2) |
| Grove black | GND, signal ground | PORT.B | J2b row 10 (GND) |
| **orange 0.25** | +12V AUX1, DinMeter supply | green power terminal "+" (upper) | J2b row 12 (+12V) |
| **blue 0.25** | DinMeter supply return | green power terminal "−" (lower) | J2b row 14 (DC−) |

Power is orange and blue so it can never be mixed up with the red/black of the Grove (5V and GND).
The green power terminal is the 2-pin pluggable screw plug (2.54 mm) that comes in the DinMeter box.

## Not connected in stage 1-2

| Where | Why |
|---|---|
| DinMeter PORT.A | deflate valve not fitted yet |
| J2 row 8 (G13) | same; the firmware keeps G13 LOW |
| J1 row 13 (→K4) | same |

## Rules

- One wire per terminal hole. Thin wires (Grove, 0.25 mm²): strip 8 mm, fold the strands back once, then clamp.
- The M5 Grove cable has no latch: secure the plug in PORT.B with a dab of hot glue.
- No joins in the pod. Strain relief: tie the Grove cable and the power wires to one of the two anchors inside the cover with a 2.5 mm cable tie, leave a 3-4 cm slack loop to PORT.B, then the harness leaves through the 10.7 × 5.4 Grove slot in the back of the pod. Only the Grove plug goes through; the power wires are clamped into the green screw plug inside the pod.
- Label both ends "M5".

Grove cable colours are not always standard on third-party cables. Go by the pin position (pin 1 GND, 2 5V, 3 yellow pin G2, 4 white pin G1, as printed on the DinMeter), not by the colour of a bought cable. The M5Stack cable is standard.
