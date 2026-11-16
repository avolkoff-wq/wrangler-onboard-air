# DinMeter harness: colours

DinMeter sits in its pod on the tunnel side wall, the board box sits by the compressor.
Everything is ordered in one place, BerryBase.ch:

| Part | BerryBase no. | Price |
|---|---|---|
| M5Stack DinMeter v1.1 | M5-K134-V11 | CHF 29.50 |
| seeed Grove cable, latching ("fixierend"), 20 cm, 5 pcs | SE-110990027 | CHF 1.65 |
| Donau stranded wire 0.14 mm², 10 colours × 10 m | E-118-MIX | CHF 15.45 |
| Heat-shrink set, coloured | Schrumpfschlauch-Set bunt | ≈ CHF 3 |

How it is built: one latching Grove cable is cut in the middle; the plug end (10 cm) goes into PORT.B inside the pod and is soldered to 0.14 mm² wires of the same colours. Those wires, plus the power pair, run about 2 m to the board box, twisted together and held by cable ties every 15 cm.

PORT.A (G13, K4, deflate valve) is not used yet. The green wire for it is laid now and left coiled at both ends.

## Harness colours

| Wire 0.14 mm² | Signal | Pod end | Board end |
|---|---|---|---|
| red | +5V from the DinMeter to the board (sensor supply) | Grove PORT.B, red | J2 row 2 (5V) |
| white | G1, sensor signal (analogue) | Grove PORT.B, white | J2 row 4 (G1) |
| yellow | G2, K1 control (compressor relay, later the inflate valve) | Grove PORT.B, yellow | J2 row 6 (G2) |
| black | GND, signal ground | Grove PORT.B, black | J2b row 10 (GND) |
| **orange** | +12V AUX1, DinMeter supply | green power terminal "+" (upper) | J2b row 12 (+12V) |
| **blue** | DinMeter supply return | green power terminal "−" (lower) | J2b row 14 (DC−) |
| green | G13, K4 (deflate valve), for later | coiled, not connected | coiled, not connected |

Power is orange and blue so it can never be mixed up with the red/black of the Grove (5V and GND).
The green power terminal is the 2-pin pluggable screw plug (2.54 mm) that comes in the DinMeter box.

## Not connected in stage 1-2

| Where | Why |
|---|---|
| DinMeter PORT.A | deflate valve not fitted yet |
| J2 row 8 (G13) | same; the firmware keeps G13 LOW |
| J1 row 13 (→K4) | same |

## Rules

- One wire per terminal hole. 0.14 mm² is too thin for a ferrule: strip 8 mm, fold the strands back once, then clamp.
- The latching Grove plug clicks into PORT.B; no glue needed.
- Joins in the pod: solder, then heat-shrink. Strain relief (cable tie) before the harness leaves through the 10.7 × 5.4 Grove slot in the back of the pod. Only the Grove plug goes through; the power wires are clamped into the green screw plug inside the pod.
- Label both ends "M5".

Grove cable colours are not always standard on third-party cables. Go by the pin position (pin 1 GND, 2 5V, 3 yellow pin G2, 4 white pin G1, as printed on the DinMeter), not by the colour of a bought cable. The M5Stack cable is standard.
