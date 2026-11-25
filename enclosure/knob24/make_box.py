"""Box for the big-screen variant (owner idea 25.11.2026): a 2.4" TFT + EC11 knob board on the front,
the ESP32 2-relay board (DC5-60V) under it, one box, one cable gland.

    front (screen, knob, key)
    ---------------------------
    | TFT 2.4" | EC11  (o)     |   display board, screwed to 4 posts on the front panel
    |          | KEY   [ ]     |
    ---------------------------
    |  relay board 74.3x48.1   |   on the lid, held in corner pockets
    ---------------------------
    lid (4 x M3 into the corner lugs)

Sizes. Relay board: from the seller's drawing (74.3 x 48.1 mm, relays SRD-05VDC ~15.3 mm tall).
Display board: the seller gives no drawing. TFT glass = standard 2.4" 320x240 panel 60.26 x 42.72 mm,
active 48.96 x 36.72 mm; the PCB, hole and knob positions are ESTIMATED from the photos.
=> measure the real boards with a caliper and change the MEASURE block, then run again.
Writes knob24_body.stl, knob24_lid.stl and knob24.glb (colored, lid exploded, boards as ghosts) for knob24_3d.html."""
import numpy as np, trimesh
from manifold3d import Manifold, CrossSection, JoinType

# ---------------- MEASURE (mm) ----------------
DISP_L, DISP_W, DISP_T = 94.0, 50.0, 1.6      # display board PCB (estimate)
GLASS_L, GLASS_W, GLASS_T = 60.26, 42.72, 3.6  # 2.4" TFT module glass, sits on the PCB
GLASS_X = 5.0                                  # glass left edge from PCB left edge (pin header strip, estimate)
ACT_L, ACT_W = 48.96, 36.72                    # visible area
KNOB_X, KNOB_Y = DISP_L - 14.0, 17.0           # EC11 shaft centre on the PCB (estimate)
KEY_X, KEY_Y = DISP_L - 14.0, 38.0             # tact key centre (estimate)
DISP_HOLES = [(3.0, 3.0), (DISP_L - 3.0, 3.0), (3.0, DISP_W - 3.0), (DISP_L - 3.0, DISP_W - 3.0)]  # M2.5 (estimate)
BEHIND_DISP = 3.0                              # wires soldered to the header, parts on the back of the display PCB
RELAY_L, RELAY_W, RELAY_T = 74.3, 48.1, 1.6    # relay board (seller drawing)
RELAY_UP = 17.5                                # tallest part above its PCB (relay 15.3 + terminals)
RELAY_UNDER = 3.0                              # solder joints under the relay PCB
# ---------------- box ----------------
WALL, FRONT, LIDT, GAP, CLR, R = 2.2, 2.0, 2.4, 2.0, 1.0, 4.0
GLAND_D = 12.4                                 # M12 cable gland (the M-series glands from the 18.11 order)

IN_L = max(DISP_L, RELAY_L) + 2 * CLR
IN_W = max(DISP_W, RELAY_W) + 2 * CLR
IN_H = GLASS_T + DISP_T + BEHIND_DISP + GAP + RELAY_UP + RELAY_T + RELAY_UNDER   # lid top face to front inner face
OUT_L, OUT_W = IN_L + 2 * WALL, IN_W + 2 * WALL
BODY_H = IN_H + FRONT                          # body is open at the back, the lid closes it
LUG_R, LUG_HOLE, LUG_H = 4.5, 2.6, 10.0        # corner lugs, M3 self-tapping

def rbox(l, w, h, r):
    cs = CrossSection.square([l - 2 * r, w - 2 * r]).translate([r, r]).offset(r, JoinType.Round, circular_segments=48)
    return Manifold.extrude(cs, h)
def cyl(r, h, x, y, z=0):
    return Manifold.cylinder(h, r, r, 48).translate([x, y, z])
def cube(l, w, h, x, y, z):
    return Manifold.cube([l, w, h]).translate([x, y, z])

# board origins in box coordinates (x, y from the outer corner; z from the bottom of the body = lid top face)
dx = WALL + (IN_L - DISP_L) / 2; dy = WALL + (IN_W - DISP_W) / 2
disp_pcb_z = IN_H - GLASS_T - DISP_T           # bottom face of the display PCB
rx = WALL + (IN_L - RELAY_L) / 2; ry = WALL + (IN_W - RELAY_W) / 2
relay_pcb_z = RELAY_UNDER                      # bottom face of the relay PCB, above the lid top face

