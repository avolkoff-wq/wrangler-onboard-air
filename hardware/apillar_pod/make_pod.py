"""DinMeter pod on the Jeep JL A-pillar grab-handle cap (upper red cap, driver side).

Base: "Jeep JL A pillar cap (with ball mount)" by livesdE, Thingiverse thing:4238093,
CC BY-NC-SA. This remix (ball removed, podium + DinMeter pod added) is under the same
license: non-commercial, attribution, share alike.

Two printed parts:
  apillar_base.stl   cap + podium + a boss that the sleeve slides over
  apillar_sleeve.stl 1/32 DIN sleeve: DinMeter goes in from the front, the stock back clip
                     goes on from the rear, then the sleeve slides onto the boss and two
                     M3 screws through the side walls hold it.
The screen stands truly vertical in the car (long side up) and turns toward the driver.

Frames. Cap: X along the cap (narrow end x~0, wide end x~82), Y across, Z out of the visible
face. Car: X forward, Y left, Z up. Left (driver) A-pillar, left-hand drive.
Run: python3 make_pod.py
"""
import numpy as np
import manifold3d as mf
import trimesh

# ---------------- parameters ----------------
RAKE = 33.0          # grab-handle leg leans back from vertical, deg (JL screen ~57 deg from horizontal)
UP_END = "narrow"    # which end of the cap points up the pillar: "narrow" or "wide"
EYE = (-500.0, -350.0, 0.0)  # driver eyes from the pod, car frame mm (back, inboard, down); z=0 keeps the screen vertical
POD_X = 45.0         # pod centre along the cap
LIFT = 6.0           # gap between cap face and the pod rear at its closest point
EMBED = 0.6          # podium sinks into the cap shell (shell ~2 mm) so it prints as one part

CUT_W, CUT_H = 45.6, 23.1      # 1/32 DIN cutout 45 x 22.5 + print clearance
FRONT = 2.0                    # front wall = panel thickness for the DinMeter back clip
WALL = 2.4
DEPTH = 46.0                   # body ~26 behind the panel + Grove plugs + boss
BOSS = 6.0                     # how deep the base boss enters the sleeve
INNER_W, INNER_H = 49.0, 27.0  # room for the clip arms around the body
CABLE_D = 9.0
SCREW_D, PILOT_D = 3.4, 2.6    # M3 clearance in the sleeve, pilot in the boss

CAP = "A_pillar_cap-without_ball.stl"


def to_manifold(tm):
    return mf.Manifold(mf.Mesh(vert_properties=np.asarray(tm.vertices, np.float32),
                               tri_verts=np.asarray(tm.faces, np.uint32)))


def to_trimesh(m):
    g = m.to_mesh()
    t = trimesh.Trimesh(g.vert_properties[:, :3], g.tri_verts, process=True)
    t.merge_vertices()
    return t


def unit(v):
    v = np.asarray(v, float)
    return v / np.linalg.norm(v)


cap_tm = trimesh.load(CAP)
cap = to_manifold(cap_tm)
cy = float((cap_tm.bounds[0][1] + cap_tm.bounds[1][1]) / 2)
loc, _, _ = cap_tm.ray.intersects_location(np.array([[POD_X, cy, 60.0]]), np.array([[0, 0, -1.0]]))
top_z = float(loc[:, 2].max())

# ---------------- cap frame <-> car frame ----------------
r = np.radians(RAKE)
pillar_up = np.array([-np.sin(r), 0, np.cos(r)])      # along the leg, upward
inboard = np.array([0.0, -1.0, 0.0])                  # cap face looks into the cabin
cap_x = pillar_up if UP_END == "wide" else -pillar_up  # cap +X runs narrow -> wide
cap_z = unit(inboard - inboard @ pillar_up * pillar_up)
cap_y = np.cross(cap_z, cap_x)
CAR_FROM_CAP = np.column_stack([cap_x, cap_y, cap_z])  # columns: cap axes in car frame

