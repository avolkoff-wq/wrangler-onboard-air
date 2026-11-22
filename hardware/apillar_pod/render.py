"""Preview renders of apillar_pod.stl (matplotlib, orthographic)."""
import sys, trimesh, numpy as np, matplotlib
matplotlib.use('Agg'); import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
m = trimesh.load(sys.argv[1] if len(sys.argv) > 1 else 'apillar_pod.stl')
out = sys.argv[2] if len(sys.argv) > 2 else 'apillar_pod.png'
v, f = m.vertices, m.faces
light = np.array([-0.5, -0.4, 0.8]); light /= np.linalg.norm(light)
sh = np.clip(m.face_normals @ light, 0, 1) * 0.75 + 0.25
fig = plt.figure(figsize=(14, 5.5))
for i, (el, az, t) in enumerate([(20, -150, 'screen side'), (0, -90, 'side, X along the cap'), (25, 40, 'rear, open for the clip')]):
    ax = fig.add_subplot(1, 3, i + 1, projection='3d')
    ax.add_collection3d(Poly3DCollection(v[f], facecolors=plt.cm.copper(sh), edgecolor='none'))
    c = v.mean(0); r = (v.max(0) - v.min(0)).max() / 2
    ax.set_xlim(c[0] - r, c[0] + r); ax.set_ylim(c[1] - r, c[1] + r); ax.set_zlim(c[2] - r, c[2] + r)
    ax.set_proj_type('ortho'); ax.view_init(el, az); ax.set_title(t); ax.set_axis_off()
plt.tight_layout(); plt.savefig(out, dpi=110)
