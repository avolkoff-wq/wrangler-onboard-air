# Schematic of the JL dash (driver's view) showing where the DinMeter goes. Not to scale.
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Circle, Rectangle, Wedge, Polygon
fig,ax=plt.subplots(figsize=(13,8.2)); ax.set_xlim(0,130); ax.set_ylim(0,82); ax.set_aspect('equal'); ax.axis('off')
dash='#2b2f35'; trim='#3c424a'; k='#15181c'; lab='#c0392b'
ax.add_patch(Rectangle((0,0),130,82,fc='#e9edf1'))
# dash body
ax.add_patch(Polygon([[0,40],[130,40],[130,74],[0,74]],fc=dash))
ax.add_patch(Rectangle((0,72),130,10,fc='#9fb3c4'))          # windscreen
# centre stack (lower, goes down to tunnel)
ax.add_patch(Polygon([[50,40],[88,40],[84,12],[54,12]],fc=dash))
# tunnel / console
ax.add_patch(Polygon([[54,12],[84,12],[88,0],[50,0]],fc='#24282d'))
# steering wheel + cluster (left)
ax.add_patch(FancyBboxPatch((14,56),26,12,boxstyle='round,pad=0,rounding_size=4',fc=trim,ec=k))
for x in (20,34): ax.add_patch(Circle((x,62),4.5,fc='#111',ec='#666'))
ax.add_patch(Circle((27,52),15,fc='none',ec='#111',lw=9))
ax.add_patch(Circle((27,52),5,fc='#111'))
ax.plot([12,22],[52,52],color='#111',lw=6); ax.plot([32,42],[52,52],color='#111',lw=6); ax.plot([27,27],[37,47],color='#111',lw=6)
ax.text(27,30,'steering wheel',ha='center',fontsize=9,color='#555')
# vents + screen
for x in (55,83): ax.add_patch(Circle((x,64),4,fc='#111',ec='#777',lw=1.5))
ax.add_patch(FancyBboxPatch((60,58),18,12,boxstyle='round,pad=0,rounding_size=1',fc='#0d1a26',ec='#777'))
ax.text(69,64,'8.4" screen',color='#9ab',ha='center',va='center',fontsize=8)
# climate controls
ax.add_patch(FancyBboxPatch((56,50),26,6,boxstyle='round,pad=0,rounding_size=1',fc=trim,ec=k))
for x in (60,69,78): ax.add_patch(Circle((x,53),2.2,fc='#222',ec='#888'))
ax.text(69,47.6,'climate controls',ha='center',fontsize=7.5,color='#bbb')
# switch row (windows etc.)
ax.add_patch(FancyBboxPatch((56,41),26,5,boxstyle='round,pad=0,rounding_size=1',fc=trim,ec=k))
for i in range(8): ax.add_patch(Rectangle((57.5+i*3.1,42.2),2.2,2.6,fc='#1a1d21',ec='#666',lw=.6))
ax.text(69,39.2,'window / drive switches',ha='center',fontsize=7.5,color='#bbb')
# lower stack: left bin with DinMeter, centre USB/12V, right AUX bank
ax.add_patch(FancyBboxPatch((55,26),12,9,boxstyle='round,pad=0,rounding_size=1',fc='#14171a',ec='#777'))   # bin opening
ax.add_patch(Rectangle((55.6,26.6),10.8,7.8,fc='#3a4250',ec='#111'))                                         # printed insert
ax.add_patch(Rectangle((56.6,27.8),8.8,5.2,fc='#e2602a',ec='#111'))                                           # DinMeter front
ax.add_patch(Rectangle((57.2,28.5),4.6,3.8,fc='#0c0f12'))
ax.text(59.5,30.4,'2.3',color='#b6f34a',fontsize=7,ha='center',va='center',family='monospace',weight='bold')
ax.add_patch(Circle((63.6,30.4),1.25,fc='#ddd',ec='#555'))
ax.add_patch(FancyBboxPatch((68.5,26),8,9,boxstyle='round,pad=0,rounding_size=1',fc='#14171a',ec='#777'))
for y in (32,29): ax.add_patch(Rectangle((70,y),2,1.3,fc='#555')); ax.add_patch(Circle((74.5,y+0.6),1,fc='#333',ec='#888'))
ax.text(72.5,24.5,'USB / 12V',ha='center',fontsize=7,color='#bbb')
ax.add_patch(FancyBboxPatch((78,26),9,9,boxstyle='round,pad=0,rounding_size=1',fc=trim,ec='#777'))
for i in range(4): ax.add_patch(Rectangle((79,26.8+i*2.0),7,1.6,fc='#1a1d21',ec='#888',lw=.6))
ax.text(82.5,24.5,'AUX 1-4',ha='center',fontsize=7,color='#bbb')
# shifter
ax.add_patch(Rectangle((67.5,6),3,12,fc='#111')); ax.add_patch(Circle((69,18),3,fc='#111',ec='#555'))
ax.add_patch(Rectangle((74,8),2,8,fc='#111')); ax.add_patch(Circle((75,16),2,fc='#111'))
ax.text(69,2.2,'shifter + transfer case lever',ha='center',fontsize=7.5,color='#999')
# labels
ax.annotate('DinMeter in the printed insert,\nin the driver-side lower bin\n(right knee, under the switch row)',xy=(61,28),xytext=(18,14),
            fontsize=11,color=lab,weight='bold',arrowprops=dict(arrowstyle='->',color=lab,lw=1.8))
ax.annotate('cable goes back behind the stack\nto the AUX wiring and relay box',xy=(66,26),xytext=(92,10),fontsize=9,color='#2a7',
            arrowprops=dict(arrowstyle='->',color='#2a7',lw=1.4))
ax.text(65,79,'Jeep Wrangler JL, driver\'s view (LHD) - schematic, not to scale',ha='center',fontsize=12,weight='bold',color='#222')
ax.text(65,75.3,'Bin positions taken from the Mopar diagram of the 2023 Rubicon lower centre stack; check against your car',ha='center',fontsize=9,color='#444')
plt.savefig('interior_location.png',dpi=150,bbox_inches='tight')
