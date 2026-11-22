import re
X={'L-':62,'L+':84,'a':112,'b':132,'c':152,'d':172,'e':192,'f':248,'g':268,'h':288,'i':308,'j':328,'R+':356,'R-':378}
def Y(r): return 70+(r-1)*20
o=[]
A=o.append
A('<svg viewBox="-95 0 630 420" xmlns="http://www.w3.org/2000/svg" aria-label="Perma-Proto раскладка" style="min-width:360px;font-family:var(--cond)">')
A('''<defs>
<linearGradient id="pcb" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#1f6b3a"/><stop offset="1" stop-color="#17532d"/></linearGradient>
<radialGradient id="pad" cx=".4" cy=".35" r=".7"><stop offset="0" stop-color="#ffe9a8"/><stop offset=".55" stop-color="#d9a640"/><stop offset="1" stop-color="#9c6e1c"/></radialGradient>
<linearGradient id="tb" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#1e7a35"/><stop offset=".5" stop-color="#2fa14a"/><stop offset="1" stop-color="#1e7a35"/></linearGradient>
<linearGradient id="ic" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#3a3a3a"/><stop offset="1" stop-color="#151515"/></linearGradient>
<linearGradient id="resv" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#b99a5e"/><stop offset=".45" stop-color="#f0dcae"/><stop offset="1" stop-color="#a8884c"/></linearGradient>
<linearGradient id="resh" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#b99a5e"/><stop offset=".45" stop-color="#f0dcae"/><stop offset="1" stop-color="#a8884c"/></linearGradient>
<filter id="sh" x="-20%" y="-20%" width="140%" height="140%"><feDropShadow dx="1.5" dy="2" stdDeviation="1.6" flood-opacity=".45"/></filter>
</defs>''')
# board
A('<rect x="38" y="40" width="364" height="340" rx="10" fill="url(#pcb)" stroke="#0f3b1f" stroke-width="2"/>')
for cx,cy in((90,52),(350,52)): pass
# copper strips under mask
for r in range(1,16):
    for x0,x1 in((X['a'],X['e']),(X['f'],X['j'])):
        A(f'<rect x="{x0-7}" y="{Y(r)-5.5}" width="{x1-x0+14}" height="11" rx="5.5" fill="#2f8a4c" opacity=".9"/>')
for k in('L-','L+','R+','R-'):
    A(f'<rect x="{X[k]-5.5}" y="{Y(1)-7}" width="11" height="{Y(15)-Y(1)+14}" rx="5.5" fill="#2f8a4c" opacity=".9"/>')
    A(f'<line x1="{X[k]+(-9 if k=="L-" else 9 if k=="R-" else 0)}" y1="{Y(1)-8}" x2="{X[k]+(-9 if k=="L-" else 9 if k=="R-" else 0)}" y2="{Y(15)+8}" stroke="{"#5fa8ff" if "-" in k else "#ff6b4a"}" stroke-width="1.6" opacity="{.9 if "-" in k else .55}"/>' if k in('L-','R-') else '')
# holes
for k in X:
    for r in range(1,16):
        A(f'<circle cx="{X[k]}" cy="{Y(r)}" r="3.6" fill="url(#pad)"/><circle cx="{X[k]}" cy="{Y(r)}" r="1.4" fill="#123"/>')
