"""Wrangler Air in the AliExpress F-100x68x50 box, schematic A (power_schematic.png): every cable, real lengths.

  W1  Grove 5 cm   PORT.B -> Mini Relay (free ribbon 50 - 2 x 7 = 36 mm, folded under the DinMeter plugs)
  W2  Grove 2 m    PORT.A -> out through the right M12 gland -> sensor (yellow G13 = signal, white unused)
  left gland:  BK GND and RD +12 V to the DinMeter terminal, RD +12 V to relay COM (the Y splice -X2 is outside)
  right gland: W2 and VT from relay NO to the diode node -X3 outside
Wire 20 AWG silicone (about 1.8 mm over the insulation).

Run:  WA_BOX=f100 python3 cables_f100.py   ->  wrangler_air_f100.step / .glb, cables_f100.json
"""
import json
import math
import os
os.environ.setdefault("WA_BOX", "f100")
import cadquery as cq
import layout_box as L

PLUG = 7.0
WIRE_D = 1.3
WD = 1.8
GROVE_COLORS = ("black", "red", "yellow", "white")
RGB = {"black": (0.08, 0.08, 0.08), "red": (0.85, 0.12, 0.12), "yellow": (0.95, 0.80, 0.10), "white": (0.95, 0.95, 0.95),
       "violet": (0.48, 0.17, 0.75)}


def length(pts):
    return sum(math.dist(a, b) for a, b in zip(pts, pts[1:]))


def norm(a):
    n = math.sqrt(sum(x * x for x in a)) or 1.0
    return tuple(x / n for x in a)


def tube(pts, r):
    solids = []
    for a, b in zip(pts, pts[1:]):
        va, vb = cq.Vector(*a), cq.Vector(*b)
        d = vb - va
        if d.Length > 1e-6:
            solids.append(cq.Solid.makeCylinder(r, d.Length, va, d.normalized()))
    for p in pts[1:-1]:
        solids.append(cq.Solid.makeSphere(r, cq.Vector(*p), angleDegrees1=-90, angleDegrees2=90))
    return cq.Workplane("XY").add(cq.Compound.makeCompound(solids))


def ribbon(pts, flat):
    return [[tuple(p[j] + (k - 1.5) * WIRE_D * f[j] for j in range(3)) for p, f in zip(pts, flat)] for k in range(4)]


def fit_loop(a, a_dir, b, b_dir, via, target):
    lo, hi = 0.0, 1.0
    def route(h):
        return [a, tuple(a[i] + 4 * a_dir[i] for i in range(3))] + via(h) + [tuple(b[i] - 4 * b_dir[i] for i in range(3)), b]
    while length(route(hi)) < target:
        hi *= 2
    for _ in range(60):
        m = (lo + hi) / 2
        lo, hi = (m, hi) if length(route(m)) < target else (lo, m)
    return route(hi)


cables, wires = [], []
down = (0, -1, 0)
gP, gS = L.GLANDS                      # (x, z) of the power and the sensor/ARB gland
YIN, YOUT = -L.Y_IN + 6, -L.TOP_Y - 30  # just inside the lock nut, 30 mm outside the box

# W1: PORT.B -> relay. The relay plug sits right under the DinMeter plugs and faces them, so the ribbon
# drops from PORT.B over the relay plug, folds back under the DinMeter plugs and comes into the relay plug from their side.
B, Bb = L.DIN_B, L.REL_GROVE
w1 = fit_loop(B, down, Bb, (0, -1, 0),
              lambda h: [(B[0] + 1, 16.0, 12.5), (Bb[0], 16.5, 10.3), (Bb[0], 25.0 + h, 9.9), (Bb[0], 27.0 + h, 7.6)], 50 - 2 * PLUG)
# W2: PORT.A -> along the free right half of the floor -> right gland -> outside (2 m, only the first 30 mm shown)
A = L.DIN_A
sx = gS[0] - 1.6
w2 = [A, (A[0], A[1] - 4, A[2]), (A[0] + 6, A[1] - 12, 21.0), (sx, 0.0, 21.0), (sx, -30.0, gS[1]), (sx, YIN, gS[1]), (sx, YOUT, gS[1])]
for name, pts, frm, to, stock, fl in (("W1_grove_PORT_B_to_relay", w1, "DinMeter PORT.B", "Mini 3A Relay", 50, (1, 0, 0)),
                                     ("W2_grove_PORT_A_to_sensor", w2, "DinMeter PORT.A", "sensor (via gland, 2 m)", 2000, (1, 0, 0))):
    flat = [fl or norm((i / (len(pts) - 1), 0, 1 - i / (len(pts) - 1))) for i in range(len(pts))]
    cables.append({"name": name, "from": frm, "to": to, "pts": pts, "route": round(length(pts), 1), "stock": stock,
                   "wires": ribbon(pts, flat), "colors": GROVE_COLORS})


