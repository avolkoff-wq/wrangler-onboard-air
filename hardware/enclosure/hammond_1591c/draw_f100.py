"""Drawing of the AliExpress F-100x68x50 box (black base 36 + clear lid 14) with DinMeter, relay and ADC.
Sizes come from layout_box.py (WA_BOX=f100). Writes box_f100.html (one SVG, three views).
Run:  WA_BOX=f100 python3 draw_f100.py
"""
import os
os.environ.setdefault("WA_BOX", "f100")
import layout_box as L

k = 5.0
CSS = """
svg{font-family:Inter,system-ui,sans-serif;font-size:13px}
.part{fill:#2a2c30;stroke:#111;stroke-width:1.5}
.inner{fill:#f4f5f6;stroke:#111;stroke-width:1}
.lid{fill:#cfe7f7;fill-opacity:.55;stroke:#5a8fb5;stroke-width:1.2}
.din{fill:#f3a37f;stroke:#b4532a;stroke-width:1}
.bezel{fill:#e2612a;stroke:#9c3c12;stroke-width:1}
.mod{fill:#fff;stroke:#555;stroke-width:1}
.term{fill:#f97326;stroke:#a6460e;stroke-width:1}
.plug{fill:#eee;stroke:#333;stroke-width:1}
.post{fill:#55585e;stroke:#111;stroke-width:1}
.ins{fill:#c9a04a;stroke:#7a5a18;stroke-width:.8}
.pcb{fill:none;stroke:#888;stroke-width:1;stroke-dasharray:3 2}
.hid{fill:none;stroke:#888;stroke-width:1;stroke-dasharray:4 3}
.win{fill:#fff;stroke:#c00;stroke-width:1.6}
.dim{stroke:#c00;stroke-width:.9;fill:none}
.dt{fill:#c00;font-size:12px}
.lab{fill:#111;font-size:12px}
.labw{fill:#fff;font-size:12px}
.ttl{fill:#111;font-size:15px;font-weight:600}
.note{fill:#444;font-size:12px}
"""


def R(x0, y0, x1, y1, cls, rx=0):
    x, y = min(x0, x1), min(y0, y1)
    return f'<rect x="{x:.1f}" y="{y:.1f}" width="{abs(x1-x0):.1f}" height="{abs(y1-y0):.1f}" rx="{rx}" class="{cls}"/>'


def C(x, y, r, cls):
    return f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}" class="{cls}"/>'


def T(x, y, s, cls="lab", a="middle"):
    return f'<text x="{x:.1f}" y="{y:.1f}" class="{cls}" text-anchor="{a}">{s}</text>'


def dim_h(x1, x2, y, s, above=True):
    t = y - 5 if above else y + 14
    return (f'<line x1="{x1:.1f}" y1="{y:.1f}" x2="{x2:.1f}" y2="{y:.1f}" class="dim" marker-start="url(#a)" marker-end="url(#a)"/>'
            + T((x1 + x2) / 2, t, s, "dt"))


def dim_v(x, y1, y2, s, left=True):
    xt = x - 6 if left else x + 6
    return (f'<line x1="{x:.1f}" y1="{y1:.1f}" x2="{x:.1f}" y2="{y2:.1f}" class="dim" marker-start="url(#a)" marker-end="url(#a)"/>'
            + T(xt, (y1 + y2) / 2 + 4, s, "dt", "end" if left else "start"))


o = []
# ---------------- top view, lid off (x right, y up: DinMeter end at the top) ----------------
OX, OY = 110 + L.W / 2 * k, 90 + (L.L / 2 + 3) * k
P = lambda x, y: (OX + x * k, OY - y * k)
o.append(T(OX, 40, "Вид сверху, крышка снята", "ttl"))
a, b = P(-L.W / 2, L.L / 2), P(L.W / 2, -L.L / 2); o.append(R(*a, *b, "part", 6 * k / 5))
a, b = P(-L.IN_W / 2, L.IN_L / 2), P(L.IN_W / 2, -L.IN_L / 2); o.append(R(*a, *b, "inner", 6))
for sx in (-1, 1):
    for sy in (-1, 1):
        c = P(sx * L.BOSS_X / 2, sy * L.BOSS_Y / 2)
        q = P(sx * L.BOSS_X / 2 + sx * 4, sy * L.BOSS_Y / 2 + sy * 4)
        o.append(R(c[0], c[1], q[0], q[1], "post")); o.append(C(*c, L.BOSS_D / 2 * k, "post")); o.append(C(*c, 1.6 * k / 2 * 1.4, "ins"))
        o.append(C(*P(sx * L.PCB_BOSS[0] / 2, sy * L.PCB_BOSS[1] / 2), L.PCB_BOSS[2] / 2 * k, "pcb"))
