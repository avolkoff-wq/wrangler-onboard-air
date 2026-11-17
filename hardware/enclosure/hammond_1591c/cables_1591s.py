"""Wrangler Air in the Hammond 1591S: every cable inside the box, with real lengths.

Grove cables are the flat 4-wire M5Stack ones (black, red, yellow, white, each about 1.3 mm), modelled wire by wire.
M5Stack sells them by overall length: 5, 10, 20 cm (docs.m5stack.com/en/accessory/cable/Grove_Cable).
The relay and the ADC each ship with a 20 cm cable. Overall length minus the two plug bodies (about 7 mm each)
is the free ribbon that has to lie somewhere in the box.

The cut-to-length wires (FLRY 1.5, Donau 0.25) follow the routes below; the cut length is the route plus 20 mm
for stripping and the terminal or splice, rounded up to 5 mm.

Run:  python3 cables_1591s.py
Writes wrangler_air_1591s.step / .glb (box, parts and all cables) and cables_1591s.json (routes and lengths for the drawing).
"""
import json
import math
import cadquery as cq
import layout_1591s as L

PLUG = 7.0           # length of one Grove plug body along the cable
WIRE_D = 1.3         # one Grove conductor
GROVE_COLORS = ("black", "red", "yellow", "white")    # order across the ribbon, GND 5V SIG SIG
RGB = {"black": (0.08, 0.08, 0.08), "red": (0.85, 0.12, 0.12), "yellow": (0.95, 0.80, 0.10), "white": (0.95, 0.95, 0.95),
       "orange": (1.0, 0.55, 0.05), "blue": (0.15, 0.40, 0.90), "shrink": (0.04, 0.04, 0.04)}


def v(a, b):
    return tuple(y - x for x, y in zip(a, b))


def norm(a):
    n = math.sqrt(sum(x * x for x in a)) or 1.0
    return tuple(x / n for x in a)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def length(pts):
    return sum(math.dist(a, b) for a, b in zip(pts, pts[1:]))


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
    """Four parallel conductors along pts. flat[i] = the direction across the ribbon at point i."""
    out = []
    for k in range(4):
        off = (k - 1.5) * WIRE_D
        out.append([tuple(p[j] + off * f[j] for j in range(3)) for p, f in zip(pts, flat)])
    return out


def fit_loop(a, a_dir, b, b_dir, via, target):
    """Route from a (leaving along a_dir) to b (arriving against b_dir) through a loop whose top is scaled
    so that the route is exactly `target` long. via(h) returns the middle points for loop size h."""
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

# ---------- Grove cables, 10 cm (free ribbon 100 - 2 x 7 = 86 mm) ----------
GROVE_LEN = 100.0
free = GROVE_LEN - 2 * PLUG
down = (0, -1, 0)
# PORT.A (DinMeter, x -11.7) -> ADC (x +18): the spare lies as a flat U above the ADC, between the units and the lid
A, Ab = L.DIN_A, L.ADC_GROVE
ra = fit_loop(A, down, Ab, (0, -1, 0),
              lambda h: [(-4.0, 20.0, 26.0), (14.0 + h, 20.0, 29.0), (14.0 + h, 29.0, 30.0), (12.0, 29.0, 28.0), (Ab[0], 24.0, 14.0), (Ab[0], Ab[1] + 5, Ab[2] + 1.5)], free)
# PORT.B (DinMeter, x -18) -> relay (x -23.5): the spare lies as a flat U above the relay
B, Bb = L.DIN_B, L.REL_GROVE
rb = fit_loop(B, down, Bb, (0, -1, 0),
              lambda h: [(-17.0, 17.0, 27.0), (-15.0, 10.0 - h, 30.0), (-31.0, 10.0 - h, 31.0), (-31.0, 12.0, 27.0), (Bb[0], 25.0, 13.0), (Bb[0], Bb[1] + 5, Bb[2] + 1.5)], free)
