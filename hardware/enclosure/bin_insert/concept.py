# Concept: printed insert that plugs into the lower centre-stack bin and holds the DinMeter.
# A x B = bin opening, C = bin depth: to be measured in the car. DinMeter numbers are fixed.
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch, Polygon
A,B,C = 80,45,55          # placeholders for the drawing only
F = 3                     # face plate thickness
fig,(a1,a2)=plt.subplots(1,2,figsize=(13,6.2),gridspec_kw={'width_ratios':[1.15,1]})
red='#c0392b'; k='#222'
def dim(ax,x1,y1,x2,y2,t,off=(0,0),rot=0,c=red):
    ax.annotate('',xy=(x1,y1),xytext=(x2,y2),arrowprops=dict(arrowstyle='<->',lw=.9,color=c))
    ax.text((x1+x2)/2+off[0],(y1+y2)/2+off[1],t,color=c,ha='center',va='center',fontsize=10,rotation=rot,bbox=dict(fc='white',ec='none',pad=1))
# ---------- front view ----------
ax=a1
ax.add_patch(FancyBboxPatch((-A/2-3,-B/2-3),A+6,B+6,boxstyle='round,pad=0,rounding_size=3',fc='#3a4250',ec=k,lw=1.5))
ax.add_patch(Rectangle((-26.5,-15),53,30,fc='#e2602a',ec=k,lw=1.2))          # DinMeter front 53x30
ax.add_patch(Rectangle((-24,-10),30,20,fc='#0c0f12',ec='none'))             # screen
ax.text(-9,0,'2.3',color='#b6f34a',fontsize=16,ha='center',va='center',family='monospace',weight='bold')
c=plt.Circle((15,0),6.5,fc='#d8d8d8',ec='#555'); ax.add_patch(c)
ax.add_patch(Rectangle((-22.6,-11.35),45.2,22.7,fill=False,ec='#fff',lw=.9,ls='--'))
ax.text(0,-B/2-1.2,'cut-out 45.2 × 22.7 (1/32 DIN)',color='#ddd',fontsize=8.5,ha='center',va='bottom')
dim(ax,-A/2-3,B/2+9,A/2+3,B/2+9,'A + 6  (bin opening A + 3 mm lip each side)')
dim(ax,-A/2-12,-B/2-3,-A/2-12,B/2+3,'B + 6',rot=90,off=(-3,0))
dim(ax,-26.5,-B/2-9,26.5,-B/2-9,'53 DinMeter front')
ax.set_title('Front view (what the driver sees)',fontsize=12,weight='bold')
ax.set_xlim(-A/2-22,A/2+12); ax.set_ylim(-B/2-16,B/2+16); ax.set_aspect('equal'); ax.axis('off')
# ---------- side section ----------
ax=a2
# car: bin (grey, existing part) with dashboard face
ax.add_patch(Rectangle((0,-B/2-2.5),C,B+5,fc='#d9dde2',ec='#888',lw=1))
ax.add_patch(Rectangle((0,-B/2),C,B,fc='white',ec='#888',lw=.8))
ax.text(C/2+8,-B/2-6,'existing bin (stays in the car)',fontsize=9,color='#666',ha='center')
ax.plot([-2,-2],[-B/2-14,B/2+14],color='#888',lw=2); ax.text(-3,B/2+12,'dash face',fontsize=9,color='#666',ha='right')
# insert: face plate + sleeve
ax.add_patch(Rectangle((-F,-B/2-3),F,B+6,fc='#3a4250',ec=k,lw=1.3))
S=30
ax.add_patch(Rectangle((0,B/2-2),S,2,fc='#3a4250',ec=k,lw=1)); ax.add_patch(Rectangle((0,-B/2),S,2,fc='#3a4250',ec=k,lw=1))
# snap bumps
for y,s in ((B/2-2,1),(-B/2+2,-1)):
    ax.add_patch(Polygon([[S-8,y],[S-4,y+s*1.4],[S-2,y]],fc='#3a4250',ec=k))
ax.text(S-5,B/2+4.5,'snap bumps',fontsize=8.5,ha='center')
# DinMeter in section
ax.add_patch(Rectangle((-F-5,-15),5,30,fc='#e2602a',ec=k,lw=1))
ax.add_patch(Rectangle((0,-11.25),27,22.5,fc='#ffd2bd',ec='#c0602a',ls='--'))
ax.text(13.5,0,'DinMeter\nbody 27',fontsize=9,ha='center',color='#8a3c12')
# cable
ax.plot([27,40,48,48],[0,0,-8,-B/2-1],color='#2a7',lw=2)
ax.text(49,-14,'cable through a Ø10\nhole in the bin floor\n(only cut, if any)',fontsize=8.5,color='#2a7')
dim(ax,0,-B/2-11,C,-B/2-11,'C  bin depth (measure; needs ≥ 35)')
dim(ax,0,B/2+8,S,B/2+8,'sleeve ≈ 30')
dim(ax,-F-5,-B/2-18,0,-B/2-18,'≈8',off=(0,-3))
ax.set_title('Side section',fontsize=12,weight='bold')
ax.set_xlim(-16,C+30); ax.set_ylim(-B/2-24,B/2+16); ax.set_aspect('equal'); ax.axis('off')
fig.suptitle('Bin insert for the DinMeter - concept.  A × B × C = bin opening and depth, to be measured in the car.',fontsize=11,color='#444')
plt.tight_layout(); plt.savefig('bin_insert_concept.png',dpi=150,bbox_inches='tight')
