"""DinMeter pod on the Jeep JL A-pillar grab-handle bolt cap.

Base: "Jeep JL A pillar cap (with ball mount)" by livesdE, Thingiverse thing:4238093,
CC BY-NC-SA. This remix (ball removed, podium + DinMeter pod added) is under the same
license: non-commercial, attribution, share alike.

Run:  python3 make_pod.py            -> apillar_pod.stl
All dimensions in mm. Cap frame: X along the cap (narrow end x~0, wide end x~82),
Y across, Z up out of the visible face.
"""
import numpy as np
import manifold3d as mf
import trimesh

# ---------------- parameters ----------------
PORTRAIT = True      # DinMeter vertical (30 wide) fits the 37 mm cap; False = landscape
FACE_YAW = 180.0     # screen faces -X (toward the narrow end of the cap). 0 = toward the wide end
FACE_PITCH = 20.0    # screen tilted up from horizontal-looking, degrees
POD_X = 58.0         # pod centre along the cap (bolt boss sits ~x 40-75)
LIFT = 8.0           # podium height under the pod centre, above the cap top
EMBED = 0.6          # podium sinks this far into the cap shell (shell is ~2 mm) to fuse

CUT_W, CUT_H = 45.6, 23.1   # 1/32 DIN cutout 45 x 22.5 + print clearance
FRONT = 2.0                 # front wall = panel thickness for the DinMeter back clip
WALL = 2.4
DEPTH = 40.0                # body ~26 behind the panel + Grove plugs
INNER_W, INNER_H = 49.0, 27.0  # room for the clip arms around the body
CABLE_D = 9.0               # cable hole down into the cap cavity

CAP = "A_pillar_cap-without_ball.stl"
OUT = "apillar_pod.stl"


def to_manifold(tm):
    return mf.Manifold(mf.Mesh(vert_properties=np.asarray(tm.vertices, np.float32),
                               tri_verts=np.asarray(tm.faces, np.uint32)))


def to_trimesh(m):
    g = m.to_mesh()
    t = trimesh.Trimesh(g.vert_properties[:, :3], g.tri_verts, process=True)
    t.merge_vertices()
    return t


cap_tm = trimesh.load(CAP)
cap = to_manifold(cap_tm)

# top surface height at the pod centre (shell top, measured by a ray)
loc, _, _ = cap_tm.ray.intersects_location(np.array([[POD_X, 142.0, 60.0]]), np.array([[0, 0, -1.0]]))
top_z = float(loc[:, 2].max())
cy = float((cap_tm.bounds[0][1] + cap_tm.bounds[1][1]) / 2)

# ---------------- pod (local frame: screen normal -Y, width X, height Z) ----------------
w_out, h_out = INNER_W + 2 * WALL, INNER_H + 2 * WALL
if PORTRAIT:  # swap so the long side of the DinMeter is vertical
    cw, ch, iw, ih, ow, oh = CUT_H, CUT_W, INNER_H, INNER_W, h_out, w_out
else:
    cw, ch, iw, ih, ow, oh = CUT_W, CUT_H, INNER_W, INNER_H, w_out, h_out
outer = mf.Manifold.cube([ow, DEPTH, oh], center=True)
inner = mf.Manifold.cube([iw, DEPTH, ih], center=True).translate([0, FRONT, 0])  # open at the rear
cut = mf.Manifold.cube([cw, FRONT * 3, ch], center=True).translate([0, -DEPTH / 2, 0])
pod = outer - inner - cut


def orient(m):
    # tilt the screen up (top edge leans back), then turn it to face along the cap
    m = m.rotate([-FACE_PITCH, 0, 0])  # screen normal tips up
    return m.rotate([0, 0, FACE_YAW + 90])  # local -Y (screen) -> -X at yaw 180


_b = orient(outer).bounding_box()  # (xmin, ymin, zmin, xmax, ymax, zmax)
_shift = [POD_X - (_b[0] + _b[3]) / 2, cy - (_b[1] + _b[4]) / 2, top_z + LIFT - _b[2]]


def place(m):
    return orient(m).translate(_shift)


pod = place(pod)
# ---------------- podium: hull of the pod underside and a pad on the cap ----------------
pod_bottom = place(mf.Manifold.cube([ow, DEPTH, 1.0], center=True).translate([0, 0, -oh / 2 + 0.5]))
pad = mf.Manifold.cube([30, 22, 1.0], center=True).translate([POD_X, cy, top_z - EMBED - 3])
podium = mf.Manifold.batch_hull([pod_bottom, pad])
# keep only what lies above the cap outer surface (sunk by EMBED so it fuses with the shell)
cap_hull = to_manifold(cap_tm.convex_hull).translate([0, 0, -EMBED])
podium = podium - cap_hull

body = cap + podium + pod
# clear the pod cavity again (the podium hull must not fill it) and drill the cable path
body = body - place(inner)
cable = mf.Manifold.cylinder(LIFT + 40, CABLE_D / 2, CABLE_D / 2, 32).translate([POD_X, cy, top_z - 6])
body = body - cable

out = to_trimesh(body)
out.export(OUT)
print("watertight", out.is_watertight, "bounds", np.round(out.bounds, 1), "top_z", round(top_z, 1))
