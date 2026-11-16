"""DinMeter pod, solid model (CadQuery / OpenCascade).

Writes:
  dinmeter_pod.step   both parts as solids, opens in Fusion 360 (Insert > Insert/Upload, or File > Open)
  dinmeter_body.stl   for printing
  dinmeter_back.stl   for printing (the cover: taped to the tunnel wall, the body clicks onto it)

All sizes in mm. Change the numbers at the top and run:  python3 pod_cq.py
"""
import cadquery as cq

# ---------- parameters ----------
W, H, D = 60.0, 38.0, 45.0      # body outside: width, height, depth (front face at z=0)
T = 2.5                         # side walls
F = 3.0                         # front wall (DinMeter clamps against it)
R_OUT = 4.0                     # outside corner radius
CUT_W, CUT_H = 45.2, 22.7       # 1/32 DIN cut-out + clearance
SLOT_W, SLOT_H = 10.7, 5.4      # Grove plug slot (HY2.0-4P 9.9 x 4.6 + 0.4 per side)
P = 3.0                         # cover plate
LIP_H, LIP_W, LIP_GAP = 2.0, 2.0, 0.3      # locating lip of the cover
ANCHOR = (10.0, 6.0, 5.0)       # cable-tie anchor on the cover: x, y, height
ANCHOR_Y = 9.0                  # anchor centre from the slot centre
TIE = (3.4, 1.8)                # tunnel for a 2.5 mm cable tie
# snap fit, no screws: two flexible tabs on the cover click into windows in the long walls of the body.
# The cover is taped to the tunnel wall; the body with the DinMeter clicks onto it.
# To take it off: press both barbs in through the windows (small screwdriver) and pull.
TAB_W, TAB_T, TAB_L = 10.0, 1.4, 12.0      # tab width, thickness, length into the body (bending strain < 1 % for PETG/ASA)
BARB, BARB_L = 0.9, 2.5                     # barb height (outwards) and length at the tab tip
WIN_GAP = 0.4                               # clearance around the barb in the window
# DinMeter is held by its own back clip from the kit, clamping against the front wall F.

# ---------- body ----------
body = (cq.Workplane("XY").rect(W, H).extrude(D)
        .edges("|Z").fillet(R_OUT)
        .faces("<Z").edges().fillet(1.5)          # soft front edge
        .faces(">Z").edges().fillet(0.6))
cavity = (cq.Workplane("XY").workplane(offset=F).rect(W - 2 * T, H - 2 * T).extrude(D)
          .edges("|Z").fillet(R_OUT - T))
body = body.cut(cavity)
# DinMeter cut-out with a small chamfer on the outside
cut = cq.Workplane("XY").workplane(offset=-1).rect(CUT_W, CUT_H).extrude(F + 2)
body = body.cut(cut)
body = body.faces("<Z").edges(cq.selectors.BoxSelector((-CUT_W/2-1, -CUT_H/2-1, -0.1), (CUT_W/2+1, CUT_H/2+1, 0.1))).chamfer(0.6)
# snap windows in the two long walls (top / bottom), matching the cover tabs
for sy in (-1, 1):
    win = (cq.Workplane("XY").workplane(offset=D - TAB_L - WIN_GAP)
           .center(0, sy * (H / 2 - T / 2)).rect(TAB_W + 2 * WIN_GAP, T + 2).extrude(BARB_L + 2 * WIN_GAP))
    body = body.cut(win)
# spare Grove slot in the bottom wall, near the back
# XZ plane faces -Y: start 1 mm inside the bottom wall and cut outwards through it
slot_b = (cq.Workplane("XZ").workplane(offset=H / 2 - T - 1).center(0, D - 11)
          .slot2D(SLOT_W, SLOT_H, 0).extrude(T + 3))
body = body.cut(slot_b)

# ---------- cover ----------
cover = (cq.Workplane("XY").rect(W, H).extrude(P)
         .edges("|Z").fillet(R_OUT)
         .faces("<Z").edges().fillet(1.0))
lip_o = W - 2 * T - 2 * LIP_GAP, H - 2 * T - 2 * LIP_GAP
lip = (cq.Workplane("XY").workplane(offset=P).rect(*lip_o).extrude(LIP_H)
       .edges("|Z").fillet(R_OUT - T - LIP_GAP)
       .cut(cq.Workplane("XY").workplane(offset=P).rect(lip_o[0] - 2 * LIP_W, lip_o[1] - 2 * LIP_W).extrude(LIP_H)))
cover = cover.union(lip)
# snap tabs: cut the lip where the tabs stand, then add the tabs with a barb at the tip
ty = lip_o[1] / 2                                   # outer face of a tab = outer face of the lip
for sy in (-1, 1):
    cover = cover.cut(cq.Workplane("XY").workplane(offset=P).center(0, sy * (ty - LIP_W / 2)).rect(TAB_W + 2, LIP_W + 0.2).extrude(LIP_H))
    tab = cq.Workplane("XY").workplane(offset=P).center(0, sy * (ty - TAB_T / 2)).rect(TAB_W, TAB_T).extrude(TAB_L)
    # barb: flat catch face towards the cover, ramp towards the tip (profile in the YZ plane, extruded along X)
    y0 = sy * ty
    prof = [(y0, P + TAB_L - BARB_L), (y0 + sy * BARB, P + TAB_L - BARB_L), (y0, P + TAB_L)]
    barb = cq.Workplane("YZ").workplane(offset=-TAB_W / 2).polyline(prof).close().extrude(TAB_W)
    cover = cover.union(tab).union(barb)
# Grove slot in the middle
cover = cover.cut(cq.Workplane("XY").workplane(offset=-1).slot2D(SLOT_W, SLOT_H, 0).extrude(P + 2))
# cable-tie anchor with a tunnel
ax_, ay_, az_ = ANCHOR
anchor = (cq.Workplane("XY").workplane(offset=P).center(0, ANCHOR_Y).rect(ax_, ay_).extrude(az_)
          .faces(">Z").edges().fillet(0.8))
tunnel = (cq.Workplane("YZ").workplane(offset=-ax_).center(ANCHOR_Y, P + 2.2).rect(TIE[0], TIE[1]).extrude(2 * ax_))
cover = cover.union(anchor.cut(tunnel))

# ---------- export ----------
asm = cq.Assembly()
asm.add(body, name="pod_body", color=cq.Color(0.23, 0.26, 0.31))
asm.add(cover, name="pod_cover", loc=cq.Location((0, 0, D + P), (0, 1, 0), 180), color=cq.Color(0.49, 0.55, 0.64))
asm.save("dinmeter_pod.step")
cq.exporters.export(body, "dinmeter_body.stl", tolerance=0.02, angularTolerance=0.1)
cq.exporters.export(cover, "dinmeter_back.stl", tolerance=0.02, angularTolerance=0.1)
print("ok")