def w(name, pts, col, frm, to):
    wires.append({"name": name, "pts": pts, "color": col, "d": WD, "from": frm, "to": to,
                  "route": round(length(pts), 1), "cut": int(math.ceil((length(pts) + 20) / 5) * 5)})


px, pz = gP
w("GND_to_DinMeter_minus", [(px - 2, YOUT, pz - 1.5), (px - 2, YIN, pz - 1.5), (-2.0, -30.0, 24.0), (-2.0, 12.0, 26.0),
                            (L.DIN_MINUS[0], L.DIN_MINUS[1] - 5, L.DIN_MINUS[2]), L.DIN_MINUS], "black", "left gland", "DinMeter terminal -")
w("12V_to_DinMeter_plus", [(px, YOUT, pz + 1.5), (px, YIN, pz + 1.5), (0.0, -30.0, 25.5), (0.0, 12.0, 27.5),
                           (L.DIN_PLUS[0], L.DIN_PLUS[1] - 5, L.DIN_PLUS[2]), L.DIN_PLUS], "red", "left gland", "DinMeter terminal +")
COM, NO = L.REL_PIN["COM"], L.REL_PIN["NO"]
w("12V_to_relay_COM", [(px + 2, YOUT, pz - 1.5), (px + 2, YIN, pz - 1.5), (COM[0], COM[1] - 5, COM[2]), COM], "red", "left gland", "relay COM")
w("relay_NO_to_ARB", [NO, (NO[0], NO[1] - 4, NO[2] + 1), (4.0, -40.0, 19.0), (gS[0] + 3.4, YIN, gS[1]), (gS[0] + 3.4, YOUT, gS[1])],
  "violet", "relay NO", "right gland, on to V1 and the ARB violet wire (-X3)")

asm = L.asm
for c in cables:
    for k, pts in enumerate(c["wires"]):
        asm.add(tube(pts, WIRE_D / 2), name=f"{c['name']}_{c['colors'][k]}", color=cq.Color(*RGB[c["colors"][k]]))
for x in wires:
    asm.add(tube(x["pts"], WD / 2), name=x["name"], color=cq.Color(*RGB[x["color"]]))

if __name__ == "__main__":
    from OCP.BRepExtrema import BRepExtrema_DistShapeShape
    parts = []
    def walk(a, loc=cq.Location()):
        l = loc * a.loc
        if a.obj is not None:
            parts.append((a.name, a.obj.val().moved(l)))
        for ch in a.children:
            walk(ch, l)
    walk(asm)
    cab = [p for p in parts if p[0].startswith(("W1_", "W2_", "GND_", "12V_", "relay_NO"))]
    solid = [p for p in parts if p not in cab and p[0] != f"{L.NAME}_lid" and not p[0].startswith("gland")]
    for c in cables:
        print(f"  {c['name']}: route {c['route']} mm, free ribbon {c['stock'] - 2 * PLUG:.0f}")
    for x in wires:
        print(f"  {x['name']}: route {x['route']}  cut {x['cut']}")
    for n, s in cab:
        hits = []
        for m, t in solid:
            e = BRepExtrema_DistShapeShape(s.wrapped, t.wrapped); e.Perform()
            if e.Value() < 0.3:
                vol = s.intersect(t).Volume()
                if vol > 0.5:
                    hits.append(f"{m} ({vol:.1f})")
        print(f"  {n}: top z {s.BoundingBox().zmax:.1f} (lid inside to {L.HB + L.HL - 2.5}), hits {hits}")
    asm.save(f"wrangler_air_{L.TAG}.step")
    asm.save(f"wrangler_air_{L.TAG}.glb", tolerance=0.2, angularTolerance=0.4)
    r1 = lambda p: [round(q, 2) for q in p]
    json.dump({"cables": [{**{k: c[k] for k in ("name", "from", "to", "route", "stock", "colors")}, "pts": [r1(p) for p in c["pts"]]} for c in cables],
               "wires": [{**x, "pts": [r1(p) for p in x["pts"]]} for x in wires]}, open(f"cables_{L.TAG}.json", "w"), indent=1)
    print("ok")
