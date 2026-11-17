"""Cable drawing for the Hammond 1591S variant: top view (lid off) and side view with every cable, from cables_1591s.json.
Patches section 3 of hammond_1591s.html and the embedded 3D model of both pages.
Run after cables_1591s.py:  python3 draw_cables_1591s.py
"""
import base64
import json
import re
import layout_1591s as L

D = json.load(open("cables_1591s.json"))
S = 4.0                                    # px per mm
COL = {"black": "#161616", "red": "#d42020", "yellow": "#f0c419", "white": "#f4f4f4", "orange": "#ff8c0d", "blue": "#2766e6", "shrink": "#0a0a0a"}
OX, OY = 50 + L.W / 2 * S, 60 + L.L / 2 * S          # top view: box centre
SX, SY = OX + L.W / 2 * S + 70, OY                   # side view: z = 0 at SX


def top(p):
    return OX + p[0] * S, OY - p[1] * S


def side(p):
    return SX + p[2] * S, OY - p[1] * S


def rect(x0, y0, x1, y1, cls, proj, z0=0, z1=0, rx=0, extra=""):
    if proj is top:
        a, b = top((x0, y1, 0)), top((x1, y0, 0))
    else:
        a, b = side((0, y1, z0)), side((0, y0, z1))
    return f'<rect x="{a[0]:.1f}" y="{a[1]:.1f}" width="{b[0]-a[0]:.1f}" height="{b[1]-a[1]:.1f}" rx="{rx}" class="{cls}" {extra}/>'


def poly(pts, proj, col, w, extra=""):
    q = " ".join(f"{proj(p)[0]:.1f},{proj(p)[1]:.1f}" for p in pts)
    return f'<polyline points="{q}" fill="none" stroke="{col}" stroke-width="{w:.1f}" stroke-linejoin="round" stroke-linecap="round" {extra}/>'


# parts as boxes: (x0, x1, y0, y1, z0, z1, class, label)
F = L.FLOOR
DB = L.DB
P = [
    (-22.8, 22.8, DB, L.TOP_Y, L.DIN_Z - 11.1, L.DIN_Z + 11.1, "din", "DinMeter"),
    (-26.5, 26.5, L.TOP_Y, L.TOP_Y + 2.5, L.DIN_Z - 15, L.DIN_Z + 15, "din", ""),
    (-11.7 - 2.3, -11.7 + 2.3, DB - 8, DB, L.DIN_Z - 8, L.DIN_Z + 2, "plug", ""),
    (-18 - 2.3, -18 + 2.3, DB - 8, DB, L.DIN_Z - 8, L.DIN_Z + 2, "plug", ""),
    (-15.1 - 4.4, -15.1 + 4.4, DB - 10, DB, L.DIN_Z + 4.1, L.DIN_Z + 11.1, "green", ""),
    (L.RXC - 12, L.RXC + 12, L.RY0, L.RY0 + 48, F, F + 8, "mod", "Relay"),
    (L.RXC - 5.25, L.RXC + 5.25, L.RY0 + 10, L.RY0 + 25.2, F + 8, F + 14, "coil", ""),
    (L.RXC - 6, L.RXC + 6, L.RY0, L.RY0 + 10, F + 8, F + 20, "term", ""),
    (L.RXC - 4.95, L.RXC + 4.95, L.RY0 + 48, L.RY0 + 54.5, F + 1.7, F + 6.3, "plug", ""),
    (L.AXC - 12, L.AXC + 12, L.AY0, L.AY0 + 32, F, F + 8, "mod", "ADC"),
    (L.AXC - 4.5, L.AXC + 4.5, L.AY0 + 0.5, L.AY0 + 8.5, F + 8, F + 19, "term", ""),
    (L.AXC - 4.95, L.AXC + 4.95, L.AY0 + 32, L.AY0 + 38.5, F + 1.7, F + 6.3, "plug", ""),
]


