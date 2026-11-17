"""Drawings for the Hammond box variants (WA_BOX=1591s or 1591c): top view (lid off) and side view with every cable,
from cables_<box>.json, plus the cut plan.
1591s: patches section 3 of hammond_1591s.html. 1591c: writes the whole hammond_1591c.html (end wall window, boss trimming,
wire slot, layout and cables). Both: embeds the model in hammond_<box>_3d.html.
Run after cables_box.py:  WA_BOX=1591c python3 draw_box.py
"""
import base64
import json
import re
import layout_box as L
from power_svg import power_svg

D = json.load(open(f"cables_{L.TAG}.json"))
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
                if L.BOSS_CUT_Z and sy > 0:
                    continue
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


def boss_cut(proj):
    """Red hatched: the part of the two DinMeter-end bosses that is cut off."""
    if not L.BOSS_CUT_Z:
        return []
    o = []
    for sx in (-1, 1):
        cx, cy = sx * L.BOSS_X / 2, L.BOSS_Y / 2
        if proj is top:
            c = top((cx, cy, 0))
            sq = top((cx + sx * 4 - 3, cy + 4 + 3, 0))
            o.append(f'<rect x="{sq[0]:.1f}" y="{sq[1]:.1f}" width="{6*S:.1f}" height="{6*S:.1f}" class="cutoff"/>')
            o.append(f'<circle cx="{c[0]:.1f}" cy="{c[1]:.1f}" r="{L.BOSS_D/2*S:.1f}" class="cutoff"/>')
            o.append(f'<circle cx="{c[0]:.1f}" cy="{c[1]:.1f}" r="{L.BOSS_HOLE/2*S:.1f}" fill="#222"/>')
        else:
            a, b = side((0, cy + 7, L.FLOOR)), side((0, cy - L.BOSS_D / 2, L.BOSS_CUT_Z))
            o.append(f'<rect x="{a[0]:.1f}" y="{a[1]:.1f}" width="{b[0]-a[0]:.1f}" height="{b[1]-a[1]:.1f}" class="cutoff"/>')
            a, b = side((0, cy + 7, L.BOSS_CUT_Z)), side((0, cy - L.BOSS_D / 2, L.HB))
            o.append(f'<rect x="{a[0]:.1f}" y="{a[1]:.1f}" width="{b[0]-a[0]:.1f}" height="{b[1]-a[1]:.1f}" class="boss"/>')
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
svg = [f'<svg viewBox="0 0 {W_SVG} {H_SVG}" style="color:var(--fg);max-width:700px;margin:0 auto">']
svg.append(f'<text x="{OX:.0f}" y="28" class="dt">Вид со стороны крышки (крышка снята)</text>')
svg.append(f'<text x="{SX + 22*S:.0f}" y="28" class="dt">Вид сбоку</text>')
svg.append(f'<text x="{OX:.0f}" y="48" class="note" text-anchor="middle">верхний торец, DinMeter</text>')
svg.append(f'<text x="{OX:.0f}" y="{OY + L.L/2*S + 52:.0f}" class="note" text-anchor="middle">нижний торец, паз под провода</text>')
svg += box_outline(top) + boss_cut(top) + parts(top) + cables(top, TAGS)
svg += box_outline(side) + boss_cut(side) + parts(side) + cables(side, [])
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
<p class="lead">Реле и ADC лежат вдоль коробки: разъёмы Grove смотрят на DinMeter, клеммники смотрят на паз. Кабели Grove нарисованы по жилам (чёрный, красный, жёлтый, белый, по 1.3 мм), с реальной длиной. Проверка модели: ни один кабель не задевает детали, до крышки остаётся не меньше 6 мм.</p>
{svg}
<table><thead><tr><th>#</th><th>Что</th><th>Провод</th><th>Трасса в коробке</th><th>Отрезать</th></tr></thead><tbody>{"".join(rows)}</tbody></table>
<ul>
<li><b>Grove 10 см вместо комплектных 20 см.</b> У 10 см между вилками остаётся 86 мм, и это ровно трасса с одной плоской петлёй над модулем (на рисунке). Комплектные 20 см дают 186 мм, лишние 100 мм на каждом кабеле придётся складывать гармошкой втрое над модулями и стягивать стяжкой: влезет, но под крышкой будет тесно.</li>
<li>«Отрезать» = трасса + 20 мм на зачистку и клемму, округлено до 5 мм. Наружные концы (до AUX1, массы, K2 и датчика) меряются по машине.</li>
<li>Врезки +12 В и массы в нижней свободной полосе между бобышками, под термоусадку BGS. Врезка +5 В датчика в красную жилу Grove PORT.A около DinMeter.</li>
<li>Порядок контактов COM, NO, NC на клеммнике реле проверить по маркировке на плате.</li>
</ul></section>
'''

STYLE_ADD = (".tag{fill:#fff;stroke:#111;stroke-width:1.5}.tagt{font:700 11px var(--f);text-anchor:middle;fill:#111}"
             ".tagi{display:inline-block;width:20px;height:20px;border-radius:50%;border:1.5px solid currentColor;text-align:center;font:700 11px/18px var(--f)}"
             ".hole{fill:var(--bg);stroke:var(--accent);stroke-width:1.5}.lid{fill:#4d535a;stroke:#222}.plug{fill:#f7f7f7;stroke:#888}.green{fill:#33a64d;stroke:#1d6b31}.coil{fill:#f98c1a;stroke:#a44}"
             ".cutoff{fill:#e0262699;stroke:#ff3b3b;stroke-width:1.5}.cutline{stroke:#ff3b3b;stroke-width:2;stroke-dasharray:7 4}"
             ".card table{margin-top:12px}</style>")
ARROW = '<defs><marker id="a" viewBox="0 0 10 10" refX="5" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0 2 L5 5 L0 8" fill="none" stroke="currentColor"/></marker></defs>'


def dim(x1, y1, x2, y2, text, off=0, vertical=False):
    """Dimension line with arrows and the value in the middle (svg px)."""
    if vertical:
        tx, ty = x1 + (off + 4 if off > 0 else off - 4), (y1 + y2) / 2 + 4
        return (f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" class="dim" marker-start="url(#a)" marker-end="url(#a)"/>'
                f'<text x="{tx:.1f}" y="{ty:.1f}" class="dt" text-anchor="{"end" if off < 0 else "start"}">{text}</text>')
    return (f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" class="dim" marker-start="url(#a)" marker-end="url(#a)"/>'
            f'<text x="{(x1 + x2) / 2:.1f}" y="{y1 - 6 + off:.1f}" class="dt">{text}</text>')


def end_view(kind):
    """Upper end wall (DinMeter window) or lower end wall (wire slot), seen from outside. x right, z up."""
    k = 5.0
    ox, oy = 110, 40 + (L.HB + L.HL) * k          # svg position of the box corner at x = -W/2, z = 0
    X = lambda x: ox + (x + L.W / 2) * k
    Z = lambda z: oy - z * k
    xl, xr = X(-L.W / 2), X(L.W / 2)
    o = [f'<svg viewBox="0 0 {int(L.W * k + 230)} {int((L.HB + L.HL) * k + 110)}" style="color:var(--fg);max-width:560px;margin:0 auto">', ARROW,
         f'<rect x="{xl:.1f}" y="{Z(L.HB+L.HL):.1f}" width="{L.W*k:.1f}" height="{(L.HB+L.HL)*k:.1f}" rx="12" class="part"/>',
         f'<line x1="{xl:.1f}" y1="{Z(L.HB):.1f}" x2="{xr:.1f}" y2="{Z(L.HB):.1f}" class="thin"/>',
         f'<text x="{xr+70:.1f}" y="{Z(L.HB+L.HL/2)+4:.1f}" class="note">крышка</text>',
         f'<text x="{xr+70:.1f}" y="{Z(0)+4:.1f}" class="note">низ</text>',
         f'<text x="{xl-60:.1f}" y="{Z(L.HB+L.HL)-8:.1f}" class="note">сторона A</text>']
    if kind == "din":
        cx, cz, w, h = L.DIN_X, L.DIN_Z, L.CUT_W, L.CUT_H
        o.append(f'<rect x="{X(cx-26.5):.1f}" y="{Z(cz+15):.1f}" width="{53*k:.1f}" height="{30*k:.1f}" rx="16" class="ghost"/>')
        o.append(f'<rect x="{X(cx-w/2):.1f}" y="{Z(cz+h/2):.1f}" width="{w*k:.1f}" height="{h*k:.1f}" class="cut"/>')
        o.append(f'<text x="{X(cx):.1f}" y="{Z(cz)+5:.1f}" class="dt" style="fill:var(--accent)">окно {w:.1f} × {h:.1f}</text>')
        o.append(f'<text x="{X(cx):.1f}" y="{Z(cz+15)-6:.1f}" class="note" text-anchor="middle" style="fill:#ddd">пунктир: рамка DinMeter 53 × 30</text>')
        # extension lines to the right, vertical dims outside the box
        for z, e in ((0, 26), (cz - h / 2, 50), (cz + h / 2, 50)):
            o.append(f'<line x1="{X(cx+w/2) if z else xr:.1f}" y1="{Z(z):.1f}" x2="{xr+e:.1f}" y2="{Z(z):.1f}" class="ext"/>')
        o.append(dim(xr + 22, Z(0), xr + 22, Z(cz - h / 2), f"{cz - h/2:.1f}", off=6, vertical=True))
        o.append(dim(xr + 46, Z(cz - h / 2), xr + 46, Z(cz + h / 2), f"{h:.1f}", off=6, vertical=True))
        for x in (-L.W / 2, cx - w / 2, cx + w / 2):
            o.append(f'<line x1="{X(x):.1f}" y1="{Z(cz - h/2) if x != -L.W/2 else Z(0):.1f}" x2="{X(x):.1f}" y2="{Z(0)+44:.1f}" class="ext"/>')
        o.append(dim(xl, Z(0) + 20, X(cx - w / 2), Z(0) + 20, f"{L.W/2 - w/2:.1f}", off=0))
        o.append(dim(X(cx - w / 2), Z(0) + 40, X(cx + w / 2), Z(0) + 40, f"{w:.1f}", off=0))
    else:
        cx, cz, w, h = L.SLOT_X, L.SLOT_Z, L.SLOT_L, L.SLOT_H
        o.append(f'<rect x="{X(cx-w/2):.1f}" y="{Z(cz+h/2):.1f}" width="{w*k:.1f}" height="{h*k:.1f}" rx="{h/2*k:.1f}" class="cut"/>')
        o.append(f'<line x1="{X(cx):.1f}" y1="{Z(cz):.1f}" x2="{X(cx):.1f}" y2="{Z(0)+44:.1f}" class="ext"/>')
        for z, e in ((cz, 26), (cz - h / 2, 50), (cz + h / 2, 50)):
            o.append(f'<line x1="{X(cx + w/2 if z != cz else cx):.1f}" y1="{Z(z):.1f}" x2="{xr+e:.1f}" y2="{Z(z):.1f}" class="ext"/>')
        o.append(f'<line x1="{xr:.1f}" y1="{Z(0):.1f}" x2="{xr+26:.1f}" y2="{Z(0):.1f}" class="ext"/>')
        o.append(dim(xr + 22, Z(0), xr + 22, Z(cz), f"{cz:.1f}", off=6, vertical=True))
        o.append(dim(xr + 46, Z(cz - h / 2), xr + 46, Z(cz + h / 2), f"{h:.0f}", off=6, vertical=True))
        o.append(f'<line x1="{xl:.1f}" y1="{Z(0):.1f}" x2="{xl:.1f}" y2="{Z(0)+44:.1f}" class="ext"/>')
        o.append(dim(xl, Z(0) + 20, X(cx), Z(0) + 20, f"{L.W/2 + cx:.1f} до центра", off=0))
        o.append(dim(X(cx - w / 2), Z(cz + h / 2) - 12, X(cx + w / 2), Z(cz + h / 2) - 12, f"{w:.0f}", off=0).replace('class="dt"', 'class="dt" style="fill:#ddd"'))
    o.append("</svg>")
    return "".join(o)


def boss_detail():
    """Two drawings side by side: top view of the upper end with the bosses, and a section through one boss."""
    k = 6.0
    cy = L.BOSS_Y / 2
    depth = 24.0                                   # how much of the box length the top view shows
    # --- top view ---
    ox, oy = 30 + L.W / 2 * k, 30
    T = lambda x, y: (ox + x * k, oy + (L.L / 2 - y) * k)
    o = [f'<svg viewBox="0 0 {int(L.W * k + 60)} {int(depth * k + 110)}" style="color:var(--fg)">', ARROW]
    a = T(-L.W / 2, L.L / 2); o.append(f'<rect x="{a[0]:.1f}" y="{a[1]:.1f}" width="{L.W*k:.1f}" height="{depth*k:.1f}" class="part"/>')
    a = T(-L.IN_W / 2, L.IN_L / 2); o.append(f'<rect x="{a[0]:.1f}" y="{a[1]:.1f}" width="{L.IN_W*k:.1f}" height="{(depth-(L.L-L.IN_L)/2)*k:.1f}" class="inner"/>')
    for sx in (-1, 1):
        c = T(sx * L.BOSS_X / 2, cy)
        q = T(sx * L.BOSS_X / 2 + sx * 4 - 3, cy + 7)
        o.append(f'<rect x="{q[0]:.1f}" y="{q[1]:.1f}" width="{6*k:.1f}" height="{6*k:.1f}" class="cutoff"/>')
        o.append(f'<circle cx="{c[0]:.1f}" cy="{c[1]:.1f}" r="{L.BOSS_D/2*k:.1f}" class="cutoff"/><circle cx="{c[0]:.1f}" cy="{c[1]:.1f}" r="{L.BOSS_HOLE/2*k:.1f}" fill="#222"/>')
    a = T(-22.8, L.L / 2 - 2); o.append(f'<rect x="{a[0]:.1f}" y="{a[1]:.1f}" width="{45.6*k:.1f}" height="{18.4*k:.1f}" class="ghost"/>')
    a = T(-24.5, L.L / 2 - 2); o.append(f'<rect x="{a[0]:.1f}" y="{a[1]:.1f}" width="{49*k:.1f}" height="{8*k:.1f}" class="ghost" style="stroke:#f6c445"/>')
    t = T(0, L.L / 2 - 2 - 12); o.append(f'<text x="{t[0]:.1f}" y="{t[1]:.1f}" class="note" text-anchor="middle" style="fill:#ddd">корпус DinMeter 45.6</text>')
    t = T(0, L.L / 2 - 2 - 4.5); o.append(f'<text x="{t[0]:.1f}" y="{t[1]:.1f}" class="note" text-anchor="middle" style="fill:#f6c445">задняя рамка 49</text>')
    l, r = T(-L.BOSS_X / 2, cy), T(L.BOSS_X / 2, cy)
    yb = oy + depth * k + 26
    for x in (l[0], r[0]):
        o.append(f'<line x1="{x:.1f}" y1="{l[1]:.1f}" x2="{x:.1f}" y2="{yb+6:.1f}" class="ext"/>')
    o.append(dim(l[0], yb, r[0], yb, f"{L.BOSS_X:.2f} между центрами", off=0))
    o.append(f'<text x="{ox:.1f}" y="{yb + 34:.1f}" class="note" text-anchor="middle">вид сверху на верхний конец коробки, крышка снята</text>')
    o.append("</svg>")
    top_svg = "".join(o)
    # --- section through one boss: horizontal = distance from the outer end face, vertical = height ---
    kk = 7.0
    span = 14.0                                    # mm shown along the box from the outer end face
    x0, z0 = 70, 30 + L.HB * kk
    Xs = lambda d: x0 + d * kk
    Zs = lambda z: z0 - z * kk
    wall = (L.L - L.IN_L) / 2
    b0, b1 = L.L / 2 - cy - L.BOSS_D / 2, L.L / 2 - cy + L.BOSS_D / 2
    o = [f'<svg viewBox="0 0 {int(span * kk + 230)} {int(L.HB * kk + 80)}" style="color:var(--fg);max-width:380px">', ARROW,
         f'<rect x="{Xs(0):.1f}" y="{Zs(L.HB):.1f}" width="{wall*kk:.1f}" height="{L.HB*kk:.1f}" class="part"/>',
         f'<rect x="{Xs(0):.1f}" y="{Zs(L.FLOOR):.1f}" width="{span*kk:.1f}" height="{L.FLOOR*kk:.1f}" class="part"/>',
         f'<rect x="{Xs(wall):.1f}" y="{Zs(L.BOSS_CUT_Z):.1f}" width="{(b1-wall)*kk:.1f}" height="{(L.BOSS_CUT_Z-L.FLOOR)*kk:.1f}" class="cutoff"/>',
         f'<rect x="{Xs(wall):.1f}" y="{Zs(L.HB):.1f}" width="{(b1-wall)*kk:.1f}" height="{(L.HB-L.BOSS_CUT_Z)*kk:.1f}" class="boss"/>',
         f'<rect x="{Xs(L.L/2-cy-1.6):.1f}" y="{Zs(L.HB):.1f}" width="{3.2*kk:.1f}" height="{5*kk:.1f}" fill="#c9a640"/>',
         f'<line x1="{Xs(-2):.1f}" y1="{Zs(L.BOSS_CUT_Z):.1f}" x2="{Xs(span):.1f}" y2="{Zs(L.BOSS_CUT_Z):.1f}" class="cutline"/>',
         f'<text x="{Xs((wall+b1)/2):.1f}" y="{Zs((L.BOSS_CUT_Z+L.FLOOR)/2):.1f}" class="dt" style="fill:#fff">срезать</text>',
         f'<text x="{Xs(0)-8:.1f}" y="{Zs(L.HB)-8:.1f}" class="note">торец</text><text x="{Xs(L.L/2-cy):.1f}" y="{Zs(L.HB)-8:.1f}" class="note" text-anchor="middle">вставка M3</text>']
    o.append(dim(Xs(span) + 30, Zs(L.HB), Xs(span) + 30, Zs(L.BOSS_CUT_Z), f"{L.HB - L.BOSS_CUT_Z:.1f} оставить", off=4, vertical=True))
    o.append(dim(Xs(span) + 30, Zs(L.BOSS_CUT_Z), Xs(span) + 30, Zs(L.FLOOR), f"{L.BOSS_CUT_Z - L.FLOOR:.1f} срезать", off=4, vertical=True))
    o.append(f'<line x1="{Xs(span):.1f}" y1="{Zs(L.HB):.1f}" x2="{Xs(span)+40:.1f}" y2="{Zs(L.HB):.1f}" class="ext"/>')
    o.append(f'<line x1="{Xs(0)-40:.1f}" y1="{Zs(0):.1f}" x2="{Xs(span):.1f}" y2="{Zs(0):.1f}" class="ext"/>')
    o.append(dim(Xs(0) - 30, Zs(0), Xs(0) - 30, Zs(L.BOSS_CUT_Z), f"{L.BOSS_CUT_Z:.1f}", off=-6, vertical=True))
    o.append(f'<text x="{Xs(span/2):.1f}" y="{Zs(0)+30:.1f}" class="note" text-anchor="middle">разрез по бобышке</text>')
    o.append("</svg>")
    return f'<div style="display:grid;grid-template-columns:1.4fr 1fr;gap:12px;align-items:end;justify-items:center">{top_svg}{"".join(o)}</div>'


if L.TAG == "1591s":
    p = "hammond_1591s.html"
    h = open(p).read()
    h = re.sub(r'<section class="card"( id="cables")?><h2>3\. .*?</section>\n?', lambda m: section, h, count=1, flags=re.S)
    if ".tag{" not in h:
        h = h.replace("</style>", STYLE_ADD)
else:
    p = "hammond_1591c.html"
    h = open("hammond_1591s.html").read()
    head = h[:h.index("<main>")]
    if ".cutoff{" not in head:
        head = head.replace(".card table{margin-top:12px}</style>", STYLE_ADD)
    script = h[h.index("</main>"):]
    script = script.replace("hammond_1591s_", "hammond_1591c_").replace("wrangler_air_1591s", "wrangler_air_1591c")
    script = script.replace("if(o.isMesh){o.material=o.material.clone();o.material.side=THREE.DoubleSide}",
                            "if(o.isMesh){o.material=o.material.clone();o.material.side=THREE.DoubleSide;if(o.parent&&o.parent.name==='boss_part_to_cut_off'||o.name==='boss_part_to_cut_off'){o.material.color.set(0xff2020)}}")
    head = head.replace("<title>Hammond 1591S Cut Plan</title>", "<title>Hammond 1591C Cut Plan</title>")
    section = section.replace("<h2>3. Раскладка и кабели внутри</h2>", "<h2>5. Раскладка и кабели внутри</h2>").replace("до крышки остаётся не меньше 6 мм", "до крышки остаётся около 3 мм")
    section = section.replace("Врезки +12 В и массы в нижней свободной полосе между бобышками", "Врезки +12 В и массы в нижней свободной полосе под реле и ADC")
    body = f'''<main>