# DinMeter
a, b = P(-26.5, L.TOP_Y + 2.5), P(26.5, L.TOP_Y); o.append(R(*a, *b, "bezel"))
a, b = P(-22.8, L.TOP_Y), P(22.8, L.DB); o.append(R(*a, *b, "din")); o.append(T(OX, P(0, L.DB + 11)[1], "DinMeter"))
for xx in (-11.7, -18.0):
    a, b = P(xx - 2.3, L.DB), P(xx + 2.3, L.DB - 8); o.append(R(*a, *b, "plug"))
a, b = P(-15.1 - 4.4, L.DB), P(-15.1 + 4.4, L.DB - 10); o.append(R(*a, *b, "term"))
o.append(T(P(-11.7, 0)[0] + 18, P(0, L.DB - 5)[1], "A", "lab", "start")); o.append(T(P(-18, 0)[0] - 8, P(0, L.DB - 5)[1], "B", "lab", "end"))
# relay and ADC
for xc, y0, ln, name, ty in ((L.RXC, L.RY0, 48, "Relay", 10), (L.AXC, L.AY0, 32, "ADC", 8.5)):
    a, b = P(xc - 12, y0 + ln), P(xc + 12, y0); o.append(R(*a, *b, "mod", 3))
    a, b = P(xc - 4.95, y0 + ln + 6.5), P(xc + 4.95, y0 + ln); o.append(R(*a, *b, "plug"))
    a, b = P(xc - 6, y0 + ty), P(xc + 6, y0); o.append(R(*a, *b, "term"))
    o.append(T(P(xc, 0)[0], P(0, y0 + ln / 2)[1], name))
# wire slot
a, b = P(L.SLOT_X - 7, -L.Y_IN), P(L.SLOT_X + 7, -L.TOP_Y); o.append(R(*a, *b, "win"))
o.append(T(P(L.SLOT_X, 0)[0], P(0, -L.TOP_Y)[1] + 18, "паз под провода 14×8", "note"))
# dims
o.append(dim_h(*[P(x, 0)[0] for x in (-L.W / 2, L.W / 2)], P(0, L.TOP_Y)[1] - 28, "68"))
o.append(dim_h(*[P(x, 0)[0] for x in (-L.IN_W / 2, L.IN_W / 2)], P(0, -L.TOP_Y)[1] + 40, "≈63 внутри", above=False))
o.append(dim_v(P(-L.W / 2, 0)[0] - 30, *[P(0, y)[1] for y in (L.TOP_Y, -L.TOP_Y)], "100"))
o.append(dim_v(P(L.W / 2, 0)[0] + 22, *[P(0, y)[1] for y in (L.Y_IN, -L.Y_IN)], "≈95", left=False))
o.append(dim_h(P(22.8, 0)[0], P(L.BOSS_X / 2 - L.BOSS_D / 2, 0)[0], P(0, L.BOSS_Y / 2)[1], ""))
o.append(T(P(L.BOSS_X / 2, 0)[0], P(0, L.DB - 6)[1], "зазор 2.1", "dt"))
o.append(dim_v(P(L.RXC + 7, 0)[0], P(0, L.RY0)[1], P(0, -L.Y_IN)[1], "13.5", left=False))

# ---------------- end view from outside, DinMeter end ----------------
EX, EZ = 200 + L.W * k + 170, 90 + (L.HB + L.HL) * k
E = lambda x, z: (EX + (x + L.W / 2) * k, EZ - z * k)
o.append(T(E(0, 0)[0], 40, "Торец с DinMeter, вид снаружи", "ttl"))
a, b = E(-L.W / 2, L.HB), E(L.W / 2, 0); o.append(R(*a, *b, "part", 4))
a, b = E(-L.W / 2, L.HB + L.HL), E(L.W / 2, L.HB); o.append(R(*a, *b, "lid", 4))
o.append(T(E(0, 0)[0], E(0, L.HB + L.HL / 2)[1] + 4, "прозрачная крышка 14", "lab"))
for sx in (-1, 1):
    a, b = E(sx * (L.BOSS_X / 2 - L.BOSS_D / 2), L.HB), E(sx * (L.BOSS_X / 2 + L.BOSS_D / 2), L.FLOOR); o.append(R(*a, *b, "hid"))
