"""One printed part: the JL upper grab-handle cap with a wedge and a 1/32 DIN cutout.

DinMeter lies with its long side along the cap, no turn. The wedge lifts the upper end (wide end
of the cap) away from the pillar by TILT, so the screen looks down toward the driver.

Base: "Jeep JL A pillar cap (with ball mount)" by livesdE, Thingiverse thing:4238093, CC BY-NC-SA.
This remix keeps the license (non-commercial, attribution, share alike).

Fitting: DinMeter in from the front, stock back clip from inside (cap off the car), cable through
the cap into the pillar, cap back on. The lower end of the body goes below the cap face: see the
printed "depth needed" and measure the room behind the cap before printing.
Cap frame: X along the cap (narrow end x~0, wide end x~82 = up in the car), Y across, Z out of the face.
Run: python3 make_wedge.py -> apillar_wedge.stl
"""
import numpy as np
import manifold3d as mf
import trimesh

TILT = 20.0          # deg, upper end lifted off the cap face
POD_X = 50.0         # centre of the DinMeter along the cap
BASE_GAP = 1.0       # front plate low edge above the cap face
EMBED = 0.6          # wedge sinks into the cap shell so it prints as one part

PLATE_L, PLATE_W = 56.0, 33.0   # front face, bezel 53 x 30 sits on it; long side along X
CUT_L, CUT_W = 45.6, 23.1       # 1/32 DIN 45 x 22.5 + print clearance
FRONT = 2.0                     # front wall = panel thickness for the back clip
BODY_L, BODY_W, BODY_D = 49.0, 27.0, 36.0  # room behind the panel: body ~26 + plugs, clip arms

CAP = "A_pillar_cap-without_ball.stl"


def to_mf(tm):
    return mf.Manifold(mf.Mesh(vert_properties=np.asarray(tm.vertices, np.float32),
                               tri_verts=np.asarray(tm.faces, np.uint32)))


cap_tm = trimesh.load(CAP)
cap = to_mf(cap_tm)
cy = float(cap_tm.bounds[:, 1].mean())


def top_at(x):
    loc, _, _ = cap_tm.ray.intersects_location(np.array([[x, cy, 60.0]]), np.array([[0, 0, -1.0]]))
    return float(loc[:, 2].max())


x0, x1 = POD_X - PLATE_L / 2, POD_X + PLATE_L / 2
z0, z1 = top_at(x0 + 2), top_at(x1 - 2)
face_slope = np.degrees(np.arctan2(z1 - z0, x1 - x0))  # the cap face itself rises toward the wide end
ang = face_slope + TILT                                 # plate angle in the cap frame

# plate local: X along, Y across, Z normal (outward); pivot on its low edge
def place(m):
    return m.rotate([0, -ang, 0]).translate([x0, cy, z0 + BASE_GAP])


plate = mf.Manifold.cube([PLATE_L, PLATE_W, FRONT]).translate([0, -PLATE_W / 2, -FRONT])
pad = mf.Manifold.cube([PLATE_L, 26, 1.0]).translate([x0, cy - 13, min(z0, z1) - EMBED - 2])
wedge = mf.Manifold.batch_hull([place(plate), pad]) - to_mf(cap_tm.convex_hull).translate([0, 0, -EMBED])

cut = place(mf.Manifold.cube([CUT_L, CUT_W, FRONT * 3], center=True).translate([PLATE_L / 2, 0, -FRONT]))
body = place(mf.Manifold.cube([BODY_L, BODY_W, BODY_D]).translate([(PLATE_L - BODY_L) / 2, -BODY_W / 2, -FRONT - BODY_D]))
inner = mf.Manifold.batch_hull([place(mf.Manifold.cube([PLATE_L - 4, PLATE_W - 4, 0.5], center=True)
                                      .translate([PLATE_L / 2, 0, -FRONT - 0.3])),
                                mf.Manifold.cube([PLATE_L - 6, 20, 1.0]).translate([x0 + 3, cy - 10, min(z0, z1) - 4])])

part = cap + wedge - inner - body - cut
g = part.to_mesh()
t = trimesh.Trimesh(g.vert_properties[:, :3], g.tri_verts, process=True)
t = max(t.split(only_watertight=False), key=lambda p: len(p.faces))
t.export("apillar_wedge.stl")

# how far the body reaches below the cap face (needed free depth behind the cap)
bv = body.to_mesh().vert_properties[:, :3]
below = max(top_at(np.clip(v[0], 3, 79)) - v[2] for v in bv)
print("watertight", t.is_watertight, "| face slope", round(face_slope, 1), "deg, plate", round(ang, 1),
      "deg | wedge top above cap", round(t.bounds[1][2] - z1, 1), "mm | depth needed below cap face", round(below, 1), "mm")
