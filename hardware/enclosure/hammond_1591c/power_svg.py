"""Power and signal schematic for Wrangler Air: where each wire goes and the three soldered splices.
Imported by draw_box.py (section "Питание и пайки"). Plain SVG, sizes in px."""

RED, BLK, ORG, BLU, YEL, WHT, GRY = "#d42020", "#202020", "#ff8c0d", "#2766e6", "#e8b90f", "#bbbbbb", "#8a929c"


def line(pts, col, w=3, dash=""):
    q = " ".join(f"{x},{y}" for x, y in pts)
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return f'<polyline points="{q}" fill="none" stroke="{col}" stroke-width="{w}" stroke-linejoin="round"{d}/>'


def block(x, y, w, h, title, sub="", fill="#eef1f4"):
    s = f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="8" fill="{fill}" stroke="#7a828c"/>'
    s += f'<text x="{x + w / 2}" y="{y + 20}" class="bt">{title}</text>'
    if sub:
        s += f'<text x="{x + w / 2}" y="{y + 37}" class="bs">{sub}</text>'
    return s


def pin(x, y, label, anchor="start", dx=6):
    return f'<circle cx="{x}" cy="{y}" r="4" fill="#fff" stroke="#333"/><text x="{x + (dx if anchor == "start" else -dx)}" y="{y + 4}" class="pl" text-anchor="{anchor}">{label}</text>'


def splice(x, y, name, dx=10, dy=-10):
    return (f'<circle cx="{x}" cy="{y}" r="7" fill="#111" stroke="#fff" stroke-width="2"/>'
            f'<text x="{x + dx}" y="{y + dy}" class="sp">{name}</text>')


def diode(x, y, direction):
    """Diode symbol centred at x, y. direction: 'left' or 'down' = the way current flows (towards the bar)."""
    if direction == "left":
        return (f'<polygon points="{x + 9},{y - 8} {x + 9},{y + 8} {x - 5},{y}" fill="#fff" stroke="#111" stroke-width="2"/>'
                f'<line x1="{x - 6}" y1="{y - 9}" x2="{x - 6}" y2="{y + 9}" stroke="#111" stroke-width="3"/>')
    if direction == "up":
        return (f'<polygon points="{x - 8},{y + 9} {x + 8},{y + 9} {x},{y - 5}" fill="#fff" stroke="#111" stroke-width="2"/>'
                f'<line x1="{x - 9}" y1="{y - 6}" x2="{x + 9}" y2="{y - 6}" stroke="#111" stroke-width="3"/>')
    return (f'<polygon points="{x - 8},{y - 9} {x + 8},{y - 9} {x},{y + 5}" fill="#fff" stroke="#111" stroke-width="2"/>'
            f'<line x1="{x - 9}" y1="{y + 6}" x2="{x + 9}" y2="{y + 6}" stroke="#111" stroke-width="3"/>')


def gnd(x, y):
    return (f'<line x1="{x}" y1="{y}" x2="{x}" y2="{y + 10}" stroke="#111" stroke-width="2"/>'
            f'<line x1="{x - 12}" y1="{y + 10}" x2="{x + 12}" y2="{y + 10}" stroke="#111" stroke-width="2"/>'
            f'<line x1="{x - 7}" y1="{y + 15}" x2="{x + 7}" y2="{y + 15}" stroke="#111" stroke-width="2"/>'
            f'<line x1="{x - 3}" y1="{y + 20}" x2="{x + 3}" y2="{y + 20}" stroke="#111" stroke-width="2"/>')


