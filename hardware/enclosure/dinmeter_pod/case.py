from manifold3d import Manifold, CrossSection, JoinType
import trimesh, numpy as np
def rr(w,h,r):  # rounded rect centered
    return CrossSection.square((w-2*r,h-2*r),center=True).offset(r,JoinType.Round,circular_segments=48)
def box(w,h,d,r): return rr(w,h,r).extrude(d)
def cyl(r,h): return Manifold.cylinder(h,r,r,48)
W,H,D=60,38,45; T=2.5; F=3          # outer, wall, front plate
CW,CH=45.6,23.0                     # DIN 1/32 cutout 45x22.5 + clearance
# BODY: z=0 front face, open back at z=D
outer=box(W,H,D,4)
cav=box(W-2*T,H-2*T,D,2).translate((0,0,F))
cut=CrossSection.square((CW,CH),center=True).extrude(F+1).translate((0,0,-0.5))
body=outer-cav-cut
bx,by=W/2-T-3.0,H/2-T-3.0
for sx in(-1,1):
  for sy in(-1,1):
    boss=cyl(3.4,12).translate((sx*bx,sy*by,D-12))
    pilot=cyl(1.25,11).translate((sx*bx,sy*by,D-11+0.01))
    body=(body+boss)-pilot
# cable hole in bottom wall near the back
hole=cyl(4.2,T+2).rotate((90,0,0)).translate((0,-H/2+T+1,D-9))
body=body-hole
# BACK COVER: plate + locating lip + side ears with mounting holes
P=3
cover=box(W,H,P,4)+box(W+24,H-14,P,3)          # ears left/right
lip=(box(W-2*T-0.6,H-2*T-0.6,2,1.7)-box(W-2*T-4.6,H-2*T-4.6,2,0.5)).translate((0,0,P))
cover=cover+lip
for sx in(-1,1):
  for sy in(-1,1):
    cover=cover-cyl(1.7,P+3).translate((sx*bx,sy*by,-1))-cyl(3.2,1.6).translate((sx*bx,sy*by,-0.01))
    # lip relief around bosses
    cover=cover-cyl(4.0,3).translate((sx*bx,sy*by,P))
  cover=cover-cyl(2.3,P+2).translate((sx*(W/2+6),0,-1))
def save(m,n):
  me=m.to_mesh(); tm=trimesh.Trimesh(me.vert_properties[:,:3],me.tri_verts); tm.export(n); return tm
b=save(body,'dinmeter_body.stl'); c=save(cover,'dinmeter_back.stl')
print('body',b.bounds.tolist(),b.is_watertight,round(b.volume/1000,1),'cm3')
print('cover',c.bounds.tolist(),c.is_watertight,round(c.volume/1000,1),'cm3')
# preview scene
c2=c.copy(); c2.apply_translation((0,0,D+25))
sc=trimesh.Scene([b,c2]); open('scene.glb','wb').write(sc.export(file_type='glb'))
