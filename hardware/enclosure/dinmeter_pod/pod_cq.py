"""Wrangler Air box, solid model (CadQuery / OpenCascade).

One box for everything, a tower as narrow as the parts allow. The DinMeter sits in the TOP face,
screen up. Inside, from the bottom: Mini 3A Relay flat on the floor (its Grove plug needs 58 mm in
length, that sets the width), ADC v1.1 standing on its long edge against the front wall (VHB tape,
small ledge under it), then the zone for the DinMeter's Grove plugs and power terminal.
The cover is the back plate: taped to the tunnel wall, the body clicks onto it with two snap tabs
in the side walls. Car wires go in through a slot low in the left side wall of the body, with a
cable-tie anchor on the inside of that wall.

Axes: x = along the car (width), y = up, z = from the front face (z = 0) to the tunnel wall (z = D).

Writes:
  dinmeter_pod.step   body + cover as solids, opens in Fusion 360
  dinmeter_body.stl   for printing (print upside down, top face on the bed)
  dinmeter_back.stl   for printing (the cover, plate on the bed)

All sizes in mm. Change the numbers at the top and run:  python3 pod_cq.py
"""
import cadquery as cq

# ---------- parameters ----------
# Sizes of the bought parts are from M5Stack's official drawings and 3D shells (github.com/m5stack/M5_Hardware):
#   DinMeter K134: bezel 53 x 30 x 2.5, body 45.6 x 22.2, 20.4 deep behind the bezel, knob D12 x 8.6
#   Unit Relay U023: 48 x 24 x 8 shell, relay and 3.96 header stick out on top
#   Unit ADC v1.1 U013-V11: 32 x 24 x 8 shell
# The orange back clip of the DinMeter is used WITHOUT its four screw ears (snip them off): with the ears
# it is 38.6 mm across and would not fit a 32 mm deep box.
W, H, D = 70.0, 64.0, 32.0      # body outside: width (along the car), height, depth (out from the tunnel wall)
T = 2.0                         # side, front and bottom walls (PETG / ASA)
TOP = 3.0                       # top wall: the DinMeter clamps against it
R_OUT = 4.0                     # outside corner radius (DinMeter bezel corners)
CUT_W, CUT_H = 46.0, 22.8       # DinMeter body 45.6 x 22.2 + clearance (CUT_H runs along z)
CUT_Z = D / 2                   # cut-out centred on the top face
P = 3.0                         # cover plate
LIP_H, LIP_W, LIP_GAP = 2.0, 2.0, 0.3      # locating lip of the cover
TAB_W, TAB_T, TAB_L = 10.0, 1.4, 12.0      # snap tabs (in the side walls)
BARB, BARB_L = 0.9, 2.5
WIN_GAP = 0.4
RELAY = (48.0, 8.0, 24.0)       # Mini 3A Relay 48 x 24 x 8 (+ relay and 3.96 terminal on top), flat on the floor
RELAY_X, RELAY_Z = -3.0, 16.0   # relay centre: 3 mm free at the terminal end, Grove plug at the +x end
ADC = (32.0, 24.0, 8.0)         # ADC v1.1 32 x 24 x 8, standing on its long edge against the front wall, above the relay
ADC_X, ADC_Y = 7.0, -9.5        # ADC centre
RAIL = (1.2, 1.8)               # relay stop behind it: thickness, height
EXIT = (14.0, 8.0)              # car-wire slot in the left side wall (along z x along y): +12 V, GND, wire to K2, sensor cable
EXIT_Y, EXIT_Z = -16.0, 17.0    # slot centre, low, at the relay terminal end
ANCHOR = (5.0, 6.0, 10.0)       # cable-tie anchor on the inside of the left wall: depth from the wall, height, length along z
ANCHOR_Y, ANCHOR_Z = -1.0, 8.0  # above the slot, towards the front (the snap tab is at the back)
TIE = (3.4, 1.8)                # tunnel for a 2.5 mm cable tie
FLOOR = -H / 2 + T

# ---------- body ----------
body = (cq.Workplane("XY").rect(W, H).extrude(D)
        .edges("|Z").fillet(R_OUT)
        .faces("<Z").edges().fillet(1.5)
        .faces(">Z").edges().fillet(0.6))
ci_h = H - T - TOP
cavity = (cq.Workplane("XY").workplane(offset=T).center(0, (T - TOP) / 2).rect(W - 2 * T, ci_h).extrude(D)
          .edges("|Z").fillet(R_OUT - T))
body = body.cut(cavity)
# DinMeter cut-out in the top wall (XZ plane faces -Y: offset -(H/2+1) is above the top, extrude goes down)
cut = (cq.Workplane("XZ").workplane(offset=-(H / 2 + 1)).center(0, CUT_Z)
       .rect(CUT_W, CUT_H).extrude(TOP + 2))