for name, pts, frm, to in (("grove_PORT_A_to_ADC", ra, "DinMeter PORT.A", "ADC v1.1"), ("grove_PORT_B_to_relay", rb, "DinMeter PORT.B", "Mini 3A Relay")):
    # across the ribbon: along z at the DinMeter (plug pins in a row along z), along x at the units (plugs lie flat)
    flat = []
    for i, p in enumerate(pts):
        t = i / (len(pts) - 1)
        f = norm((t, 0, 1 - t))
        flat.append(f)
    cables.append({"name": name, "from": frm, "to": to, "kind": "grove", "pts": pts, "route": round(length(pts), 1),
                   "wires": ribbon(pts, flat), "colors": GROVE_COLORS, "stock": GROVE_LEN})

# ---------- cut-to-length wires ----------
S = L.SLOT_IN
OUT = -L.L / 2 - 6          # 6 mm outside the lower end wall
E_NO, E_12, E_G, E_SB = (-19.5, 20.0), (-16.3, 21.7), (-16.3, 18.3), (-12.8, 20.0)   # x, z of each wire in the slot
SP12, SPG, BRK = (-6.0, -45.0, 14.0), (4.0, -45.0, 12.0), (2.0, -48.5, 8.0)
# +5 V splice into the red conductor of the PORT.A Grove ribbon, a third of the way along it
spl = cables[0]["wires"][1][2]


def w(name, pts, col, d, frm, to, product):
    wires.append({"name": name, "pts": pts, "color": col, "d": d, "from": frm, "to": to, "product": product,
                  "route": round(length(pts), 1), "cut": int(math.ceil((length(pts) + 20) / 5) * 5)})


FLRY_R, FLRY_B, DON = "Kalitec FLRY 1.5 red", "Kalitec FLRY 1.5 black", "Donau 0.25"
w("car_12V_AUX1", [(E_12[0], OUT, E_12[1]), (E_12[0], S[1] + 3, E_12[1]), (SP12[0] - 4, SP12[1], SP12[2] + 1), SP12], "red", 2.2, "slot", "+12 V splice", FLRY_R)
w("car_GND", [(E_G[0], OUT, E_G[1]), (E_G[0], S[1] + 3, E_G[1]), (SPG[0] - 4, SPG[1] - 1, SPG[2] + 1), SPG], "black", 2.2, "slot", "GND splice", FLRY_B)
w("12V_to_relay_COM", [SP12, (SP12[0] - 5, SP12[1] + 1, SP12[2] + 1), (-21.0, -40.0, 16.0), (L.REL_PIN["COM"][0], L.REL_PIN["COM"][1] - 3, L.REL_PIN["COM"][2]), L.REL_PIN["COM"]],
  "red", 2.2, "+12 V splice", "relay COM", FLRY_R)
w("relay_NO_to_K2", [L.REL_PIN["NO"], (L.REL_PIN["NO"][0], L.REL_PIN["NO"][1] - 6, L.REL_PIN["NO"][2] + 2), (E_NO[0], S[1] + 4, E_NO[1]), (E_NO[0], OUT, E_NO[1])],
  "red", 2.2, "relay NO", "slot, on to D1 and K2 pin 86", FLRY_R)
w("12V_to_DinMeter_plus", [SP12, (-8.0, -36.0, 19.0), (-8.0, 14.0, 24.0), (L.DIN_PLUS[0], 19.0, L.DIN_PLUS[2]), L.DIN_PLUS],
  "orange", 1.0, "+12 V splice", "DinMeter terminal +", DON + " orange")
w("GND_to_DinMeter_minus", [SPG, (-6.0, -36.0, 18.0), (-6.5, 14.0, 23.0), (L.DIN_MINUS[0], 18.5, L.DIN_MINUS[2]), L.DIN_MINUS],
  "blue", 1.0, "GND splice", "DinMeter terminal -", DON + " blue")
