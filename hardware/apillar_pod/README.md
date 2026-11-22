# DinMeter pod on the A-pillar grab-handle cap (JL)

## Current: one-piece wedge (`apillar_wedge.stl`, `make_wedge.py`)
Cap + wedge with the 1/32 DIN cutout. DinMeter long side along the cap, no turn. The upper (wide)
end is lifted off the pillar by TILT = 20 deg; the wedge is ~20 mm high at the top, ~0 at the bottom.
The body goes up to ~34 mm below the cap face at its lower end: measure the room behind the cap.
Clip goes on from inside with the cap off the car.

## Plain cutout (`apillar_cutout.stl`, `make_cutout.py`)
1/32 DIN hole 45.6 x 23.1 straight in the upper red cap, long side along the cap, centred at x 55
(wide end, wide end points up in the car). Inner ribs cleared 49 x 25 for the stock back clip; the
~2 mm shell is the panel. Needs ~35 mm free depth behind the cap face (body ~26 + plugs): measure
from the cap face to the bolt head before printing.

## Earlier concepts (kept for reference)

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