# ---- body ----
lug_xy = [(0, 0), (OUT_L, 0), (0, OUT_W), (OUT_L, OUT_W)]
body = rbox(OUT_L, OUT_W, BODY_H, R)
for x, y in lug_xy:
    body = body + cyl(LUG_R, LUG_H, x, y, BODY_H - FRONT - LUG_H + FRONT)   # lugs at the front half, outside the corners
body = body - rbox(IN_L, IN_W, IN_H, max(R - WALL, 0.6)).translate([WALL, WALL, -0.01])
for x, y in lug_xy:
    body = body - cyl(LUG_HOLE / 2, LUG_H + 1, x, y, BODY_H - LUG_H - 0.5)
# lugs: the lid screws go into them from the back, so they run the full height on the back side
for x, y in lug_xy:
    body = body + (cyl(LUG_R, BODY_H, x, y) - cyl(LUG_HOLE / 2, BODY_H + 1, x, y, -0.5) - rbox(IN_L, IN_W, IN_H, 0.6).translate([WALL, WALL, -0.01]))
# screen window with a 45 deg chamfer outward
wx = dx + GLASS_X + (GLASS_L - ACT_L) / 2; wy = dy + (DISP_W - ACT_W) / 2
wl, ww = ACT_L + 1, ACT_W + 1
win = Manifold.extrude(CrossSection.square([wl, ww], center=True), FRONT + 0.02, scale_top=((wl + 2 * FRONT) / wl, (ww + 2 * FRONT) / ww))
body = body - win.translate([wx + ACT_L / 2, wy + ACT_W / 2, BODY_H - FRONT - 0.01])
# glass pocket on the inside of the front, so the glass lies flat against the panel
body = body - cube(GLASS_L + 0.6, GLASS_W + 0.6, 0.6, dx + GLASS_X - 0.3, dy + (DISP_W - GLASS_W) / 2 - 0.3, IN_H - 0.6)
# knob hole and a flexure key (U slot, nub under the tongue presses the tact key)
body = body - cyl(3.9, FRONT + 2, dx + KNOB_X, dy + KNOB_Y, BODY_H - FRONT - 1)
kx, ky = dx + KEY_X, dy + KEY_Y
slot = cube(11, 11, FRONT + 2, kx - 5.5, ky - 5.5, BODY_H - FRONT - 1) - cube(9, 9, FRONT + 4, kx - 4.5, ky - 4.5, BODY_H - FRONT - 2) - cube(9, 1.2, FRONT + 4, kx - 4.5, ky - 5.6, BODY_H - FRONT - 2)
body = body - slot
key_top = IN_H; key_gap = GLASS_T - 3.5   # tact key cap top ~3.5 mm above the PCB (TS 6x6x5) -> nub length
body = body + cyl(1.6, max(key_gap + 0.3, 0.6), kx, ky, key_top - max(key_gap + 0.3, 0.6))
# 4 posts for the display PCB (from the front inner face down to the PCB back face is GLASS_T)
for hx, hy in DISP_HOLES:
    p = cyl(2.6, GLASS_T, dx + hx, dy + hy, IN_H - GLASS_T) - cyl(1.1, GLASS_T + 1, dx + hx, dy + hy, IN_H - GLASS_T - 0.5)
    body = body + p
# cable gland in the end wall under the knob, at the relay level
gz = relay_pcb_z + RELAY_T + RELAY_UP / 2
body = body - Manifold.cylinder(WALL + 2, GLAND_D / 2, GLAND_D / 2, 64).rotate([0, 90, 0]).translate([OUT_L - WALL - 1, OUT_W / 2, gz])

# ---- lid ----
lid = rbox(OUT_L, OUT_W, LIDT, R)
for x, y in lug_xy:
    lid = lid + cyl(LUG_R, LIDT, x, y)
for x, y in lug_xy:
    lid = lid - cyl(1.7, LIDT + 1, x, y, -0.5) - Manifold.cylinder(1.6, 1.7, 3.2, 32).translate([x, y, -0.01])   # M3 countersink from outside