def box_outline(proj):
    o = []
    if proj is top:
        o.append(rect(-L.W / 2, -L.L / 2, L.W / 2, L.L / 2, "part", top, rx=5 * S))
        o.append(rect(-L.IN_W / 2, -L.IN_L / 2, L.IN_W / 2, L.IN_L / 2, "inner", top, rx=2 * S))
        for sx in (-1, 1):
            for sy in (-1, 1):
                c = top((sx * L.BOSS_X / 2, sy * L.BOSS_Y / 2, 0))
                o.append(f'<circle cx="{c[0]:.1f}" cy="{c[1]:.1f}" r="{L.BOSS_D / 2 * S:.1f}" class="boss"/>')
        # window and slot
        a, b = top((-L.CUT_W / 2, L.TOP_Y, 0)), top((L.CUT_W / 2, L.Y_IN, 0))
        a2, b2 = top((L.SLOT_X - 7, -L.Y_IN, 0)), top((L.SLOT_X + 7, -L.TOP_Y, 0))
        o.append(f'<rect x="{a2[0]:.1f}" y="{a2[1]:.1f}" width="{b2[0]-a2[0]:.1f}" height="{b2[1]-a2[1]:.1f}" class="hole"/>')
    else:
        o.append(rect(0, -L.L / 2, 0, L.L / 2, "part", side, 0, L.HB, rx=4))
        o.append(rect(0, -L.IN_L / 2, 0, L.IN_L / 2, "inner", side, L.FLOOR, L.HB))
        o.append(rect(0, -L.L / 2, 0, L.L / 2, "lid", side, L.HB, L.HB + L.HL, rx=3))
        a, b = side((0, -L.TOP_Y, L.SLOT_Z - 4)), side((0, -L.Y_IN, L.SLOT_Z + 4))
        o.append(f'<rect x="{a[0]:.1f}" y="{b[1]:.1f}" width="{b[0]-a[0]:.1f}" height="{a[1]-b[1]:.1f}" class="hole"/>')
    return o


def parts(proj):
    o = []
    order = sorted(P, key=lambda p: p[5] if proj is top else -p[1])
    for x0, x1, y0, y1, z0, z1, cls, lab in order:
        o.append(rect(x0, y0, x1, y1, cls, proj, z0, z1, rx=2))
        if lab and proj is top:
            c = top(((x0 + x1) / 2, y0 + 3 if lab != "DinMeter" else (y0 + y1) / 2 + 4, 0))
            o.append(f'<text x="{c[0]:.1f}" y="{c[1]:.1f}" class="lbl" text-anchor="middle">{lab}</text>')
    return o


def cables(proj, nums):
    items = []
    for i, c in enumerate(D["cables"]):
        zs = sum(p[2] for p in c["pts"]) / len(c["pts"])
        items.append((zs if proj is top else 0, "g", i, c))
    for i, w in enumerate(D["wires"]):
        zs = sum(p[2] for p in w["pts"]) / len(w["pts"])
        items.append((zs if proj is top else 0, "w", i, w))
    o = []
    for _, kind, i, c in sorted(items, key=lambda t: t[0]):
        if kind == "g":
            for k, ww in enumerate(c["wires"]):
                o.append(poly(ww, proj, "#333", 1.3 * S + 1.2))
                o.append(poly(ww, proj, COL[c["colors"][k]], 1.3 * S - 0.6))
        else:
            o.append(poly(c["pts"], proj, "#000", c["d"] * S + 2, 'opacity=".55"'))
            o.append(poly(c["pts"], proj, COL[c["color"]], c["d"] * S))
    # splices
    for k, p in D["splices"].items():
        if k == "breakout":
            continue
        c = proj(p)
        o.append(f'<rect x="{c[0]-9:.1f}" y="{c[1]-5:.1f}" width="18" height="10" rx="4" fill="#111" stroke="#888"/>')
    # number tags
    if proj is top:
        for n, (pt) in nums:
            c = top(pt)
            o.append(f'<circle cx="{c[0]:.1f}" cy="{c[1]:.1f}" r="9" class="tag"/><text x="{c[0]:.1f}" y="{c[1]+4:.1f}" class="tagt">{n}</text>')
    return o


