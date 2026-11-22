"""One printed part: the JL upper grab-handle cap with a wedge on it and a 1/32 DIN cutout in
the wedge face. The wedge cancels the pillar lean, so the DinMeter stands upright, landscape
(long side horizontal), screen turned toward the driver.

Base: "Jeep JL A pillar cap (with ball mount)" by livesdE, Thingiverse thing:4238093, CC BY-NC-SA.
This remix keeps the license (non-commercial, attribution, share alike).

Fitting: DinMeter in from the front, stock back clip from inside (the cap is off the car at that
point), cable through the cap into the pillar, then snap the cap back on.
Frames. Cap: X along the cap (narrow end x~0, wide end x~82), Y across, Z out of the face.
Car: X forward, Y left, Z up. Driver-side (left) A-pillar.
Run: python3 make_wedge.py -> apillar_wedge.stl, apillar_wedge_car.stl (preview)
"""
import numpy as np
import manifold3d as mf
import trimesh

RAKE = 33.0           # handle leg lean back from vertical, deg
UP_END = "wide"       # the wide end of the cap points up (photo of the upper red cap)
EYE = (-300.0, -500.0, 0.0)  # driver eyes from the pod, car mm (back, inboard, down); z=0 = upright screen
POD_X = 52.0          # where on the cap the wedge sits
LIFT = 2.0            # minimum gap between the cap face and the back of the front plate
EMBED = 0.6           # wedge sinks into the cap shell so it prints as one part

PLATE_W, PLATE_H = 56.0, 33.0   # front face, bezel 53 x 30 sits on it
CUT_W, CUT_H = 45.6, 23.1       # 1/32 DIN 45 x 22.5 + print clearance, landscape
FRONT = 2.0                     # front wall = panel thickness for the back clip
BODY_W, BODY_H, BODY_D = 49.0, 27.0, 40.0  # space behind the panel: body ~26 + plugs, clip arms

CAP = "A_pillar_cap-without_ball.stl"


def to_mf(tm):
    return mf.Manifold(mf.Mesh(vert_properties=np.asarray(tm.vertices, np.float32),
                               tri_verts=np.asarray(tm.faces, np.uint32)))


def to_tm(m):
    g = m.to_mesh()
    t = trimesh.Trimesh(g.vert_properties[:, :3], g.tri_verts, process=True)
    t.merge_vertices()
    return max(t.split(only_watertight=False), key=lambda p: len(p.faces))


def unit(v):
    v = np.asarray(v, float)
    return v / np.linalg.norm(v)


cap_tm = trimesh.load(CAP)
cap = to_mf(cap_tm)
cy = float(cap_tm.bounds[:, 1].mean())
loc, _, _ = cap_tm.ray.intersects_location(np.array([[POD_X, cy, 60.0]]), np.array([[0, 0, -1.0]]))
top_z = float(loc[:, 2].max())

# cap frame in the car
r = np.radians(RAKE)
pillar_up = np.array([-np.sin(r), 0, np.cos(r)])
cap_x = pillar_up if UP_END == "wide" else -pillar_up
cap_z = np.array([0.0, -1.0, 0.0])            # face looks into the cabin
cap_y = np.cross(cap_z, cap_x)
CAR_FROM_CAP = np.column_stack([cap_x, cap_y, cap_z])

# screen frame: normal to the eyes, "up" truly vertical; local X width, -Y screen normal, Z up
s = unit(EYE)
up = unit(np.array([0, 0, 1.0]) - s[2] * s)
CAR_FROM_POD = np.column_stack([np.cross(up, s), -s, up])
R = CAR_FROM_CAP.T @ CAR_FROM_POD             # pod local -> cap frame

WALL = 2.0
# the whole DinMeter (front wall + body room + wall) must sit outside the cap face:
# place it so its lowest corner clears the cap by LIFT
box_l = mf.Manifold.cube([BODY_W + 2 * WALL, FRONT + BODY_D + WALL, BODY_H + 2 * WALL], center=True).translate(
    [0, (FRONT + BODY_D + WALL) / 2, 0])
shell_pts = [np.array([sx * (BODY_W / 2 + WALL), y, sz * (BODY_H / 2 + WALL)])
             for sx in (-1, 1) for sz in (-1, 1) for y in (0.0, FRONT + BODY_D + WALL)]
T = np.array([POD_X, cy, top_z + LIFT - min((R @ p)[2] for p in shell_pts)])
T[:2] -= (R @ np.array([0, (FRONT + BODY_D) / 2, 0]))[:2]


def place(m):
    return m.transform(np.column_stack([R, T]).astype(float).tolist())


plate = mf.Manifold.cube([PLATE_W, FRONT, PLATE_H], center=True).translate([0, FRONT / 2, 0])
pad = mf.Manifold.cube([40, 26, 1.0], center=True).translate([POD_X, cy, top_z - EMBED - 2])
wedge = mf.Manifold.batch_hull([place(plate), place(box_l), pad]) - to_mf(cap_tm.convex_hull).translate([0, 0, -EMBED])

# body room behind the front wall, opened down into the cap cavity for the clip and the cable
body_room = place(mf.Manifold.cube([BODY_W, BODY_D, BODY_H], center=True).translate([0, FRONT + BODY_D / 2, 0]))
inner_pad = mf.Manifold.cube([30, 18, 1.0], center=True).translate([POD_X, cy, top_z - 6])
hollow = mf.Manifold.batch_hull([body_room, inner_pad])
cut = place(mf.Manifold.cube([CUT_W, FRONT * 3, CUT_H], center=True).translate([0, FRONT / 2, 0]))

part = cap + wedge - hollow - cut
t = to_tm(part)
t.export("apillar_wedge.stl")
car = t.copy()
car.vertices = (CAR_FROM_CAP @ (car.vertices - [POD_X, cy, top_z]).T).T
car.export("apillar_wedge_car.stl")
print("watertight", t.is_watertight, "size", np.round(t.extents, 1),
      "wedge height over cap face", round(t.bounds[1][2] - top_z, 1))