body = body.cut(cut)
body = body.faces(">Y").edges(cq.selectors.BoxSelector((-CUT_W/2-1, H/2-0.1, CUT_Z-CUT_H/2-1), (CUT_W/2+1, H/2+0.1, CUT_Z+CUT_H/2+1))).chamfer(0.6)
# snap windows in the side walls, at mid height
for sx in (-1, 1):
    win = (cq.Workplane("XY").workplane(offset=D - TAB_L - WIN_GAP)
           .center(sx * (W / 2 - T / 2), 0).rect(T + 2, TAB_W + 2 * WIN_GAP).extrude(BARB_L + 2 * WIN_GAP))
    body = body.cut(win)
# relay: two stops on the floor, in front of and behind it
for zz in (RELAY_Z - RELAY[2] / 2 - 0.3 - RAIL[0] / 2, RELAY_Z + RELAY[2] / 2 + 0.3 + RAIL[0] / 2):
    body = body.union(cq.Workplane("XY").box(30.0, RAIL[1], RAIL[0]).translate((RELAY_X, FLOOR + RAIL[1] / 2, zz)))
# ADC: ledge on the front wall under it
body = body.union(cq.Workplane("XY").box(ADC[0] - 4, 1.4, 1.6)
                  .translate((ADC_X, ADC_Y - ADC[1] / 2 - 0.7, T + 0.8)))

# car-wire slot low in the left side wall (YZ plane, normal +X: start inside the wall, cut outwards with a negative extrude)
side = (cq.Workplane("YZ").workplane(offset=-W / 2 + T + 1).center(EXIT_Y, EXIT_Z)
        .slot2D(EXIT[0], EXIT[1], 90).extrude(-(T + 3)))
body = body.cut(side)
# cable-tie anchor on the inside of the left wall, above the slot, tunnel along y
a_d, a_h, a_l = ANCHOR
x_in = -W / 2 + T
anchor = cq.Workplane("XY").box(a_d, a_h, a_l).translate((x_in + a_d / 2, ANCHOR_Y, ANCHOR_Z)).edges("|Y").fillet(0.8)
tunnel = cq.Workplane("XY").box(TIE[1], a_h + 2, TIE[0]).translate((x_in + 2.2, ANCHOR_Y, ANCHOR_Z))
body = body.union(anchor.cut(tunnel))

# ---------- cover (back plate, taped to the tunnel wall) ----------
cover = (cq.Workplane("XY").rect(W, H).extrude(P)
         .edges("|Z").fillet(R_OUT)
         .faces("<Z").edges().fillet(1.0))
lip_o = W - 2 * T - 2 * LIP_GAP, ci_h - 2 * LIP_GAP
lip_c = (T - TOP) / 2
lip = (cq.Workplane("XY").workplane(offset=P).center(0, lip_c).rect(*lip_o).extrude(LIP_H)
       .edges("|Z").fillet(R_OUT - T - LIP_GAP)
       .cut(cq.Workplane("XY").workplane(offset=P).center(0, lip_c).rect(lip_o[0] - 2 * LIP_W, lip_o[1] - 2 * LIP_W).extrude(LIP_H)))
cover = cover.union(lip)
tx = lip_o[0] / 2
for sx in (-1, 1):
    cover = cover.cut(cq.Workplane("XY").workplane(offset=P).center(sx * (tx - LIP_W / 2), 0).rect(LIP_W + 0.2, TAB_W + 2).extrude(LIP_H))
    tab = cq.Workplane("XY").workplane(offset=P).center(sx * (tx - TAB_T / 2), 0).rect(TAB_T, TAB_W).extrude(TAB_L)
    x0 = sx * tx
    prof = [(x0, P + TAB_L - BARB_L), (x0 + sx * BARB, P + TAB_L - BARB_L), (x0, P + TAB_L)]
    barb = cq.Workplane("XZ").workplane(offset=-TAB_W / 2).polyline(prof).close().extrude(TAB_W)
    cover = cover.union(tab).union(barb)
# ---------- export ----------
asm = cq.Assembly()
asm.add(body, name="box_body", color=cq.Color(0.23, 0.26, 0.31))
asm.add(cover, name="box_cover", loc=cq.Location((0, 0, D + P), (0, 1, 0), 180), color=cq.Color(0.49, 0.55, 0.64))
asm.save("dinmeter_pod.step")
cq.exporters.export(body, "dinmeter_body.stl", tolerance=0.02, angularTolerance=0.1)
cq.exporters.export(cover, "dinmeter_back.stl", tolerance=0.02, angularTolerance=0.1)
bb = body.val().BoundingBox()
print("ok", round(bb.xlen, 1), round(bb.ylen, 1), round(bb.zlen, 1))
