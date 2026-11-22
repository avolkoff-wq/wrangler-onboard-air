# universal board: holes (col,row); strips: left a-e, right f-j
L='abcde';Rr='fghij'
def strip(c,r): return ('L' if c in L else 'R')+str(r)
items=[ # (name, [holes]) parts; each hole occupied on top side
 ('J3.+5V',[('a',1)]),('J3.SIG',[('a',3)]),('J3.GND',[('a',5)]),
 ('J1.GND',[('a',7)]),('J1.OUT1',[('a',9)]),('J1.AUX1',[('a',11)]),('J1.OUT2',[('a',13)]),
 ('J2.5V',[('j',2)]),('J2.G1',[('j',4)]),('J2.G2',[('j',6)]),('J2.G13',[('j',8)]),
 ('J2b.GND',[('j',10)]),('J2b.+12V',[('j',12)]),('J2b.DC-',[('j',14)]),
 ('K1.4',[('e',9)]),('K1.5',[('e',10)]),('K1.6',[('e',11)]),('K1.3',[('f',9)]),('K1.2',[('f',10)]),('K1.1',[('f',11)]),
 ('K4.4',[('e',13)]),('K4.5',[('e',14)]),('K4.6',[('e',15)]),('K4.3',[('f',13)]),('K4.2',[('f',14)]),('K4.1',[('f',15)]),
]
res=[('R1',('d',3),('g',4)),('R2',('i',4),('i',10)),('R3',('h',6),('h',11)),('R4',('g',8),('g',15))]
top=[('5V',('e',1),('f',2)),('AUX1-K4',('c',11),('c',15)),('AUX1-12a',('d',11),('d',12)),('AUX1-12b',('e',12),('f',12)),('GNDL',('d',5),('d',7))]
bot=[('GNDx',('c',7),('h',10)),('GND10-14',('g',10),('g',14))]
occ={}
def use(h,who,side='top'):
    k=(h,side if side=='bot' else 'top')
    if side=='top':
        assert h not in occ, f'hole {h} used by {occ.get(h)} and {who}'
        occ[h]=who
for n,hs in items:
    for h in hs: use(h,n)
for n,a,b in res: use(a,n); use(b,n)
for n,a,b in top: use(a,n); use(b,n)
# bottom wires may share pads but must not land on a hole whose top item is a resistor body only? they use pads; record
botpads=[]
for n,a,b in bot: botpads+= [a,b]
# resistor bodies cover intermediate holes on top: check top jumpers don't land under bodies
def between(a,b):
    (c1,r1),(c2,r2)=a,b
    if c1==c2: return [(c1,r) for r in range(min(r1,r2)+1,max(r1,r2))]
    return []
covered={h:n for n,a,b in res for h in between(a,b)}
for n,a,b in top:
    for h in (a,b): assert h not in covered, f'top jumper {n} lands under {covered[h]}'
# union find
par={}
def f(x):
    par.setdefault(x,x)
    while par[x]!=x: x=par[x]
    return x
def u(a,b): par[f(a)]=f(b)
for n,a,b in res: pass
for n,a,b in top+bot: u(strip(*a),strip(*b))
net={}
for n,hs in items:
    for h in hs: net.setdefault(f(strip(*h)),[]).append(n)
for n,a,b in res:
    net.setdefault(f(strip(*a)),[]).append(n+'.a'); net.setdefault(f(strip(*b)),[]).append(n+'.b')
for k,v in sorted(net.items(),key=lambda kv:str(kv[0])): print(k, sorted(v))
