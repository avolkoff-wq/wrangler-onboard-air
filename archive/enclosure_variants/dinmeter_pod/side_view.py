# Right side view of the DinMeter pod with dimensions (mm)
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch, Circle
W,H,D,T,F,P=60,38,45,2.5,3,3
fig,ax=plt.subplots(figsize=(10,7))
k='#222'
def dim(x1,y1,x2,y2,t,off=(0,0),rot=0):
    ax.annotate('',xy=(x1,y1),xytext=(x2,y2),arrowprops=dict(arrowstyle='<->',lw=.9,color='#c0392b'))
    ax.text((x1+x2)/2+off[0],(y1+y2)/2+off[1],t,color='#c0392b',ha='center',va='center',fontsize=10,rotation=rot,
            bbox=dict(fc='white',ec='none',pad=1))
# body (side view: z to the right, y up), section shown dashed inside
ax.add_patch(FancyBboxPatch((0,-H/2),D,H,boxstyle='round,pad=0,rounding_size=1.5',fc='#e9e9e9',ec=k,lw=1.6))
ax.add_patch(Rectangle((F,-H/2+T),D-F,H-2*T,fc='white',ec='#888',lw=.8,ls='--'))
# bosses
for y in (H/2-T-3,-(H/2-T-3)):
    ax.add_patch(Rectangle((D-12,y-3.4),12,6.8,fc='#d5d5d5',ec='#888',lw=.8,ls='--'))
# cable hole in bottom wall
ax.add_patch(Rectangle((D-11-2.7,-H/2-0.4),5.4,T+0.8,fc='white',ec=k,lw=1.2))
ax.text(D-11,-H/2-4,'Grove slot 10.7 × 5.4',ha='center',fontsize=9)
# back cover + lip
ax.add_patch(Rectangle((D+0.5,-H/2),P,H,fc='#cfd8e3',ec=k,lw=1.4))
ax.add_patch(Rectangle((D-1.5,-H/2+T+0.3),2,H-2*T-0.6,fc='#cfd8e3',ec='#555',lw=.8,ls='--'))
# DinMeter: front bezel + body behind the 45x22.5 cutout
ax.add_patch(Rectangle((-5,-15),5,30,fc='#ff7a3d',ec=k,lw=1.2))
ax.add_patch(Rectangle((0,-11.25),27,22.5,fc='#ffd2bd',ec='#c0602a',lw=1,ls='--'))
# back clip from the kit (approximate): frame right behind the front wall, ~8 deep, ears to about +/-9.5 high
ax.add_patch(Rectangle((F,-13.3),8,26.6,fc='#f6b26b',ec='#c46a14',lw=1,ls='--'))
ax.text(F+4,-16.5,'back clip\n(kit, approx.)',ha='center',va='top',fontsize=8,color='#a3540f')
ax.text(-2.5,0,'DinMeter\nfront',rotation=90,ha='center',va='center',fontsize=9)
ax.text(14,0,'DinMeter body\n(behind front plate)',ha='center',va='center',fontsize=9,color='#8a3c12')
ax.text(D/2+5,H/2-6,'pod body',fontsize=10,ha='center')
ax.text(D+2,H/2+3,'back cover: flat, VHB tape\nto the tunnel side wall,\nGrove slot 10.7 × 5.4 in the centre\n(Grove plug passes through)',fontsize=9)
# dimensions
dim(0,-H/2-9,D,-H/2-9,'45')
dim(D+0.5,-H/2-15,D+P+0.5,-H/2-15,'3',off=(4,0))
dim(-5,-H/2-15,D+P+0.5,-H/2-15,'overall 53.5',off=(0,-3))
dim(-12,-H/2,-12,H/2,'38',rot=90,off=(-2.5,0))
dim(-8,-15,-8,15,'30',rot=90,off=(-2,0))
dim(0,H/2+3,F,H/2+3,'3',off=(0,2.5))
dim(-5,H/2+8,0,H/2+8,'≈5',off=(0,2.5))
ax.text(D/2,-H/2-24,'Right side view, mm.  Front = left (towards the driver).  Width (into the page) 60. Back = tunnel side wall.',
        ha='center',fontsize=9,color='#555')
ax.set_xlim(-20,D+28); ax.set_ylim(-H/2-28,H/2+14); ax.set_aspect('equal'); ax.axis('off')
ax.set_title('DinMeter pod - right side',fontsize=13,weight='bold')
plt.savefig('side_view.png',dpi=160,bbox_inches='tight')