<h1>Wrangler Air в коробке Hammond 1591C</h1>
<p class="lead">Коробка <a href="https://www.distrelec.ch/en/multipurpose-frabs-enclosure-1591-65x120x36mm-black-abs-ip54-hammond-1591cbk/p/15019435" target="_blank">Hammond 1591CBK</a>, 120 × 65 × 40 мм, чёрный ABS, IP54. Размеры по чертежу Hammond (REV 06.16.2023). Стоит вертикально, крышкой к стенке тоннеля. DinMeter в верхнем торце 65 × 40, рамка 53 × 30 почти во весь торец. Две бобышки крышки у этого торца подрезаются снизу (красным), чтобы прошёл корпус DinMeter. Модель проверена: после подрезки пересечений нет.</p>
<div>
<section class="card"><h2>1. Окно под DinMeter в верхнем торце</h2>{end_view("din")}
<ul><li>Окно <b>{L.CUT_W:.1f} × {L.CUT_H:.1f} мм</b>, по центру ширины ({L.W/2 - L.CUT_W/2:.2f} мм от каждого края), нижний край окна в <b>{L.DIN_Z - L.CUT_H/2:.1f} мм</b> от низа коробки (центр {L.DIN_Z:.1f} мм).</li>
<li>DinMeter стоит ниже центра торца: его задняя рамка должна пройти под оставшимися верхушками бобышек. Рамка DinMeter снизу почти доходит до края коробки (около 0.6 мм), сверху остаётся около 9 мм.</li>
<li>Если на внутренней стороне торца есть рёбра, сточить их под рамкой DinMeter.</li></ul></section>
</div>
<section class="card"><h2>2. Подрезка двух бобышек у верхнего торца</h2>{boss_detail()}
<ul><li>Обе бобышки у торца с DinMeter: срезать всё <b>ниже {L.HB - L.BOSS_CUT_Z:.1f} мм от верхнего обреза коробки</b>, вместе с ребром, которое идёт от бобышки в угол. Сверху остаётся пенёк {L.HB - L.BOSS_CUT_Z:.1f} мм с латунной вставкой M3, крышка держится на всех 4 винтах.</li>
<li>Чем: бокорезы под ноль, потом бор или стамеска. Сначала вкрутить винт во вставку, чтобы при резке её не выдернуло.</li>
<li>Длина латунной вставки на чертеже Hammond не указана. Если окажется длиннее {L.HB - L.BOSS_CUT_Z:.0f} мм, подрезать бобышку только до неё, а DinMeter опустить ещё на миллиметр.</li></ul></section>
<section class="card"><h2>3. Паз под провода в нижнем торце</h2>{end_view("slot")}
<ul><li>Паз <b>{L.SLOT_L:.0f} × {L.SLOT_H:.0f} мм</b> со скруглёнными концами, центр в <b>{L.W/2 + L.SLOT_X:.1f} мм</b> от края стороны A и в <b>{L.SLOT_Z:.1f} мм</b> от низа коробки.</li>
<li>Через паз: +12 В, масса, провод на K2, три провода датчика в термоусадке.</li></ul></section>
<section class="card"><h2>4. Питание и пайки</h2>{power_svg()}
<ul><li><b>S1</b>, +12 В: красный 1.5 от AUX1 спаять с красным 1.5 на COM реле и оранжевым 0.25 на «+» DinMeter.</li>
<li><b>S2</b>, масса: чёрный 1.5 от кузова спаять с синим 0.25 на «−» DinMeter. Это только переход с толстого провода на тонкий под клемму DinMeter.</li>
<li><b>S3</b>, +5 В датчика: зачистить 5 мм красной жилы кабеля Grove PORT.A посередине, не разрезая, намотать на неё красный провод датчика и пропаять.</li>
<li>Каждую пайку закрыть термоусадкой, S1 и S2 уложить в нижней свободной полосе коробки.</li>
<li>Реле и ADC отдельного питания не требуют: 5 В и массу они получают от DinMeter по кабелям Grove.</li></ul></section>
{section}
<section class="card"><h2>6. 3D</h2><div id="stage"></div><p class="lead" style="margin-top:8px">Вращать мышью или пальцем, колесо или щипок для масштаба. Крышка показана поднятой, коробка полупрозрачная, срезаемые части бобышек красные.</p></section>
'''
    h = head + body + script
glb = base64.b64encode(open(f"wrangler_air_{L.TAG}.glb", "rb").read()).decode()
for f in (p, f"hammond_{L.TAG}_3d.html"):
    t = h if f == p else open(f).read()
    t = re.sub(r'const GLB="[^"]*"', lambda m: f'const GLB="{glb}"', t, count=1)
    open(f, "w").write(t)
print("ok", p, len(svg), W_SVG, H_SVG)
