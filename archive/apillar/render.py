"""Preview of the pod in the car frame (X forward, Y left, Z up), orthographic."""
import trimesh, numpy as np, matplotlib
matplotlib.use('Agg'); import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
m = trimesh.load('apillar_assembly_car.stl')
v, f = m.vertices, m.faces
light = np.array([-0.6, -0.5, 0.6]); light /= np.linalg.norm(light)
sh = np.clip(m.face_normals @ light, 0, 1) * 0.75 + 0.25
r = np.radians(33); up = np.array([-np.sin(r), 0, np.cos(r)])
fig = plt.figure(figsize=(15, 5.5))
for i, (el, az, t) in enumerate([(5, -145, 'from the driver seat'), (0, -90, 'from the side (car X left-right)'), (0, 180, 'from the front, screen vertical')]):
    ax = fig.add_subplot(1, 3, i + 1, projection='3d')
    ax.add_collection3d(Poly3DCollection(v[f], facecolors=plt.cm.copper(sh), edgecolor='none'))
    p = np.array([-up * 90, up * 90]) + [0, 20, 0]
    ax.plot(p[:, 0], p[:, 1], p[:, 2], color='gray', lw=6, alpha=0.4)  # A-pillar line
    c = v.mean(0); rr = 60
    ax.set_xlim(c[0] - rr, c[0] + rr); ax.set_ylim(c[1] - rr, c[1] + rr); ax.set_zlim(c[2] - rr, c[2] + rr)
    ax.set_proj_type('ortho'); ax.view_init(el, az); ax.set_title(t); ax.set_axis_off()
plt.tight_layout(); plt.savefig('apillar_pod.png', dpi=110)