def grove(pts, colors, gap=4.5):
    """Four-wire Grove ribbon along an L-shaped path (vertical then horizontal or vice versa), drawn wire by wire."""
    out = []
    for i, c in enumerate(colors):
        off = (i - (len(colors) - 1) / 2) * gap
        q = []
        for k, (x, y) in enumerate(pts):
            nb = [pts[j] for j in (k - 1, k + 1) if 0 <= j < len(pts)]
            vert = any(abs(n[0] - x) < 1e-6 for n in nb)      # a vertical segment touches this point: shift in x
            hor = any(abs(n[1] - y) < 1e-6 for n in nb)       # a horizontal one: shift in y
            q.append((x + (off if vert else 0), y + (off if hor else 0)))
        out.append(line(q, c, 2.5))
    return "".join(out)


def power_svg():
    o = ['<svg viewBox="0 0 1000 640" style="color:var(--fg);max-width:980px;margin:0 auto;background:#fff;border-radius:8px">',
         '<style>.bt{font:700 14px var(--f);text-anchor:middle;fill:#1d232b}.bs{font:12px var(--f);text-anchor:middle;fill:#5d6672}'
         '.pl{font:600 12px var(--f);fill:#1d232b}.sp{font:700 13px var(--f);fill:#c0392b}.wl{font:12px var(--f);fill:#38404a}.hd{font:700 13px var(--f);fill:#5d6672}</style>']
    # zones
    o.append('<rect x="250" y="30" width="520" height="560" rx="12" fill="#f6f7f9" stroke="#5d6672" stroke-width="2" stroke-dasharray="8 6"/>')
    o.append('<text x="510" y="22" class="hd" text-anchor="middle">КОРОБКА 1591C</text>')
    o.append('<text x="125" y="22" class="hd" text-anchor="middle">МАШИНА</text><text x="890" y="22" class="hd" text-anchor="middle">ДАТЧИК ДАВЛЕНИЯ</text>')
    o.append('<rect x="244" y="70" width="12" height="440" rx="4" fill="#fff" stroke="#e3612a" stroke-width="2"/>'
             '<text x="250" y="530" class="wl" text-anchor="middle">паз</text>')
    # car side
    o.append(block(20, 70, 140, 50, "AUX1", "+12 В, выключатель"))
    o.append(block(20, 160, 140, 50, "AUX2", "+12 В, обход"))
    o.append(block(20, 280, 120, 130, "K2", "реле"))
    o.append(pin(140, 320, "86", "end")); o.append(pin(140, 380, "85", "end"))
    o.append('<text x="30" y="400" class="wl">30/87 компрессор</text>')
    o.append(block(20, 470, 140, 50, "Масса", "кузов"))
    # DinMeter, relay, ADC
    o.append(block(560, 60, 190, 150, "DinMeter", "внутри 12 В → 5 В на Grove"))
    o.append(pin(560, 110, "+ 12 В")); o.append(pin(560, 150, "−"))
    o.append(pin(610, 210, "PORT.B", "start", 8)); o.append(pin(700, 210, "PORT.A", "start", 8))
    o.append(block(360, 300, 160, 110, "Mini Relay", "на PORT.B, G2"))
    o.append(pin(360, 360, "COM")); o.append(pin(360, 395, "NO"))
    o.append(block(600, 330, 150, 100, "ADC v1.1", "на PORT.A, I²C"))
    o.append(pin(630, 430, "GND", "end", 8)); o.append(pin(690, 430, "IN", "start", 8))
    o.append(block(820, 380, 150, 160, "Датчик", "питание 5 В"))
    o.append(pin(820, 440, "+5 В", "start", 8)); o.append(pin(820, 480, "SIG", "start", 8)); o.append(pin(820, 520, "GND", "start", 8))
    # Grove cables
    o.append(grove([(610, 214), (610, 265), (450, 265), (450, 300)], (BLK, RED, YEL, WHT)))
    o.append('<text x="470" y="255" class="wl">Grove 10 см</text>')
    o.append(grove([(700, 214), (700, 326)], (WHT, YEL, BLK, RED)))
    o.append('<text x="712" y="300" class="wl">Grove 10 см</text>')
    # +12 V: AUX1 -> S1 -> DinMeter + and relay COM
    S1 = (310, 95)
    o.append(line([(160, 95), S1], RED, 5))
    o.append('<text x="166" y="86" class="wl">1.5 красный</text>')
    o.append(line([S1, (500, 95), (500, 110), (556, 110)], ORG, 3))
    o.append('<text x="400" y="88" class="wl">0.25 оранжевый</text>')
    o.append(line([S1, (310, 360), (356, 360)], RED, 5))
    o.append('<text x="316" y="250" class="wl" transform="rotate(90 316 250)">FLRY 1.5</text>')
    o.append(splice(*S1, "S1", -4, -14))
    # GND: car -> S2 -> DinMeter -
    S2 = (290, 495)
    o.append(line([(160, 495), S2], BLK, 5))
    o.append('<text x="166" y="486" class="wl">1.5 чёрный</text>')
    o.append(line([S2, (290, 555), (540, 555), (540, 150), (556, 150)], BLU, 3))
    o.append('<text x="548" y="545" class="wl" transform="rotate(-90 548 545)">0.25 синий</text>')
    o.append(splice(*S2, "S2", 10, 22))
    # relay NO -> slot -> D1 -> K2 86; AUX2 -> D2 -> K2 86; D3 across the coil; 85 to ground
    o.append(line([(356, 395), (335, 395), (335, 455), (215, 455), (215, 320), (144, 320)], RED, 5))
    o.append('<text x="262" y="472" class="wl">1.5 красный «K2»</text>')
    o.append(diode(215, 410, "up"))
    o.append('<text x="224" y="414" class="pl">D1</text>')
    o.append(line([(160, 185), (190, 185), (190, 320)], RED, 3))
    o.append(diode(190, 260, "down"))
    o.append('<text x="168" y="264" class="pl" text-anchor="end">D2</text>')
    o.append('<circle cx="190" cy="320" r="4" fill="#111"/><circle cx="215" cy="320" r="0" fill="#111"/>')
    o.append(line([(165, 320), (165, 380)], "#111", 2))
    o.append(diode(165, 350, "up"))
    o.append('<text x="172" y="356" class="pl">D3</text><circle cx="165" cy="320" r="3.5" fill="#111"/><circle cx="165" cy="380" r="3.5" fill="#111"/>')
    o.append(line([(144, 380), (165, 380), (165, 420)], BLK, 3)); o.append(gnd(165, 420))
    o.append('<text x="20" y="560" class="wl">D1, D2: диоды MINI в колодках</text><text x="20" y="577" class="wl">D3: 1N4007 на катушке K2</text>')
    # sensor: red into PORT.A Grove red wire (S3), yellow -> IN, black -> ADC GND
    S3 = (700 + 4.5 * 1.5, 255)
    o.append(line([(816, 440), (790, 440), (790, 255), (S3[0] + 4, 255)], RED, 3))
    o.append(splice(*S3, "S3", 10, -10))
    o.append(line([(816, 480), (690, 480), (690, 434)], YEL, 3))
    o.append(line([(816, 520), (630, 520), (630, 434)], BLK, 3))
    o.append('<text x="820" y="565" class="wl">3 × 0.25 в термоусадке,</text><text x="820" y="582" class="wl">в коробку через паз</text>')
    o.append("</svg>")
    return "".join(o)