a, b = E(-26.5, L.DIN_Z + 15), E(26.5, L.DIN_Z - 15); o.append(R(*a, *b, "hid", 4 * k))
a, b = E(-L.CUT_W / 2, L.DIN_Z + L.CUT_H / 2), E(L.CUT_W / 2, L.DIN_Z - L.CUT_H / 2); o.append(R(*a, *b, "win"))
o.append(T(E(0, 0)[0], E(0, L.DIN_Z)[1] + 4, "окно 46 × 22.8", "dt"))
o.append(T(E(0, 0)[0], E(0, L.FLOOR)[1] + 2, "чёрное основание 36", "labw"))
o.append(dim_v(E(-L.W / 2, 0)[0] - 14, E(0, L.DIN_Z - L.CUT_H / 2)[1], E(0, 0)[1], f"{L.DIN_Z - L.CUT_H/2:.1f}"))
o.append(dim_v(E(-L.W / 2, 0)[0] - 14, E(0, L.HB)[1], E(0, L.DIN_Z + L.CUT_H / 2)[1], f"{L.HB - L.DIN_Z - L.CUT_H/2:.1f}"))
o.append(dim_v(E(L.W / 2, 0)[0] + 14, E(0, L.HB)[1], E(0, 0)[1], "36", left=False))
o.append(dim_v(E(L.W / 2, 0)[0] + 14, E(0, L.HB + L.HL)[1], E(0, L.HB)[1], "14", left=False))
o.append(dim_h(E(-L.W / 2, 0)[0], E(-L.CUT_W / 2, 0)[0], E(0, 0)[1] + 22, f"{L.W/2 - L.CUT_W/2:.0f}", above=False))
o.append(dim_h(E(-L.CUT_W / 2, 0)[0], E(L.CUT_W / 2, 0)[0], E(0, 0)[1] + 22, "46", above=False))
o.append(T(E(0, 0)[0], E(0, 0)[1] + 58, "пунктир: стойки винтов крышки за стенкой и рамка DinMeter 53×30", "note"))

# ---------------- side section (y right = towards DinMeter, z up) ----------------
SX0, SZ = EX - 40, EZ + 150 + (L.HB + L.HL) * k
S = lambda y, z: (SX0 + (y + L.L / 2) * k, SZ - z * k)
o.append(T(S(0, 0)[0], SZ - (L.HB + L.HL) * k - 40, "Разрез вдоль коробки", "ttl"))
a, b = S(-L.L / 2, L.HB), S(L.L / 2, 0); o.append(R(*a, *b, "part", 4))
a, b = S(-L.Y_IN, L.HB), S(L.Y_IN, L.FLOOR); o.append(R(*a, *b, "inner"))
a, b = S(-L.L / 2, L.HB + L.HL), S(L.L / 2, L.HB); o.append(R(*a, *b, "lid", 4))
a, b = S(-L.Y_IN, L.HB + L.HL - 2.5), S(L.Y_IN, L.HB); o.append(R(*a, *b, "inner"))
a, b = S(L.TOP_Y, L.DIN_Z + 15), S(L.TOP_Y + 2.5, L.DIN_Z - 15); o.append(R(*a, *b, "bezel"))
a, b = S(L.DB, L.DIN_Z + 11.1), S(L.TOP_Y, L.DIN_Z - 11.1); o.append(R(*a, *b, "din"))
o.append(T(S((L.DB + L.TOP_Y) / 2, 0)[0], S(0, L.DIN_Z)[1] + 4, "DinMeter"))
a, b = S(L.DB - 8, L.DIN_Z + 2), S(L.DB, L.DIN_Z - 8); o.append(R(*a, *b, "plug"))
a, b = S(L.DB - 10, L.DIN_Z + 11.1), S(L.DB, L.DIN_Z + 4.1); o.append(R(*a, *b, "term"))
F = L.FLOOR
a, b = S(L.RY0, F + 8), S(L.RY0 + 48, F); o.append(R(*a, *b, "mod", 2))
a, b = S(L.RY0, F + 20), S(L.RY0 + 10, F + 8); o.append(R(*a, *b, "term"))
a, b = S(L.RY0 + 48, F + 6.3), S(L.RY0 + 54.5, F + 1.7); o.append(R(*a, *b, "plug"))
o.append(T(S(L.RY0 + 30, 0)[0], S(0, F + 4)[1] + 4, "Relay (ADC рядом)"))
a, b = S(-L.TOP_Y, L.SLOT_Z + 4), S(-L.Y_IN, L.SLOT_Z - 4); o.append(R(*a, *b, "win"))
o.append(dim_v(S(L.TOP_Y, 0)[0] + 30, S(0, L.HB + L.HL - 2.5)[1], S(0, F)[1], "≈45", left=False))
o.append(dim_h(S(L.DB, 0)[0], S(L.TOP_Y, 0)[0], S(0, L.HB + L.HL)[1] - 14, "20.4"))
o.append(T(S(0, 0)[0], SZ + 30, "Над модулями ≈ 25 мм до крышки: место под петли Grove", "note"))

W_SVG = int(EX + L.W * k + 190)
H_SVG = int(SZ + 60)
svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W_SVG} {H_SVG}" width="{W_SVG}" height="{H_SVG}">'
       f'<style>{CSS}</style><defs><marker id="a" viewBox="0 0 10 10" refX="5" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">'
       f'<path d="M0 0 L10 5 L0 10 z" fill="#c00"/></marker></defs><rect width="100%" height="100%" fill="#fff"/>' + "".join(o) + "</svg>")
open("box_f100.svg", "w").write(svg)
open("box_f100.html", "w").write('<!doctype html><meta charset="utf-8"><title>Box F-100x68x50</title><body style="margin:0;background:#fff">' + svg)
print(W_SVG, H_SVG)
