"""Wrangler Air in the AliExpress F-100x68x50 box, schematic A (power_schematic.png): every cable, real lengths.

  W1  Grove 5 cm   PORT.B -> Mini Relay (free ribbon 50 - 2 x 7 = 36 mm, folded under the DinMeter plugs)
  W2  Grove 2 m    PORT.A -> out through the right M12 gland -> sensor (yellow G13 = signal, white unused)
  left gland:  BK GND and RD +12 V to the DinMeter terminal, RD +12 V to relay COM (the Y splice -X2 is outside)
  right gland: W2 and VT from relay NO to the diode node -X3 outside
Wire 20 AWG silicone (about 1.8 mm over the insulation).

Run:  WA_BOX=f100 python3 cables_f100.py   ->  wrangler_air_f100.step / .glb, cables_f100.json   (relay along the box)
      WA_BOX=f100x python3 cables_f100.py  ->  wrangler_air_f100x.*                               (relay across, alternative)
"""
import json
import math
import os
os.environ.setdefault("WA_BOX", "f100")   # or f100x: relay across the box
import cadquery as cq
import layout_box as L

PLUG = 7.0
WIRE_D = 1.3
WD = 1.8
GROVE_COLORS = ("black", "red", "yellow", "white")
RGB = {"black": (0.08, 0.08, 0.08), "red": (0.85, 0.12, 0.12), "yellow": (0.95, 0.80, 0.10), "white": (0.95, 0.95, 0.95),
       "orange": (0.98, 0.50, 0.05), "blue": (0.15, 0.35, 0.85),
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


def fit_loop(a, a_dir, b, b_dir, via, target, kb=4.0):
    lo, hi = 0.0, 1.0
    def route(h):
        return [a, tuple(a[i] + 4 * a_dir[i] for i in range(3))] + via(h) + [tuple(b[i] - kb * b_dir[i] for i in range(3)), b]
    while length(route(hi)) < target:
        hi *= 2
    for _ in range(60):
        m = (lo + hi) / 2
        lo, hi = (m, hi) if length(route(m)) < target else (lo, m)
    return route(hi)


cables, wires = [], []
down = (0, -1, 0)
gS, gP = L.GLANDS[L.GLAND_SENSOR], L.GLANDS[L.GLAND_POWER]   # (x, z) of the sensor/ARB gland and of the power gland
YIN, YOUT = -L.Y_IN + 6, -L.TOP_Y - 30  # just inside the lock nut, 30 mm outside the box
B, Bb = L.DIN_B, L.REL_GROVE
A = L.DIN_A
COM, NO = L.REL_PIN["COM"], L.REL_PIN["NO"]
DM, DP = L.DIN_MINUS, L.DIN_PLUS
sx, sz = gS
px, pz = gP


def w(name, pts, col, frm, to):
    wires.append({"name": name, "pts": pts, "color": col, "d": WD, "from": frm, "to": to,
                  "route": round(length(pts), 1), "cut": int(math.ceil((length(pts) + 20) / 5) * 5)})


if not L.REL_ACROSS and L.TAG != "cust_adc":
    # ---- f100 / cust: relay along the box on the left. Left gland = sensor + ARB, right gland = power ----
    # W1 Grove 5 cm: the relay plug sits right under the DinMeter plugs and faces them, so the ribbon
    # drops from PORT.B over the relay plug, folds back under the DinMeter plugs and comes into the relay plug from their side.
    w1 = fit_loop(B, down, Bb, (0, -1, 0),
                  lambda h: [(B[0] + 1, 16.0, 12.5), (Bb[0], 16.5, 10.3), (Bb[0], 25.0 + h, 9.9), (Bb[0], 27.0 + h, 7.6)], 50 - 2 * PLUG)
    w1_stock = 50
    # W2 Grove 2 m: PORT.A straight down over the relay into the left gland
    gx = sx + 1.2
    w2 = [A, (A[0], A[1] - 4, A[2]), (-8.0, 8.0, 24.0), (-8.0, -35.5, 24.0), (gx, -39.0, 20.0), (gx, YIN, sz), (gx, YOUT, sz)]
    if L.TAG == "cust_def":
        # schematic C: Unit 2Relay, terminals on the side towards the middle. Left gland M12: Grove 2 m + violet (K1 NO); right: power + orange (K2 NO)
        w2 = [A, (A[0], A[1] - 4, A[2]), (-8.0, 8.0, 25.0), (-8.0, -35.5, 25.0), (sx - 1.0, -39.0, 20.0), (sx - 1.0, YIN, sz), (sx - 1.0, YOUT, sz)]
        C1, N1, C2, N2 = (L.REL_PIN[k] for k in ("COM1", "NO1", "COM2", "NO2"))
        w("relay_NO_to_ARB", [N1, (N1[0], N1[1], N1[2] + 4), (0.0, N1[1], 23.0), (0.0, -34.0, 23.0), (sx + 2.6, -40.0, sz),
                              (sx + 2.6, YIN, sz), (sx + 2.6, YOUT, sz)], "violet", "2Relay K1 NO", "left gland, on to V1 and the ARB violet wire (-X3)")
        # orange to the valve leaves with the power wires (right gland M12: BK, RD, RD, OG ~4.5 mm bundle)
        w("relay_K2_NO_to_valve", [N2, (N2[0], N2[1], N2[2] + 4), (4.0, N2[1], 21.0), (px - 3.0, -38.0, 19.0),
                                   (px - 2.5, YIN, pz + 1.5), (px - 2.5, YOUT, pz + 1.5)], "orange", "2Relay K2 NO", "right gland, to the deflate valve -Y1 (+)")
        w("jumper_COM1_COM2", [C1, (C1[0], C1[1], C1[2] + 2.5), (-2.0, C1[1], C1[2] + 2.5), (-2.0, C2[1], C2[2] + 2.5), (C2[0], C2[1], C2[2] + 2.5), C2],
          "red", "2Relay K1 COM", "2Relay K2 COM")
    else:
        # violet: relay NO straight down into the left gland, beside the Grove
        vx = sx - 3.6
        w("relay_NO_to_ARB", [NO, (NO[0], NO[1] - 3, NO[2]), (vx, -38.0, 17.5), (vx, YIN, sz), (vx, YOUT, sz)],
          "violet", "relay NO", "left gland, on to V1 and the ARB violet wire (-X3)")
    # power from the right gland: black and red up the free right half to the DinMeter terminal,
    # red to COM low along the end wall, under the Grove and the violet
    w("GND_to_DinMeter_minus", [(px - 2, YOUT, pz - 1.5), (px - 2, YIN, pz - 1.5), (8.0, -30.0, 26.0), (2.0, 12.0, 28.0),
                                (DM[0], DM[1] - 5, DM[2]), DM], "black", "right gland", "DinMeter terminal -")
    w("12V_to_DinMeter_plus", [(px, YOUT, pz + 1.5), (px, YIN, pz + 1.5), (10.0, -30.0, 27.0), (4.0, 12.0, 29.0),
                               (DP[0], DP[1] - 5, DP[2]), DP], "red", "right gland", "DinMeter terminal +")
    if L.TAG == "cust_def":
        w("12V_to_relay_COM", [(px + 2, YOUT, pz - 1.5), (px + 2, YIN, pz - 1.5), (9.0, -30.0, 24.0), (5.0, COM[1] + 4, 25.0),
                               (COM[0], COM[1] + 4, COM[2] + 5), COM], "red", "right gland", "2Relay K1 COM")
    else:
        w("12V_to_relay_COM", [(px + 2, YOUT, pz - 1.5), (px + 2, YIN, pz - 1.5), (8.0, -38.5, 12.0), (COM[0], -38.5, 12.0),
                               (COM[0], COM[1] - 3, COM[2]), COM], "red", "right gland", "relay COM")
elif L.TAG == "cust_adc":
    # ---- own box with ADC (schematic B): relay left, ADC right. Left gland = power, right gland = sensor (OG BU YE) + violet ----
    w1 = fit_loop(B, down, Bb, (0, -1, 0),
                  lambda h: [(B[0] + 1, 16.0, 12.5), (Bb[0], 16.5, 10.3), (Bb[0], 25.0 + h, 9.9), (Bb[0], 27.0 + h, 7.6)], 50 - 2 * PLUG)
    w1_stock = 50
    Ab = L.ADC_GROVE
    w2 = fit_loop(A, down, Ab, (0, -1, 0),
                  lambda h: [(A[0] + 4, 15.0, 18.0 + h), (6.0, 15.0, 18.0 + h), (Ab[0], 27.0, 9.0), (Ab[0], 27.5, 7.6)], 100 - 2 * PLUG)
    w2_stock, w2_to = 100, "ADC v1.1"
    G, IN_ = L.ADC_PIN["GND"], L.ADC_PIN["IN"]
    xs = sx - 2.0
    w("sensor_GND_to_ADC", [(xs, YOUT, sz - 1.5), (xs, YIN, sz - 1.5), (G[0], -26.0, 16.0), (G[0], G[1] - 4, G[2]), G], "blue", "right gland", "ADC GND")
    w("sensor_signal_to_ADC", [(xs + 2, YOUT, sz + 1.5), (xs + 2, YIN, sz + 1.5), (IN_[0], -26.0, 17.0), (IN_[0], IN_[1] - 4, IN_[2]), IN_],
      "yellow", "right gland", "ADC IN")
    X4 = w2[len(w2) // 2]
    w("sensor_5V_to_X4", [(xs + 1, YOUT, sz), (xs + 1, YIN, sz), (6.0, -24.0, 24.0), (X4[0], X4[1] - 6, X4[2] + 1), (X4[0], X4[1], X4[2] + 1)],
      "orange", "right gland", "-X4 solder onto the red conductor of W2")
    vx = sx + 3.4
    w("relay_NO_to_ARB", [NO, (NO[0], NO[1] - 3, NO[2]), (NO[0] + 2, -40.0, 13.0), (vx - 1, -40.5, 13.0), (vx, YIN, sz), (vx, YOUT, sz)],
      "violet", "relay NO", "right gland, on to V1 and the ARB violet wire (-X3)")
    w("GND_to_DinMeter_minus", [(px - 2, YOUT, pz - 1.5), (px - 2, YIN, pz - 1.5), (-1.0, -30.0, 25.0), (-1.0, 12.0, 27.5),
                                (DM[0], DM[1] - 5, DM[2]), DM], "black", "left gland", "DinMeter terminal -")
    w("12V_to_DinMeter_plus", [(px, YOUT, pz + 1.5), (px, YIN, pz + 1.5), (1.0, -30.0, 26.5), (1.0, 12.0, 29.0),
                               (DP[0], DP[1] - 5, DP[2]), DP], "red", "left gland", "DinMeter terminal +")
    w("12V_to_relay_COM", [(px - 2.5, YOUT, pz), (px - 2.5, YIN, pz), (COM[0], -38.0, 15.0), (COM[0], COM[1] - 3, COM[2]), COM],
      "red", "left gland", "relay COM")
else:
    # ---- f100x: relay across the box next to the glands, terminal at the right. Right gland = sensor + ARB, left = power ----
    # W1 needs a 10 cm Grove here (free ribbon 86 mm): down the left side in a loop towards the lid, into the plug at the left end
    w1 = fit_loop(B, down, Bb, (1, 0, 0),
                  lambda h: [(-23.0, 8.0, 20.0 + h), (-27.0, -6.0, 20.0 + h), (-28.0, -19.0, 11.0), (-29.3, -26.0, 7.5)], 100 - 2 * PLUG, kb=1.5)
    w1_stock = 100
    # W2 Grove 2 m: PORT.A across to the right half, over the relay into the right gland
    gx = sx - 1.6
    w2 = [A, (A[0], A[1] - 4, A[2]), (4.0, 8.0, 22.0), (gx, -15.0, 22.0), (gx, -38.0, 20.0), (gx, YIN, sz), (gx, YOUT, sz)]
    # violet: relay NO out of the terminal end, down and straight into the right gland, beside the Grove
    vx = sx + 3.4
    w("relay_NO_to_ARB", [NO, (NO[0] + 4, NO[1], NO[2]), (NO[0] + 4, -38.0, 18.0), (vx, -39.0, sz), (vx, YIN, sz), (vx, YOUT, sz)],
      "violet", "relay NO", "right gland, on to V1 and the ARB violet wire (-X3)")
    # power from the left gland: black and red up the middle to the DinMeter terminal; red to COM low across, under the Grove and violet
    w("GND_to_DinMeter_minus", [(px - 2, YOUT, pz - 1.5), (px - 2, YIN, pz - 1.5), (-10.0, -30.0, 24.0), (-6.0, 12.0, 27.0),
                                (DM[0], DM[1] - 5, DM[2]), DM], "black", "left gland", "DinMeter terminal -")
    w("12V_to_DinMeter_plus", [(px, YOUT, pz + 1.5), (px, YIN, pz + 1.5), (-8.0, -30.0, 25.5), (-4.0, 12.0, 28.5),
                               (DP[0], DP[1] - 5, DP[2]), DP], "red", "left gland", "DinMeter terminal +")
    w("12V_to_relay_COM", [(px + 2, YOUT, pz - 1.5), (px + 2, YIN, pz - 1.5), (-8.0, -39.0, 13.0), (COM[0] + 3, -39.0, 13.0),
                           (COM[0] + 3, COM[1], COM[2]), COM], "red", "left gland", "relay COM")

w2n = ("W2_grove_PORT_A_to_ADC", "ADC v1.1", w2_stock) if L.TAG == "cust_adc" else ("W2_grove_PORT_A_to_sensor", "sensor (via gland, 2 m)", 2000)
for name, pts, frm, to, stock in (("W1_grove_PORT_B_to_relay", w1, "DinMeter PORT.B", "Unit 2Relay" if L.TAG == "cust_def" else "Mini 3A Relay", w1_stock),
                                  (w2n[0], w2, "DinMeter PORT.A", w2n[1], w2n[2])):
    cables.append({"name": name, "from": frm, "to": to, "pts": pts, "route": round(length(pts), 1), "stock": stock,
                   "wires": ribbon(pts, [(1, 0, 0)] * len(pts) if name.startswith("W2") or not L.REL_ACROSS
                                   else [(1, 0, 0)] * (len(pts) - 3) + [(0.5, 0.87, 0), (0, 1, 0), (0, 1, 0)]),
                   "colors": GROVE_COLORS})

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
    cab = [p for p in parts if p[0].startswith(("W1_", "W2_", "GND_", "12V_", "relay_NO", "relay_K2", "jumper_", "sensor_"))]
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