if __name__ == "__main__":
    open("/tmp/power.html", "w").write('<meta charset="utf-8"><body style="margin:0;--f:Inter,system-ui,sans-serif">' + power_svg())


def fuse(x, y, label):
    return (f'<rect x="{x - 14}" y="{y - 7}" width="28" height="14" rx="3" fill="#fff" stroke="#111" stroke-width="2"/>'
            f'<line x1="{x - 14}" y1="{y}" x2="{x + 14}" y2="{y}" stroke="#111" stroke-width="1.5"/>'
            f'<text x="{x}" y="{y - 13}" class="pl" text-anchor="middle">{label}</text>')


def power_svg_nosolder():
    """Power and signal wiring as built: every car wire enters through the one 14 x 8 mm slot.
    +12 V split after F1 with a solder-seal Y splice, ground straight to the DinMeter, one solder joint for the sensor 5 V."""
    o = ['<svg viewBox="0 0 1000 720" style="color:var(--fg);max-width:1000px;margin:0 auto;background:#fff;border-radius:8px">',
         '<style>.bt{font:700 14px var(--f);text-anchor:middle;fill:#1d232b}.bs{font:12px var(--f);text-anchor:middle;fill:#5d6672}'
         '.pl{font:600 12px var(--f);fill:#1d232b}.sp{font:700 13px var(--f);fill:#1b7f3b}.wl{font:12px var(--f);fill:#38404a}.hd{font:700 13px var(--f);fill:#5d6672}</style>']
    o.append('<rect x="260" y="30" width="540" height="620" rx="12" fill="#f6f7f9" stroke="#5d6672" stroke-width="2" stroke-dasharray="8 6"/>')
    o.append('<text x="530" y="22" class="hd" text-anchor="middle">КОРОБКА 1591C</text><text x="120" y="22" class="hd" text-anchor="middle">МАШИНА</text>')
    # car side, top to bottom: AUX1, ground, AUX2, ARB, sensor
    o.append(block(10, 60, 130, 50, "AUX1", "+12 В, выключатель"))
    o.append(block(10, 130, 130, 50, "Масса", "кузов"))
    o.append(block(10, 210, 130, 50, "AUX2", "+12 В, обход"))
    o.append(block(10, 290, 130, 130, "ARB CKMTA12", ""))
    o.append(pin(140, 330, "фиол.", "end"))
    o.append('<text x="18" y="362" class="wl">→ реле давления</text><text x="18" y="379" class="wl">→ 2 реле 40 А внутри</text><text x="18" y="396" class="wl">→ масса</text>')
    o.append(block(10, 575, 150, 95, "Датчик", "питание 5 В"))
    o.append(pin(160, 600, "+5 В", "end", 8)); o.append(pin(160, 625, "GND", "end", 8)); o.append(pin(160, 650, "SIG", "end", 8))
    # modules
    o.append(block(560, 60, 190, 150, "DinMeter", "внутри 12 В → 5 В на Grove"))
    o.append(pin(560, 110, "+ 12 В")); o.append(pin(560, 150, "−"))
    o.append(pin(610, 210, "PORT.B", "start", 8)); o.append(pin(700, 210, "PORT.A", "start", 8))
    o.append(block(360, 300, 160, 110, "Mini Relay", "на PORT.B, G2"))
    o.append(pin(360, 360, "COM")); o.append(pin(360, 395, "NO"))
    o.append(block(620, 340, 150, 90, "ADC v1.1", "на PORT.A, I²C"))
    o.append(pin(660, 430, "GND", "end", 8)); o.append(pin(730, 430, "IN", "start", 8))
    o.append(grove([(610, 214), (610, 265), (450, 265), (450, 300)], (BLK, RED, YEL, WHT)))
    o.append('<text x="470" y="255" class="wl">Grove 10 см</text>')
    o.append(grove([(700, 214), (700, 340)], (RED, BLK, YEL, WHT)))
    o.append('<text x="712" y="320" class="wl">Grove 10 см</text>')
    # every car wire passes the one slot, top to bottom in this order
    Y = {"din": 536, "com": 542, "k2": 548, "gnd": 554, "s5": 560, "ss": 566, "sg": 572}
    SX = 268
    # AUX1 -> F1 -> Y splice -> two reds
    o.append(line([(140, 85), (200, 85)], RED, 4)); o.append(fuse(168, 85, "F1 3 А"))
    o.append(line([(200, 85), (240, 85), (240, Y["din"]), (SX, Y["din"])], RED, 3.5))
    o.append(line([(200, 85), (234, 85), (234, Y["com"]), (SX, Y["com"])], RED, 3.5))
    o.append('<rect x="196" y="76" width="48" height="18" rx="5" fill="#ffffffdd" stroke="#c0392b" stroke-width="2"/><text x="220" y="70" class="sp" text-anchor="middle">Y</text>')
    # ground
    o.append(line([(140, 155), (222, 155), (222, Y["gnd"]), (SX, Y["gnd"])], BLK, 3))
    # AUX2 -> D2 and relay NO -> D1, both to the ARB purple wire
    # the two diode lines merge in a Y (solder-seal connector) into one wire to the ARB purple wire
    M = (200, 330)
    o.append(line([(140, 235), (190, 235), (190, 300), M], RED, 3)); o.append(diode(190, 270, "down"))
    o.append('<text x="176" y="274" class="pl" text-anchor="end">D2</text>')
    o.append(line([(SX, Y["k2"]), (228, Y["k2"]), (228, 362), M], RED, 3.5)); o.append(diode(228, 460, "up"))
    o.append('<text x="214" y="464" class="pl" text-anchor="end">D1</text>')
    o.append(line([M, (144, 330)], RED, 3.5))
    o.append('<rect x="188" y="321" width="24" height="18" rx="5" fill="#ffffffdd" stroke="#c0392b" stroke-width="2"/><text x="200" y="356" class="sp" text-anchor="middle">Y</text>')
    # sensor, 3 x 0.25 in one heat-shrink sleeve
    o.append(line([(164, 600), (236, 600), (236, Y["s5"]), (SX, Y["s5"])], RED, 2))
    o.append(line([(164, 625), (242, 625), (242, Y["ss"]), (SX, Y["ss"])], BLK, 2))
    o.append(line([(164, 650), (248, 650), (248, Y["sg"]), (SX, Y["sg"])], YEL, 2))
    o.append('<text x="168" y="690" class="wl">3 × 0.25 в термоусадке</text>')
    # the slot, about to scale with the wires: 14 x 8 mm, 7 wires
    o.append('<rect x="252" y="529" width="16" height="50" rx="8" fill="#ffffff55" stroke="#e3612a" stroke-width="2.5"/>')
    o.append('<text x="276" y="600" class="pl">паз 14 × 8 мм</text><text x="276" y="615" class="wl">все провода из машины</text>')
    # inside, fan out
    o.append(line([(SX, Y["din"]), (290, Y["din"]), (290, 110), (556, 110)], RED, 3.5))
    o.append('<text x="300" y="104" class="wl">0.75 красный на «+»</text>')
    o.append(line([(SX, Y["com"]), (310, Y["com"]), (310, 360), (356, 360)], RED, 3.5))
    o.append(line([(SX, Y["k2"]), (330, Y["k2"]), (330, 395), (356, 395)], RED, 3.5))
    o.append('<text x="345" y="480" class="wl" transform="rotate(-90 345 480)">NO → ARB</text>')
    o.append(line([(SX, Y["gnd"]), (545, Y["gnd"]), (545, 150), (556, 150)], BLK, 3))
    o.append('<text x="420" y="548" class="wl">0.25 чёрный в «−»</text>')
    S3x = 700 - 4.5 * 1.5
    o.append(line([(SX, Y["s5"]), (580, Y["s5"]), (580, 285), (S3x, 285)], RED, 2))
    o.append(splice(S3x, 285, "пайка", -58, -8))
    o.append(line([(SX, Y["ss"]), (660, Y["ss"]), (660, 434)], BLK, 2))
    o.append(line([(SX, Y["sg"]), (730, Y["sg"]), (730, 434)], YEL, 2))
    o.append('<text x="300" y="690" class="wl">F1: MINI 3 А в держателе TC-Mini FK1 · D1, D2: диоды MINI в таких же держателях</text>'
             '<text x="300" y="707" class="wl">Y и выводы держателей: термоусадочные соединители с припоем</text>')
    o.append("</svg>")
    return "".join(o)