# lip that keys into the body opening
lid = lid + (rbox(IN_L - 0.4, IN_W - 0.4, 1.6, 0.8) - rbox(IN_L - 3.6, IN_W - 3.6, 2, 0.6).translate([1.6, 1.6, -0.2])).translate([WALL + 0.2, WALL + 0.2, LIDT])
# corner pockets for the relay board: 4 L blocks, board sits on a 3 mm shelf, RELAY_UNDER above the lid
for cx, cy, sx, sy in [(rx, ry, 1, 1), (rx + RELAY_L, ry, -1, 1), (rx, ry + RELAY_W, 1, -1), (rx + RELAY_L, ry + RELAY_W, -1, -1)]:
    blk = cube(7, 7, RELAY_UNDER + RELAY_T + 1.2, 0, 0, 0)
    blk = blk - cube(7, 7, RELAY_T + 2, 1.8, 1.8, RELAY_UNDER)          # step the board drops into
    blk = blk - cube(3.2, 3.2, RELAY_UNDER + 0.01, 1.8, 1.8, 0) if False else blk
    m = blk.mirror([1, 0, 0]) if sx < 0 else blk
    m = m.mirror([0, 1, 0]) if sy < 0 else m
    lid = lid + m.translate([cx - 1.8 * sx, cy - 1.8 * sy, LIDT])
# vent / wire slots are not needed: the relays switch coils only (compressor relay, valve)

def to_tm(m):
    mesh = m.to_mesh(); t = trimesh.Trimesh(mesh.vert_properties[:, :3], mesh.tri_verts); t.merge_vertices(); parts = t.split(only_watertight=False); return max(parts, key=lambda q: len(q.faces)) if len(parts) > 1 else t
b, l = to_tm(body), to_tm(lid)
b.export("knob24_body.stl"); l.export("knob24_lid.stl")
print(f"inside {IN_L:.1f} x {IN_W:.1f} x {IN_H:.1f} mm, outside {OUT_L:.1f} x {OUT_W:.1f} x {BODY_H + LIDT:.1f} mm (+ lugs R{LUG_R}, + knob)")
print("watertight", b.is_watertight, l.is_watertight)

# ---- GLB for the 3D page: body, lid lowered, the two boards and the knob ----
def boxm(l, w, h, x, y, z): return trimesh.creation.box([l, w, h]).apply_translation([x + l / 2, y + w / 2, z + h / 2])
def col(m, rgba): m.visual.face_colors = rgba; return m
LIFT = 45
items = {
  "body": col(b.copy().apply_translation([0, 0, LIDT]), [111, 122, 90, 255]),
  "lid": col(l.copy().apply_translation([0, 0, -LIFT]), [58, 61, 54, 255]),
  "display_pcb": col(boxm(DISP_L, DISP_W, DISP_T, dx, dy, LIDT + disp_pcb_z), [31, 95, 191, 255]),
  "tft": col(boxm(GLASS_L, GLASS_W, GLASS_T, dx + GLASS_X, dy + (DISP_W - GLASS_W) / 2, LIDT + disp_pcb_z + DISP_T), [15, 15, 18, 255]),
  "knob": col(trimesh.creation.cylinder(radius=7, height=16, sections=48).apply_translation([dx + KNOB_X, dy + KNOB_Y, LIDT + BODY_H + 8]), [25, 25, 25, 255]),
  "relay_pcb": col(boxm(RELAY_L, RELAY_W, RELAY_T, rx, ry, LIDT + relay_pcb_z - LIFT), [31, 95, 191, 255]),
  "relay1": col(boxm(19, 15.5, 15.3, rx + 36, ry + 30, LIDT + relay_pcb_z + RELAY_T - LIFT), [59, 130, 246, 255]),
  "relay2": col(boxm(19, 15.5, 15.3, rx + 54, ry + 30, LIDT + relay_pcb_z + RELAY_T - LIFT), [59, 130, 246, 255]),
  "esp32": col(boxm(18, 25.5, 3.2, rx + 3, ry + 18, LIDT + relay_pcb_z + RELAY_T - LIFT), [40, 40, 40, 255]),
}
sc = trimesh.Scene()
for n, m in items.items(): sc.add_geometry(m, node_name=n, geom_name=n)
sc.apply_transform(trimesh.transformations.rotation_matrix(-np.pi / 2, [1, 0, 0]))   # Z up -> Y up for three.js
sc.export("knob24.glb")
