"""Simplest option: a 1/32 DIN cutout straight in the JL grab-handle cap.

Base: "Jeep JL A pillar cap (with ball mount)" by livesdE, Thingiverse thing:4238093, CC BY-NC-SA.
The DinMeter goes in from the outside, the stock back clip holds it from inside (the cap shell is
~2 mm, the panel thickness the clip expects). The cap top is planar along its length, so the bezel
sits flat. Long side of the screen runs along the cap (along the pillar).
Run: python3 make_cutout.py -> apillar_cutout.stl
"""
import numpy as np, manifold3d as mf, trimesh

CUT_X = 55.0               # cutout centre along the cap (cap is 82 long, wide end at x~82)
CUT_L, CUT_W = 45.6, 23.1  # 45 x 22.5 + print clearance, long side along the cap
POCKET_L, POCKET_W = 49.0, 25.0  # inner ribs cleared around the body for the clip
SHELL = 2.0

cap_tm = trimesh.load("A_pillar_cap-without_ball.stl")
cap = mf.Manifold(mf.Mesh(vert_properties=np.asarray(cap_tm.vertices, np.float32),
                          tri_verts=np.asarray(cap_tm.faces, np.uint32)))
cy = float(cap_tm.bounds[:, 1].mean())
loc, _, _ = cap_tm.ray.intersects_location(np.array([[CUT_X, cy, 60.0]]), np.array([[0, 0, -1.0]]))
top = float(loc[:, 2].max())
hole = mf.Manifold.cube([CUT_L, CUT_W, 60], center=True).translate([CUT_X, cy, top])
pocket = mf.Manifold.cube([POCKET_L, POCKET_W, top - SHELL + 6], center=False).translate(
    [CUT_X - POCKET_L / 2, cy - POCKET_W / 2, -6])
out = cap - hole - pocket
g = out.to_mesh()
t = trimesh.Trimesh(g.vert_properties[:, :3], g.tri_verts, process=True)
t = max(t.split(only_watertight=False), key=lambda p: len(p.faces))
t.export("apillar_cutout.stl")
print("watertight", t.is_watertight, "cut x", CUT_X - CUT_L / 2, "-", CUT_X + CUT_L / 2, "top z", round(top, 1))