if __name__ == "__main__" and True:
    open("/tmp/power2.html", "w").write('<meta charset="utf-8"><body style="margin:0;--f:Inter,system-ui,sans-serif">' + power_svg_nosolder())


def power_svg_iec():
    """Circuit diagram in IEC 60617 / IEC 61082 style: black orthogonal wiring on a 10 px grid, signal flow left to right,
    designators (-F1, -K1, -A1 ...), wire colour codes per IEC 60757 (RD, BK, YE, VT) with cross-section, junction dots,
    multi-conductor cables drawn as one line with a stroke and the conductor count, the box entry as terminal strip -X1."""
    W = 2
    o = ['<svg viewBox="0 0 1000 680" style="color:var(--fg);max-width:1000px;margin:0 auto;background:#fff;border-radius:8px">',
         '<style>.d{font:700 12px var(--f);fill:#111}.n{font:12px var(--f);fill:#333}.c{font:11px var(--f);fill:#555}'
         '.w{stroke:#111;stroke-width:2;fill:none;stroke-linejoin:miter}.b{fill:#fff;stroke:#111;stroke-width:1.5}'
         '.box{fill:none;stroke:#111;stroke-width:1.2;stroke-dasharray:10 4 2 4}.t{fill:#fff;stroke:#111;stroke-width:1.5}</style>']

    CC = {"RD": "#d42020", "BK": "#111111", "YE": "#e0a800", "VT": "#7b2cbf", "WH": "#a7adb4", "OG": "#ff7a00", "BU": "#1f5fd6"}

    def L(*pts, c="BK", w=2.5):
        o.append(f'<polyline fill="none" stroke="{CC[c]}" stroke-width="{w}" stroke-linejoin="miter" points="' + " ".join(f"{x},{y}" for x, y in pts) + '"/>')

    def grove4(pts, order=("BK", "RD", "YE", "WH")):
        """Grove cable: four conductors side by side, in their real colours."""
        for i, c in enumerate(order):
            off = (i - 1.5) * 4
            q = []
            for k, (x, y) in enumerate(pts):
                nb = [pts[j] for j in (k - 1, k + 1) if 0 <= j < len(pts)]
                vert = any(n[0] == x for n in nb); hor = any(n[1] == y for n in nb)
                q.append((x + (off if vert else 0), y + (off if hor else 0)))
            L(*q, c=c, w=2)

    def dot(x, y):
        o.append(f'<circle cx="{x}" cy="{y}" r="4" fill="#111"/>')

    def lab(x, y, t, cls="c", anchor="middle"):
        o.append(f'<text x="{x}" y="{y}" class="{cls}" text-anchor="{anchor}">{t}</text>')

    def comp(x, y, w, h, des, name, sub=""):
        o.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" class="b"/>')
        lab(x + 6, y - 6, des, "d", "start")
        lab(x + w / 2, y + 18, name, "d")
        if sub:
            lab(x + w / 2, y + 34, sub, "c")

    def pinl(x, y, t, side="l"):
        o.append(f'<circle cx="{x}" cy="{y}" r="3" class="t"/>')
        lab(x + (8 if side == "l" else -8), y + 4, t, "n", "start" if side == "l" else "end")

    def diode_h(x, y, des):            # current to the left
        o.append(f'<polygon points="{x+8},{y-8} {x+8},{y+8} {x-6},{y}" class="b"/><line x1="{x-7}" y1="{y-9}" x2="{x-7}" y2="{y+9}" stroke="#111" stroke-width="2.5"/>')
        lab(x, y - 14, des, "d")

    def diode_v(x, y, des):            # current downwards
        o.append(f'<polygon points="{x-8},{y-8} {x+8},{y-8} {x},{y+6}" class="b"/><line x1="{x-9}" y1="{y+7}" x2="{x+9}" y2="{y+7}" stroke="#111" stroke-width="2.5"/>')
        lab(x + 14, y + 4, des, "d", "start")

    def fuse_h(x, y, des, val):
        o.append(f'<rect x="{x-16}" y="{y-7}" width="32" height="14" class="b"/><line x1="{x-16}" y1="{y}" x2="{x+16}" y2="{y}" stroke="#111" stroke-width="1.5"/>')
        lab(x, y - 14, f"{des} {val}", "d")

    def ground(x, y):
        o.append(f'<line x1="{x-10}" y1="{y}" x2="{x+10}" y2="{y}" stroke="#111" stroke-width="2"/>'
                 f'<line x1="{x-6}" y1="{y+5}" x2="{x+6}" y2="{y+5}" stroke="#111" stroke-width="2"/>'
                 f'<line x1="{x-2}" y1="{y+10}" x2="{x+2}" y2="{y+10}" stroke="#111" stroke-width="2"/>')

    def cable(x1, y1, x2, y2, n, name, lx, ly):
        L((x1, y1), (x2, y2)) if (x1 == x2 or y1 == y2) else None
        lab(lx, ly, name, "c", "start")

    def slash(x, y, n, vertical):      # multi-conductor marking: one stroke and the conductor count
        if vertical:
            o.append(f'<line x1="{x-6}" y1="{y+6}" x2="{x+6}" y2="{y-6}" stroke="#111" stroke-width="1.5"/>')
            lab(x + 9, y - 4, str(n), "c", "start")
        else:
            o.append(f'<line x1="{x-6}" y1="{y+6}" x2="{x+6}" y2="{y-6}" stroke="#111" stroke-width="1.5"/>')
            lab(x, y - 9, str(n), "c")

    # enclosure and terminal strip -X1 (the 14 x 8 mm slot: every car wire passes here)
    o.append('<rect x="290" y="40" width="550" height="580" class="box"/>')
    lab(296, 34, "-A0  коробка Hammond 1591C", "d", "start")
    T = {1: 100, 2: 140, 3: 180, 4: 300, 5: 480, 6: 520, 7: 560}
    o.append(f'<rect x="270" y="{T[1]-16}" width="40" height="{T[7]-T[1]+32}" class="b"/>')
    lab(264, T[1] - 22, "-X1", "d", "end")
    lab(264, T[7] + 34, "паз 14×8", "c", "end")
    for k, y in T.items():
        o.append(f'<circle cx="270" cy="{y}" r="3" class="t"/><circle cx="310" cy="{y}" r="3" class="t"/>')
        lab(290, y + 4, str(k), "n")

    # outside: supply, ground, compressor, sensor
    # -X1:1 ground, -X1:2 +12 V to the DinMeter, -X1:3 +12 V to the relay COM
    lab(20, T[1] + 4, "Масса", "d", "start"); L((80, T[1]), (267, T[1]), c="BK")
    L((80, T[1]), (80, T[1] + 10), c="BK"); ground(80, T[1] + 10); lab(94, T[1] + 22, "31", "c", "start")
    lab(200, T[1] - 6, "BK 0.25", "c", "start")
    lab(20, T[2] + 4, "AUX1", "d", "start"); L((60, T[2]), (267, T[2]), c="RD")
    fuse_h(110, T[2], "", ""); lab(110, T[2] + 24, "-F1 3 A", "d")
    dot(170, T[2]); L((170, T[2]), (170, T[3]), (267, T[3]), c="RD")
    lab(170, T[2] - 12, "-X2", "c")
    lab(200, T[2] - 6, "RD 0.25", "c", "start"); lab(200, T[3] - 6, "RD 0.25", "c", "start")
    lab(20, 244, "AUX2", "d", "start"); L((60, 240), (170, 240), (170, T[4]), c="VT")
    diode_v(170, 268, "-V2")
    comp(20, 270, 100, 60, "-M1", "ARB CKMTA12", "")
    pinl(120, T[4], "VT", "r")
    L((170, T[4]), (267, T[4]), c="VT"); L((123, T[4]), (170, T[4]), c="VT"); dot(170, T[4])
    diode_h(220, T[4], "-V1")
    lab(170, T[4] + 18, "-X3", "c"); lab(262, T[4] + 18, "VT 0.25", "c", "end")
    lab(20, 352, "внутри: реле давления,", "c", "start"); lab(20, 366, "2 реле 12 В 40 А", "c", "start")

    comp(20, 420, 100, 160, "-B1", "датчик", "0.5–4.5 В")
    for k, t, c in ((5, "+5 V", "OG"), (6, "GND", "BU"), (7, "OUT", "YE")):
        pinl(120, T[k], t, "r")
        L((123, T[k]), (267, T[k]), c=c)
    lab(140, T[5] - 6, "OG 0.25", "c", "start"); lab(140, T[6] - 6, "BU 0.25", "c", "start"); lab(140, T[7] - 6, "YE 0.25", "c", "start")

    # inside
    comp(600, 70, 200, 160, "-A1", "M5Stack DinMeter", "12 В → 5 В")
    pinl(600, T[1], "GND"); pinl(600, T[2], "+12 V")
    pinl(650, 230, "PORT.B"); pinl(720, 230, "PORT.A")
    L((313, T[1]), (597, T[1]), c="BK"); L((313, T[2]), (597, T[2]), c="RD")
    o.append('<rect x="390" y="165" width="120" height="190" class="b"/>')
    lab(396, 159, "-K1", "d", "start"); lab(450, 330, "Mini Relay", "d"); lab(450, 346, "3 A", "c")
    # relay contact inside -K1, normally open: COM blade, NO fixed contact
    o.append(f'<line x1="393" y1="{T[3]}" x2="440" y2="{T[3]}" stroke="#111" stroke-width="2"/><line x1="440" y1="{T[3]}" x2="466" y2="{T[3]+48}" stroke="#111" stroke-width="2"/>'
             f'<line x1="393" y1="{T[4]}" x2="472" y2="{T[4]}" stroke="#111" stroke-width="2"/><line x1="472" y1="{T[4]}" x2="472" y2="{T[3]+56}" stroke="#111" stroke-width="2"/>')
    o.append(f'<circle cx="390" cy="{T[3]}" r="3" class="t"/><circle cx="390" cy="{T[4]}" r="3" class="t"/>')
    lab(398, T[3] - 6, "COM", "n", "start"); lab(398, T[4] - 6, "NO", "n", "start")
    L((313, T[3]), (387, T[3]), c="RD"); L((313, T[4]), (387, T[4]), c="VT")
    o.append('<circle cx="510" cy="245" r="3" class="t"/>')
    grove4([(650, 236), (650, 245), (516, 245)]); lab(530, 268, "-W1 Grove 10 см", "c", "start")
    comp(620, 400, 160, 180, "-A2", "ADC v1.1", "ADS1110, I²C")
    o.append('<circle cx="720" cy="400" r="3" class="t"/>')
    grove4([(720, 236), (720, 394)], ("WH", "YE", "BK", "RD")); lab(736, 320, "-W2 Grove 10 см", "c", "start")
    pinl(620, T[6], "GND"); pinl(620, T[7], "IN")
    L((313, T[6]), (617, T[6]), c="BU"); L((313, T[7]), (617, T[7]), c="YE")
    L((313, T[5]), (560, T[5]), (560, 360), (726, 360), c="OG"); dot(726, 360)
    lab(736, 364, "-X4 пайка на красную жилу W2", "c", "start")

    # title block
    o.append('<rect x="640" y="630" width="350" height="44" class="b"/>')
    lab(650, 648, "Wrangler Air · схема электрическая", "d", "start")
    lab(650, 666, "IEC 60617 / IEC 61082 · цвета IEC 60757 · лист 1/1", "c", "start")
    o.append("</svg>")
    return "".join(o)
