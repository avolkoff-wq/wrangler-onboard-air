# DinMeter pod on the A-pillar grab-handle cap (JL)

Concept v1. Upper red cap of the driver-side grab handle carries a podium and a 1/32 DIN sleeve.
The screen stands truly vertical (long side up) and turns toward the driver.

- Base: [Jeep JL A pillar cap (with ball mount)](https://www.thingiverse.com/thing:4238093) by livesdE,
  file `A_pillar_cap-without_ball.stl`, CC BY-NC-SA. This remix keeps the same license.
- `apillar_base.stl`: cap + podium + boss. `apillar_sleeve.stl`: sleeve, cutout 45.6 x 23.1, front wall 2 mm
  for the stock back clip. Assembly: DinMeter in from the front, clip from the rear, plug the cable,
  slide the sleeve over the boss, two M3 x 8 self-tapping screws through the side walls.
- Cable: 9 mm channel from the boss into the cap cavity, then behind the pillar trim.
- `apillar_assembly.stl` / `apillar_assembly_car.stl`: preview only (cap frame / car frame).
- `make_pod.py` parameters: RAKE (pillar lean, 33 deg), UP_END (which cap end points up), EYE (driver eyes
  from the pod; z = 0 keeps the screen vertical), POD_X, LIFT.
- Material: ASA (PETG second). PLA softens under the windscreen in summer.
- `apillar_pod.stl`: concept v0 (screen along the cap), superseded.

Open: confirm this cap is the upper one and which end points up; real rake of the leg.