w("sensor_SIG", [BRK, (12.0, -42.0, 10.0), (L.ADC_PIN["IN"][0], L.ADC_PIN["IN"][1] - 7, L.ADC_PIN["IN"][2]), L.ADC_PIN["IN"]],
  "yellow", 1.0, "sensor bundle", "ADC IN", DON + " yellow")
w("sensor_GND", [BRK, (14.0, -43.0, 9.0), (L.ADC_PIN["GND"][0], L.ADC_PIN["GND"][1] - 7, L.ADC_PIN["GND"][2]), L.ADC_PIN["GND"]],
  "black", 1.0, "sensor bundle", "ADC GND", DON + " black")
w("sensor_5V", [BRK, (-2.0, -38.0, 16.0), (-3.0, 12.0, 21.0), spl], "red", 1.0, "sensor bundle", "splice into PORT.A Grove red", DON + " red")
w("sensor_bundle", [(E_SB[0], OUT, E_SB[1]), (E_SB[0], S[1] + 3, E_SB[1]), (-6.0, S[1] + 2, 12.0), BRK], "shrink", 3.4, "slot", "breakout", "3 x Donau 0.25 in Electraline heat shrink")

# ---------- assembly ----------
asm = L.asm
for c in cables:
    for k, pts in enumerate(c["wires"]):
        asm.add(tube(pts, WIRE_D / 2), name=f"{c['name']}_{c['colors'][k]}", color=cq.Color(*RGB[c["colors"][k]]))
for x in wires:
    asm.add(tube(x["pts"], x["d"] / 2), name=x["name"], color=cq.Color(*RGB[x["color"]]))
for nm, p in (("shrink_12V_splice", SP12), ("shrink_GND_splice", SPG)):
    asm.add(cq.Workplane("XY").add(cq.Solid.makeCylinder(2.3, 10, cq.Vector(p[0] - 5, p[1], p[2]), cq.Vector(1, 0, 0))), name=nm, color=cq.Color(*RGB["shrink"]))
asm.add(cq.Workplane("XY").add(cq.Solid.makeCylinder(1.6, 8, cq.Vector(*spl) - cq.Vector(0, 0, 4), cq.Vector(0, 0, 1))), name="shrink_5V_splice", color=cq.Color(*RGB["shrink"]))

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
    cab = [p for p in parts if p[0].startswith(("grove_P", "car_", "12V_", "GND_", "relay_NO", "sensor_"))]
    solid = [p for p in parts if p not in cab and not p[0].startswith("shrink") and p[0] != "hammond_1591s_lid"]
    lid_z = L.HB
    print("routes:")
    for c in cables:
        print(f"  {c['name']}: {c['route']} mm of {c['stock'] - 2 * PLUG:.0f} free ribbon")
    for x in wires:
        print(f"  {x['name']}: route {x['route']}  cut {x['cut']}")
    print("clearance of each cable to the parts (negative = touches):")
    for n, s in cab:
        worst = []
        for m, t in solid:
            e = BRepExtrema_DistShapeShape(s.wrapped, t.wrapped); e.Perform()
            if e.Value() < 0.3:
                vol = s.intersect(t).Volume()
                if vol > 0.5:
                    worst.append(f"{m} ({vol:.1f} mm3)")
        top = s.BoundingBox().zmax
        print(f"  {n}: top z {top:.1f} (lid {lid_z}), hits: {worst}")
    asm.save("wrangler_air_1591s.step")
    asm.save("wrangler_air_1591s.glb", tolerance=0.2, angularTolerance=0.4)
    r1 = lambda p: [round(q, 2) for q in p]
    json.dump({"cables": [{**{k: c[k] for k in ("name", "from", "to", "route", "stock", "colors")}, "pts": [r1(p) for p in c["pts"]],
                           "wires": [[r1(p) for p in ww] for ww in c["wires"]]} for c in cables],
               "wires": [{**x, "pts": [r1(p) for p in x["pts"]]} for x in wires],
               "splices": {"12V": SP12, "GND": SPG, "breakout": BRK, "5V": r1(spl)}}, open("cables_1591s.json", "w"), indent=1)
    print("ok")
