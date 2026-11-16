"""Wrangler Air box, solid model (CadQuery / OpenCascade).

One box for everything, as small as the DinMeter allows: the top face is the size of the
DinMeter bezel (53 x 30) plus the walls. The DinMeter sits in the TOP face, screen up.
Inside, at the bottom: Mini 3A Relay lies flat on the floor, ADC v1.1 lies on top of it (VHB tape).
Between the modules and the DinMeter there is room for the Grove plugs and the power terminal.
The cover is the back plate: taped to the tunnel wall, the body clicks onto it with two snap tabs
in the side walls. Car wires go out through a slot in the cover, behind the tunnel trim, with a
cable-tie anchor on the inside of the cover.

Axes: x = along the car (width), y = up, z = from the front face (z = 0) to the tunnel wall (z = D).

Writes:
  dinmeter_pod.step   body + cover as solids, opens in Fusion 360
  dinmeter_body.stl   for printing (print upside down, top face on the bed)
  dinmeter_back.stl   for printing (the cover, plate on the bed)

All sizes in mm. Change the numbers at the top and run:  python3 pod_cq.py
"""
import cadquery as cq

# ---------- parameters ----------
W, H, D = 55.0, 63.0, 32.0      # body outside: width (along the car), height, depth (out from the tunnel wall)
T = 2.0                         # side, front and bottom walls (PETG / ASA)
TOP = 3.0                       # top wall: the DinMeter clamps against it
R_OUT = 4.0                     # outside corner radius (DinMeter bezel corners)
CUT_W, CUT_H = 45.2, 22.7       # 1/32 DIN cut-out + clearance (CUT_H runs along z)
CUT_Z = D / 2                   # cut-out centred on the top face
P = 3.0                         # cover plate
LIP_H, LIP_W, LIP_GAP = 2.0, 2.0, 0.3      # locating lip of the cover
TAB_W, TAB_T, TAB_L = 10.0, 1.4, 12.0      # snap tabs (in the side walls)
BARB, BARB_L = 0.9, 2.5
WIN_GAP = 0.4
RELAY = (48.4, 8.0, 24.4)       # Mini 3A Relay 48 x 24 x 8, flat on the floor, against the front wall
ADC = (32.4, 10.2, 24.4)        # ADC v1.1 32 x 24 x 10.2, flat on top of the relay
RAIL = (1.2, 1.8)               # relay stop behind it: thickness, height
EXIT = (14.0, 8.0)              # car-wire slot in the cover: +12 V, GND, wire to K2, sensor cable
EXIT_X, EXIT_Y = -8.0, -4.5     # slot centre on the cover (between the modules and the DinMeter)
ANCHOR = (10.0, 6.0, 5.0)       # cable-tie anchor on the inside of the cover: x, y, height
ANCHOR_X = 13.0
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
# relay stop on the floor behind the relay (the front wall is the front stop)
body = body.union(cq.Workplane("XY").box(30.0, RAIL[1], RAIL[0])
                  .translate((0, FLOOR + RAIL[1] / 2, T + 0.5 + RELAY[2] + RAIL[0] / 2)))

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
# the cover is mounted turned 180 deg about y, so its x is mirrored: build with -x
cover = cover.cut(cq.Workplane("XY").workplane(offset=-1).center(-EXIT_X, EXIT_Y).slot2D(EXIT[0], EXIT[1], 0).extrude(P + 2))
ax_, ay_, ah_ = ANCHOR
anchor = (cq.Workplane("XY").workplane(offset=P).center(-ANCHOR_X, EXIT_Y).rect(ax_, ay_).extrude(ah_)
          .faces(">Z").edges().fillet(0.8))
tunnel = cq.Workplane("XY").box(ax_ + 2, TIE[0], TIE[1]).translate((-ANCHOR_X, EXIT_Y, P + 2.2))
cover = cover.union(anchor.cut(tunnel))

# ---------- export ----------
asm = cq.Assembly()
asm.add(body, name="box_body", color=cq.Color(0.23, 0.26, 0.31))
asm.add(cover, name="box_cover", loc=cq.Location((0, 0, D + P), (0, 1, 0), 180), color=cq.Color(0.49, 0.55, 0.64))
asm.save("dinmeter_pod.step")
cq.exporters.export(body, "dinmeter_body.stl", tolerance=0.02, angularTolerance=0.1)
cq.exporters.export(cover, "dinmeter_back.stl", tolerance=0.02, angularTolerance=0.1)
bb = body.val().BoundingBox()
print("ok", round(bb.xlen, 1), round(bb.ylen, 1), round(bb.zlen, 1))