# silkscreen
for k in 'abcdefghij': A(f'<text x="{X[k]}" y="{Y(1)-13}" text-anchor="middle" style="fill:#e9f3ea;font-size:10px;font-weight:600">{k}</text>')
for r in range(1,16): A(f'<text x="220" y="{Y(r)+3.5}" text-anchor="middle" style="fill:#cfe6d4;font-size:9px">{r}</text>')
for k,l in(('L-','−'),('L+','+'),('R+','+'),('R-','−')): A(f'<text x="{X[k]}" y="{Y(1)-13}" text-anchor="middle" style="fill:#e9f3ea;font-size:12px;font-weight:700">{l}</text>')
A('<text x="220" y="372" text-anchor="middle" style="fill:#cfe6d4;font-size:9px;letter-spacing:.12em">WRANGLER AIR · PERMA-PROTO 1/4</text>')
# helpers
def lead(x1,y1,x2,y2): A(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="#c9ccd1" stroke-width="2.2" stroke-linecap="round"/>'); [A(f'<circle cx="{x}" cy="{y}" r="2.4" fill="#e6e8eb" stroke="#888" stroke-width=".6"/>') for x,y in((x1,y1),(x2,y2))]
BANDS={'10k':['#7a4a1e','#111','#e07a1f','#c9a33a'],'20k':['#c0392b','#111','#e07a1f','#c9a33a'],'330':['#e07a1f','#e07a1f','#7a4a1e','#c9a33a']}
def resistor(a,b,val,name,lx,ly,anchor='start'):
    (c1,r1),(c2,r2)=a,b; x1,y1,x2,y2=X[c1],Y(r1),X[c2],Y(r2)
    lead(x1,y1,x2,y2)
    import math
    mx,my=(x1+x2)/2,(y1+y2)/2; ang=math.degrees(math.atan2(y2-y1,x2-x1))
    g=f'<g transform="translate({mx},{my}) rotate({ang})" filter="url(#sh)"><rect x="-16" y="-6.5" width="32" height="13" rx="6.5" fill="url(#resh)" stroke="#7a5a20" stroke-width=".8"/>'
    for k,c in enumerate(BANDS[val]): g+=f'<rect x="{-10+k*6+(3 if k==3 else 0)}" y="-6.3" width="3" height="12.6" fill="{c}"/>'
    A(g+'</g>')
    A(f'<text x="{lx}" y="{ly}" text-anchor="{anchor}" style="fill:#fff;font-size:10px;font-weight:700;paint-order:stroke;stroke:#0f3b1f;stroke-width:3px">{name} {val}</text>')
def jumper(pts,col):
    d='M'+' L'.join(f'{x} {y}' for x,y in pts)
    A(f'<path d="{d}" fill="none" stroke="#000" stroke-opacity=".35" stroke-width="7" stroke-linejoin="round" stroke-linecap="round" transform="translate(1.2,1.8)"/>')
    A(f'<path d="{d}" fill="none" stroke="{col}" stroke-width="5.2" stroke-linejoin="round" stroke-linecap="round"/>')
    A(f'<path d="{d}" fill="none" stroke="#fff" stroke-opacity=".35" stroke-width="1.4" stroke-linejoin="round" stroke-linecap="round" transform="translate(-.9,-1.2)"/>')
    for x,y in(pts[0],pts[-1]): A(f'<circle cx="{x}" cy="{y}" r="3.4" fill="#e6e8eb" stroke="#777" stroke-width=".8"/>')
def bottom(pts,col):
    d='M'+' L'.join(f'{x} {y}' for x,y in pts)
    A(f'<path d="{d}" fill="none" stroke="{col}" stroke-width="3" stroke-dasharray="2 5" stroke-linecap="round" stroke-linejoin="round" opacity=".95"/>')
    for x,y in(pts[0],pts[-1]): A(f'<circle cx="{x}" cy="{y}" r="5.2" fill="none" stroke="{col}" stroke-width="1.8"/>')
def chip(r0,name):
    x0,x1=X['e']-6,X['f']+6; y0,y1=Y(r0)-9,Y(r0+2)+9
    A(f'<rect x="{x0}" y="{y0}" width="{x1-x0}" height="{y1-y0}" rx="2" fill="#2b2b2b" stroke="#555" stroke-width="1"/>')
    for k in range(3):
        for x in(X['e'],X['f']): A(f'<rect x="{x-3.5}" y="{Y(r0+k)-2.5}" width="7" height="5" fill="#cfd3d8"/>')
    A(f'<rect x="{X["e"]+5}" y="{Y(r0)-7}" width="{X["f"]-X["e"]-10}" height="{Y(r0+2)-Y(r0)+14}" rx="2" fill="url(#ic)" filter="url(#sh)"/>')
    A(f'<circle cx="{X["f"]-12}" cy="{Y(r0+2)+1}" r="2.6" fill="#555"/>')
    A(f'<text x="220" y="{Y(r0+1)-2}" text-anchor="middle" style="fill:#ddd;font-size:9px;font-weight:700">{name}</text><text x="220" y="{Y(r0+1)+8}" text-anchor="middle" style="fill:#999;font-size:6.5px">AQV252G</text>')
    for k,n in enumerate('456'): A(f'<text x="{X["e"]-9}" y="{Y(r0+k)+3}" text-anchor="end" style="fill:#fff;font-size:8px;font-weight:700">{n}</text>')
    for k,n in enumerate('321'): A(f'<text x="{X["f"]+9}" y="{Y(r0+k)+3}" style="fill:#fff;font-size:8px;font-weight:700">{n}</text>')
def term(side,rows,labels,title,colors):
    x=X['a'] if side=='L' else X['j']
    d=-1 if side=='L' else 1
    bx0=x-6 if side=='R' else x-40; bw=46
    y0,y1=Y(rows[0])-19,Y(rows[-1])+19
    A(f'<g filter="url(#sh)"><rect x="{bx0}" y="{y0}" width="{bw}" height="{y1-y0}" rx="3" fill="url(#tb)" stroke="#145a26"/>')
    for k in range(len(rows)-1): A(f'<line x1="{bx0}" y1="{(Y(rows[k])+Y(rows[k+1]))/2}" x2="{bx0+bw}" y2="{(Y(rows[k])+Y(rows[k+1]))/2}" stroke="#145a26" stroke-width="1"/>')
    A('</g>')
    for r,l,c in zip(rows,labels,colors):
        sx=x+d*16
        A(f'<circle cx="{sx}" cy="{Y(r)}" r="6.5" fill="#b8bec4" stroke="#6b7177"/><line x1="{sx-4.5}" y1="{Y(r)}" x2="{sx+4.5}" y2="{Y(r)}" stroke="#4a4f54" stroke-width="1.8"/>')
        we=x+d*36
        A(f'<rect x="{we-4}" y="{Y(r)-4}" width="8" height="8" rx="1.5" fill="#0d3d18"/>')
        A(f'<circle cx="{x}" cy="{Y(r)}" r="4.6" fill="url(#pad)" stroke="#6b4a10" stroke-width=".8"/><circle cx="{x}" cy="{Y(r)}" r="1.6" fill="#5a3a08"/>')
        tx=30 if d<0 else 410
        A(f'<line x1="{tx+d*-2}" y1="{Y(r)}" x2="{we+d*4}" y2="{Y(r)}" stroke="{c}" stroke-width="1" stroke-dasharray="2 2" opacity=".7"/>')
        A(f'<text x="{tx}" y="{Y(r)+4}" text-anchor="{"end" if d<0 else "start"}" style="fill:{c};font-size:12px;font-weight:700">{l}</text>')
    tx=30 if d<0 else 410
    A(f'<text x="{tx}" y="{y0+6}" text-anchor="{"end" if d<0 else "start"}" style="fill:var(--fg);font-size:11px;font-weight:700;letter-spacing:.04em">{title}</text>')
P,G,S,F='#d9412e','#2d6fd6','#c9a227','var(--fg)'
# --- components (same netlist as netcheck.py) ---
chip(9,'K1'); chip(13,'K4')
resistor(('d',3),('g',4),'10k','R1',220,Y(3)-12,'middle')
resistor(('i',4),('i',10),'20k','R2',X['i']-9,Y(5)+4,'end')
resistor(('h',6),('h',11),'330','R3',X['h']-9,Y(7)+4,'end')
resistor(('g',8),('g',15),'330','R4',X['g']-9,Y(12)+16,'end')
# top jumpers, orthogonal
jumper([(X['e'],Y(1)),(220,Y(1)),(220,Y(2)),(X['f'],Y(2))],P)          # +5V
jumper([(X['c'],Y(11)),(X['c']+10,Y(11)),(X['c']+10,Y(15)),(X['c'],Y(15))],P)  # AUX1 -> K4.6
jumper([(X['d'],Y(11)),(X['d'],Y(12))],P)
jumper([(X['e'],Y(12)),(X['f'],Y(12))],P)                              # +12V across
jumper([(X['d'],Y(5)),(X['d'],Y(7))],G)                                # GND sensor<->car
# bottom GND
bottom([(X['c'],Y(7)),(X['c']-10,Y(7)),(X['c']-10,Y(12)+10),(X['h']+10,Y(12)+10),(X['h']+10,Y(10)),(X['h'],Y(10))],G)
bottom([(X['g'],Y(10)),(X['g']+10,Y(10)),(X['g']+10,Y(14)),(X['g'],Y(14))],G)
# terminals on top of everything
term('L',[1,3,5],['+5V','SIG','GND'],'J3 · датчик',[P,S,G])
term('L',[7,9,11,13],['GND','→K2','AUX1','→K4'],'J1 · машина',[G,P,P,P])
term('R',[2,4,6,8],['5V','G1','G2','G13'],'J2 · DinMeter',[P,S,S,S])
term('R',[10,12,14],['GND','+12V','DC−'],'J2b · питание',[G,P,G])
# legend
ly=400
A(f'<g style="font-size:10px;fill:var(--fg)"><line x1="-50" y1="{ly}" x2="-30" y2="{ly}" stroke="{P}" stroke-width="5" stroke-linecap="round"/><text x="-25" y="{ly+3.5}">перемычка сверху</text>'
  f'<line x1="80" y1="{ly}" x2="100" y2="{ly}" stroke="{G}" stroke-width="3" stroke-dasharray="2 5" stroke-linecap="round"/><text x="105" y="{ly+3.5}">масса проводом снизу платы</text>'
  f'<circle cx="262" cy="{ly}" r="4.6" fill="url(#pad)"/><text x="271" y="{ly+3.5}">ножка клеммника (паять)</text>'
  f'<circle cx="400" cy="{ly}" r="6" fill="#b8bec4" stroke="#6b7177"/><text x="410" y="{ly+3.5}">винт</text></g>')
A('</svg>')
svg='\n'.join(o)
p='compressor-sim.html'; s=open(p).read()
s2,n=re.subn(r'<svg viewBox="[^"]*" xmlns="http://www.w3.org/2000/svg" aria-label="Perma-Proto раскладка".*?</svg>',lambda m:svg,s,count=1,flags=re.S)
print('replaced',n); open(p,'w').write(s2)
