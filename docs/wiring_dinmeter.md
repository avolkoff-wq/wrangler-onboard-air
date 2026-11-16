# DinMeter harness: colours

DinMeter sits in its pod on the tunnel side wall, the board box sits by the compressor.
Between them runs one shielded 8-core cable, LiYCY 8 x 0.25 mm² (cores coloured to DIN 47100).

## Main cable, pod to board box

| Core (DIN 47100) | Signal | Pod end | Board end |
|---|---|---|---|
| white | G1, sensor signal (analogue) | PORT.B, white Grove wire | J2 row 4 (G1) |
| green | G2, K1 control (relay / inflate valve) | PORT.B, yellow Grove wire | J2 row 6 (G2) |
| yellow | G13, K4 control (deflate valve) | PORT.A, yellow Grove wire | J2 row 8 (G13) |
| pink | +5V from the DinMeter to the board (sensor supply) | PORT.B, red Grove wire | J2 row 2 (5V) |
| brown | GND, signal ground | PORT.B, black Grove wire | J2b row 10 (GND) |
| red | +12V AUX1, DinMeter supply | DinMeter power terminal + | J2b row 12 (+12V) |
| blue | DC−, DinMeter supply return | DinMeter power terminal − | J2b row 14 (DC−) |
| grey | spare | cut, insulate | cut, insulate |
| shield | screen | cut back, insulate | twin ferrule together with brown, J2b row 10 |

## Grove cables inside the pod (short, 20-25 cm)

| Port | Black | Red | Yellow | White |
|---|---|---|---|---|
| PORT.B | GND → brown | 5V → pink | G2 → green | G1 → white |
| PORT.A | not used, insulate | not used, insulate | G13 → yellow | G15, not used, insulate |

Grove cable colours are not always standard. Go by the pin position in the plug (pin 1 GND, 2 5V, 3 yellow pin, 4 white pin, as printed on the DinMeter), not by the colour of a bought cable.

## Rules

- One core per terminal hole. Shield and brown share a twin ferrule.
- Shield is earthed at the board end only, so the sensor signal (white) is not disturbed by the relay lines.
- Ferrules on every core at the board end.
- Joins in the pod: solder + heat-shrink, then a strain relief (cable tie through the pod wall) before the cable leaves through the back.
- Label both ends of the main cable "M5".
