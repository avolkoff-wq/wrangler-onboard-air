from manifold3d import Manifold, CrossSection, JoinType
import trimesh, json
def rr(w,h,r): return CrossSection.square((w-2*r,h-2*r),center=True).offset(r,JoinType.Round,circular_segments=48)
def box(w,h,d,r): return rr(w,h,r).extrude(d)
def cyl(r,h): return Manifold.cylinder(h,r,r,48)
# coordinates: x = width, y = depth (front = -y), z = up
IX,IY,IH=62,96,46; T=2.5
OX,OY=IX+2*T,IY+2*T
base=box(OX,OY,IH+T,5)-box(IX,IY,IH+1,3).translate((0,0,T))
# corner screw bosses for lid (M3 self-tapping, pilot 2.5)
cb=[(sx*(IX/2-3.5),sy*(IY/2-3.5)) for sx in(-1,1) for sy in(-1,1)]
for x,y in cb:
    base=base+cyl(4,IH).translate((x,y,T))-cyl(1.25,14).translate((x,y,T+IH-13.9))
# board: 55 x 44, landscape, at the back; holes in the centre channel, 35.6 apart along x
BY=IY/2-18-22   # board centre y (back side)
holes=[(-17.8,BY),(17.8,BY)]
for x,y in holes:
    base=base+cyl(3.5,6).translate((x,y,T))-cyl(1.25,6).translate((x,y,T+0.5))
# cable glands M12 on the left and right walls, low, behind the board centre
for sx in(-1,1):
    base=base-cyl(6.15,T+2).rotate((0,90,0)).translate((sx*(OX/2)-(1 if sx>0 else -1)*0 - (T+2)/2*(-1 if sx<0 else 1)*0,BY,T+14)) if False else base
gl=cyl(6.15,T+4).rotate((90,0,0))
for gx in(-15,15): base=base-gl.translate((gx,OY/2+1,T+12))
# lid
LT=3
lid=box(OX,OY,LT,5)
lip=box(IX-0.6,IY-0.6,2,3)-box(IX-4.6,IY-4.6,2,2)
lid=lid+lip.translate((0,0,-2))
for x,y in cb:
    lid=lid-cyl(1.7,LT+4).translate((x,y,-3))-cyl(3.2,1.6).translate((x,y,LT-1.59))-cyl(4.6,3).translate((x,y,-2.01))
DY=-IY/2+3+14   # DinMeter centre y (front strip)
lid=lid-CrossSection.square((45.6,23.0),center=True).extrude(LT+6).translate((0,DY,-4))
def save(m,n):
    me=m.to_mesh(); t=trimesh.Trimesh(me.vert_properties[:,:3],me.tri_verts); t.export(n); return t
b=save(base,'enc_base.stl'); l=save(lid,'enc_lid.stl')
print(b.bounds.tolist(),b.is_watertight,l.bounds.tolist(),l.is_watertight)
json.dump({'IH':IH,'T':T,'BY':BY,'DY':DY,'LT':LT,'OX':OX,'OY':OY},open('dims.json','w'))
