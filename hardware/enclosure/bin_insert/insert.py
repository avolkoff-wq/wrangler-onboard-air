# Printed insert for the lower centre-stack bin. A x B = bin opening, C = depth (measure in the car).
from manifold3d import Manifold, CrossSection, JoinType
import trimesh, sys
A,B,C = [float(x) for x in sys.argv[1:4]] if len(sys.argv)>3 else (80,45,55)   # placeholders
F,S,T,CL = 3.0, 30.0, 2.0, 0.3        # face thickness, sleeve length, sleeve wall, clearance
def rr(w,h,r): return CrossSection.square((w-2*r,h-2*r),center=True).offset(r,JoinType.Round,circular_segments=40)
face = rr(A+6,B+6,3).extrude(F)                                     # z 0..3, front at z=0
w,h = A-2*CL, B-2*CL
sleeve = (rr(w,h,1.5)-rr(w-2*T,h-2*T,0.6)).extrude(S).translate((0,0,F))
part = face + sleeve
# snap bumps on top and bottom, near the end of the sleeve
for s in (1,-1):
    bump = Manifold.cube((20,1.2,4),center=True).translate((0,s*(h/2+0.6),F+S-6))
    part = part + bump
    # slots either side of the bump so the wall can flex
    for x in (-11,11):
        part = part - Manifold.cube((1.2,T+2,12),center=True).translate((x,s*(h/2-T/2),F+S-6))
cut = Manifold.cube((45.2,22.7,F+2),center=True).translate((0,0,F/2))
part = part - cut
me=part.to_mesh(); tm=trimesh.Trimesh(me.vert_properties[:,:3],me.tri_verts)
tm.export('bin_insert.stl'); print(tm.bounds.round(1).tolist(), tm.is_watertight)