def tagpoint(pts, t=0.5):
    import math
    segs = [(a, b, math.dist(a, b)) for a, b in zip(pts, pts[1:])]
    tot = sum(s[2] for s in segs) * t
    for a, b, l in segs:
        if tot <= l:
            f = tot / l if l else 0
            return tuple(a[i] + f * (b[i] - a[i]) for i in range(3))
        tot -= l
    return pts[-1]


TAGS = [(1, tagpoint(D["cables"][0]["pts"], .42)), (2, tagpoint(D["cables"][1]["pts"], .4))]
tpos = {"car_12V_AUX1": .55, "car_GND": .75, "12V_to_relay_COM": .7, "relay_NO_to_K2": .35, "12V_to_DinMeter_plus": .55,
        "GND_to_DinMeter_minus": .42, "sensor_SIG": .6, "sensor_GND": .75, "sensor_5V": .5, "sensor_bundle": .7}
for i, w in enumerate(D["wires"]):
    TAGS.append((i + 3, tagpoint(w["pts"], tpos.get(w["name"], .5))))

W_SVG = int(SX + (L.HB + L.HL) * S + 60)
H_SVG = int(OY + L.L / 2 * S + 100)
svg = [f'<svg viewBox="0 0 {W_SVG} {H_SVG}" style="color:var(--fg)">']
svg.append(f'<text x="{OX:.0f}" y="28" class="dt">Вид со стороны крышки (крышка снята)</text>')
svg.append(f'<text x="{SX + 22*S:.0f}" y="28" class="dt">Вид сбоку</text>')
svg.append(f'<text x="{OX:.0f}" y="48" class="note" text-anchor="middle">верхний торец, DinMeter</text>')
svg.append(f'<text x="{OX:.0f}" y="{OY + L.L/2*S + 52:.0f}" class="note" text-anchor="middle">нижний торец, паз под провода</text>')
svg += box_outline(top) + parts(top) + cables(top, TAGS)
svg += box_outline(side) + parts(side) + cables(side, [])
svg.append(f'<text x="{SX:.0f}" y="{OY + L.L/2*S + 22:.0f}" class="note">дно</text><text x="{SX + L.HB*S:.0f}" y="{OY + L.L/2*S + 22:.0f}" class="note" text-anchor="middle">крышка</text>')
# 20 mm scale bar
sb = OY + L.L / 2 * S + 48 + 30
svg.append(f'<line x1="{OX - L.W/2*S:.0f}" y1="{sb:.0f}" x2="{OX - L.W/2*S + 20*S:.0f}" y2="{sb:.0f}" stroke="currentColor" stroke-width="3"/><text x="{OX - L.W/2*S + 20*S + 8:.0f}" y="{sb + 4:.0f}" class="note">20 мм</text>')
svg.append("</svg>")
svg = "".join(svg)

link = {
    "Kalitec FLRY 1.5 red": "https://www.galaxus.ch/search?q=Kalitec%20FLRY%201.5%20rot",
    "Kalitec FLRY 1.5 black": "https://www.galaxus.ch/search?q=Kalitec%20FLRY%201.5%20schwarz",
}
RU = {"car_12V_AUX1": "+12 В от AUX1", "car_GND": "Масса", "12V_to_relay_COM": "+12 В на COM реле", "relay_NO_to_K2": "NO реле на K2 (через D1)",
      "12V_to_DinMeter_plus": "+12 В на клемму + DinMeter", "GND_to_DinMeter_minus": "Масса на клемму − DinMeter",
      "sensor_SIG": "Датчик, сигнал, на IN ADC", "sensor_GND": "Датчик, масса, на GND ADC", "sensor_5V": "Датчик, +5 В, врезка в красный провод Grove PORT.A",
      "sensor_bundle": "Три провода датчика в термоусадке"}
rows = []
g10 = '<a href="https://www.galaxus.ch/en/s1/product/m5stack-10cm-grove-cable-5-pieces-single-board-computer-kits-24874618" target="_blank">M5Stack Grove 10 см</a>'
for i, c in enumerate(D["cables"]):
    rows.append(f'<tr><td><span class="tagi">{i+1}</span></td><td>Grove {"PORT.A → ADC" if i == 0 else "PORT.B → реле"}</td><td>{g10}, плоский 4-жильный</td>'
                f'<td>{c["route"]:.0f} мм (вся свободная длина)</td><td>готовый, не резать</td></tr>')