# screen: normal toward the eyes, long side truly vertical
s = unit(EYE)
up = unit(np.array([0, 0, 1.0]) - s[2] * s)
side = np.cross(up, s)
# pod local frame: width X, screen normal -Y, height Z (portrait: long side = local Z)
CAR_FROM_POD = np.column_stack([side, -s, up])
R = CAR_FROM_CAP.T @ CAR_FROM_POD                     # pod local -> cap frame

ow, oh = INNER_H + 2 * WALL, INNER_W + 2 * WALL      # portrait outer size
iw, ih = INNER_H, INNER_W
cw, ch = CUT_H, CUT_W

# anchor: pod rear centre sits LIFT above the cap face, then lift further until the whole rear
# plate clears the cap
rear_c = np.array([0, DEPTH / 2, 0])
corners = np.array([[sx * ow / 2, DEPTH / 2, sz * oh / 2] for sx in (-1, 1) for sz in (-1, 1)])
anchor = np.array([POD_X, cy, top_z])
lift = LIFT - min(((R @ c)[2] - (R @ rear_c)[2]) for c in corners)
T = anchor + np.array([0, 0, lift]) - R @ rear_c


def place(m):
    return m.transform(np.column_stack([R, T]).astype(float).tolist())


# ---------------- sleeve ----------------
outer = mf.Manifold.cube([ow, DEPTH, oh], center=True)
inner = mf.Manifold.cube([iw, DEPTH, ih], center=True).translate([0, FRONT, 0])  # open rear
cut = mf.Manifold.cube([cw, FRONT * 3, ch], center=True).translate([0, -DEPTH / 2, 0])
screw_y = DEPTH / 2 - BOSS / 2
screws = mf.Manifold.cylinder(ow + 4, SCREW_D / 2, SCREW_D / 2, 24).rotate([0, 90, 0]).translate([-(ow + 4) / 2, screw_y, 0])
sleeve = outer - inner - cut - screws.translate([0, 0, oh / 4]) - screws.translate([0, 0, -oh / 4])

# ---------------- base: cap + podium + boss ----------------
plate = mf.Manifold.cube([ow, 2.0, oh], center=True).translate([0, DEPTH / 2 + 1.0, 0])
boss = mf.Manifold.cube([iw - 0.4, BOSS, ih - 0.4], center=True).translate([0, DEPTH / 2 - BOSS / 2, 0])
pilots = mf.Manifold.cylinder(iw + 2, PILOT_D / 2, PILOT_D / 2, 24).rotate([0, 90, 0]).translate([-(iw + 2) / 2, screw_y, 0])
pad = mf.Manifold.cube([34, 24, 1.0], center=True).translate([POD_X, cy, top_z - EMBED - 3])
podium = mf.Manifold.batch_hull([place(plate), pad]) - to_manifold(cap_tm.convex_hull).translate([0, 0, -EMBED])
base = cap + podium + place(boss)
base = base - place(pilots.translate([0, 0, oh / 4])) - place(pilots.translate([0, 0, -oh / 4]))
# cable: from the middle of the boss down into the cap cavity
cable = mf.Manifold.batch_hull([
    place(mf.Manifold.sphere(CABLE_D / 2, 24).translate([0, DEPTH / 2 - BOSS - 1, 0])),
    mf.Manifold.sphere(CABLE_D / 2, 24).translate([POD_X, cy, top_z - 6])])
base = base - cable
sleeve_placed = place(sleeve)

for name, m in (("apillar_base.stl", base), ("apillar_sleeve.stl", sleeve), ("apillar_assembly.stl", base + sleeve_placed)):
    t = to_trimesh(m)
    t = max(t.split(only_watertight=False), key=lambda p: len(p.faces))  # drop hull slivers
    t.export(name)
    print(name, "watertight", t.is_watertight, "size", np.round(t.extents, 1))

# assembly in the car frame, for the preview
asm = to_trimesh(base + sleeve_placed)
asm.vertices = (CAR_FROM_CAP @ (asm.vertices - anchor).T).T
asm.export("apillar_assembly_car.stl")
tilt = np.degrees(np.arccos(abs((CAR_FROM_CAP @ R @ np.array([0, 0, 1.0]))[2])))
print("screen long side from vertical, deg:", round(tilt, 2))