for i, w in enumerate(D["wires"]):
    prod = w["product"]
    if prod in link:
        prod = f'<a href="{link[prod]}" target="_blank">{prod}</a>'
    elif prod.startswith("Donau"):
        prod = f'<a href="https://www.galaxus.ch/search?q=Donau%20Elektronik%20LiY%200.25" target="_blank">{prod}</a>'
    else:
        prod = prod.replace("Electraline heat shrink", '<a href="https://www.galaxus.ch/search?q=Electraline%20Schrumpfschlauch%209.5" target="_blank">Electraline</a>')
    out = " + снаружи до места в машине" if w["name"] in ("car_12V_AUX1", "car_GND", "relay_NO_to_K2", "sensor_bundle") else ""
    rows.append(f'<tr><td><span class="tagi">{i+3}</span></td><td>{RU[w["name"]]}</td><td>{prod}</td><td>{w["route"]:.0f} мм</td><td><b>{w["cut"]} мм</b>{out}</td></tr>')

section = f'''<section class="card" id="cables"><h2>3. Раскладка и кабели внутри</h2>
<p class="lead">Реле и ADC развёрнуты: разъёмы Grove смотрят на DinMeter, клеммники смотрят на паз. Кабели Grove нарисованы по жилам (чёрный, красный, жёлтый, белый, по 1.3 мм), с реальной длиной. Проверка модели: ни один кабель не задевает детали, до крышки остаётся не меньше 6 мм.</p>
{svg}
<table><thead><tr><th>#</th><th>Что</th><th>Провод</th><th>Трасса в коробке</th><th>Отрезать</th></tr></thead><tbody>{"".join(rows)}</tbody></table>
<ul>
<li><b>Grove 10 см вместо комплектных 20 см.</b> У 10 см между вилками остаётся 86 мм, и это ровно трасса с одной плоской петлёй над модулем (на рисунке). Комплектные 20 см дают 186 мм, лишние 100 мм на каждом кабеле придётся складывать гармошкой втрое над модулями и стягивать стяжкой: влезет, но под крышкой будет тесно.</li>
<li>«Отрезать» = трасса + 20 мм на зачистку и клемму, округлено до 5 мм. Наружные концы (до AUX1, массы, K2 и датчика) меряются по машине.</li>
<li>Врезки +12 В и массы в нижней свободной полосе между бобышками, под термоусадку BGS. Врезка +5 В датчика в красную жилу Grove PORT.A около DinMeter.</li>
<li>Порядок контактов COM, NO, NC на клеммнике реле проверить по маркировке на плате.</li>
</ul></section>
'''

p = "hammond_1591s.html"
h = open(p).read()
h = re.sub(r'<section class="card"( id="cables")?><h2>3\. .*?</section>\n?', lambda m: section, h, count=1, flags=re.S)
if ".tag{" not in h:
    h = h.replace("</style>", ".tag{fill:#fff;stroke:#111;stroke-width:1.5}.tagt{font:700 11px var(--f);text-anchor:middle;fill:#111}"
                  ".tagi{display:inline-block;width:20px;height:20px;border-radius:50%;border:1.5px solid currentColor;text-align:center;font:700 11px/18px var(--f)}"
                  ".hole{fill:var(--bg);stroke:var(--accent);stroke-width:1.5}.lid{fill:#4d535a;stroke:#222}.plug{fill:#f7f7f7;stroke:#888}.green{fill:#33a64d;stroke:#1d6b31}.coil{fill:#f98c1a;stroke:#a44}"
                  ".card table{margin-top:12px}</style>")
glb = base64.b64encode(open("wrangler_air_1591s.glb", "rb").read()).decode()
for f in (p, "hammond_1591s_3d.html"):
    t = h if f == p else open(f).read()
    t = re.sub(r'const GLB="[^"]*"', lambda m: f'const GLB="{glb}"', t, count=1)
    open(f, "w").write(t)
print("ok", len(svg), W_SVG, H_SVG)
