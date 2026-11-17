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
    """Same circuit without solder: twin ferrule at relay COM, ground straight to the DinMeter,
    sensor +5 V tapped off the PORT.A Grove red wire with a 3M Scotchlok UB2A (wire not cut)."""
    o = ['<svg viewBox="0 0 1020 630" style="color:var(--fg);max-width:1000px;margin:0 auto;background:#fff;border-radius:8px">',
         '<style>.bt{font:700 14px var(--f);text-anchor:middle;fill:#1d232b}.bs{font:12px var(--f);text-anchor:middle;fill:#5d6672}'
         '.pl{font:600 12px var(--f);fill:#1d232b}.sp{font:700 13px var(--f);fill:#1b7f3b}.wl{font:12px var(--f);fill:#38404a}.hd{font:700 13px var(--f);fill:#5d6672}</style>']
    o.append('<rect x="250" y="30" width="550" height="560" rx="12" fill="#f6f7f9" stroke="#5d6672" stroke-width="2" stroke-dasharray="8 6"/>')
    o.append('<text x="525" y="22" class="hd" text-anchor="middle">КОРОБКА 1591C, ОДНА ПАЙКА</text>')
    o.append('<text x="125" y="22" class="hd" text-anchor="middle">МАШИНА</text><text x="925" y="22" class="hd" text-anchor="middle">ДАТЧИК</text>')
    o.append('<rect x="244" y="70" width="12" height="440" rx="4" fill="#fff" stroke="#e3612a" stroke-width="2"/>'
             '<text x="250" y="530" class="wl" text-anchor="middle">паз</text>')
    o.append(block(20, 70, 130, 50, "AUX1", "+12 В, выключатель"))
    o.append(block(20, 160, 130, 50, "AUX2", "+12 В, обход"))
    o.append(block(10, 280, 130, 140, "ARB CKMTA12", ""))
    o.append(pin(140, 320, "фиол.", "end"))
    o.append('<text x="18" y="360" class="wl">→ реле давления</text><text x="18" y="377" class="wl">→ 2 реле 40 А внутри</text><text x="18" y="394" class="wl">→ масса</text>')
    o.append(block(20, 470, 130, 50, "Масса", "кузов"))
    o.append(block(560, 60, 190, 150, "DinMeter", "внутри 12 В → 5 В на Grove"))
    o.append(pin(560, 110, "+ 12 В")); o.append(pin(560, 150, "−"))
    o.append(pin(610, 210, "PORT.B", "start", 8)); o.append(pin(700, 210, "PORT.A", "start", 8))
    o.append(block(360, 300, 160, 110, "Mini Relay", "на PORT.B, G2"))
    o.append(pin(360, 360, "COM")); o.append(pin(360, 395, "NO"))
    o.append(block(620, 340, 150, 90, "ADC v1.1", "на PORT.A, I²C"))
    o.append(pin(660, 430, "GND", "end", 8)); o.append(pin(730, 430, "IN", "start", 8))
    o.append(block(855, 380, 150, 160, "Датчик", "питание 5 В"))
    o.append(pin(855, 440, "+5 В", "start", 8)); o.append(pin(855, 480, "SIG", "start", 8)); o.append(pin(855, 520, "GND", "start", 8))
    # Grove
    o.append(grove([(610, 214), (610, 265), (450, 265), (450, 300)], (BLK, RED, YEL, WHT)))
    o.append('<text x="470" y="255" class="wl">Grove 10 см</text>')
    o.append(grove([(700, 214), (700, 340)], (WHT, YEL, BLK, RED)))
    o.append('<text x="660" y="300" class="wl" text-anchor="end">Grove 10 см</text>')
    # +12 V
    # AUX1 -> F1 -> solder-seal Y splice outside the box -> two red 0.75: relay COM and DinMeter +
    o.append(line([(150, 95), (222, 95)], RED, 4))
    o.append(fuse(185, 95, "F1 3 А"))
    o.append(line([(222, 95), (530, 95), (530, 110), (556, 110)], RED, 4))
    o.append('<text x="300" y="86" class="wl">0.75 красный на «+» DinMeter</text>')
    o.append(line([(222, 95), (222, 130), (310, 130), (310, 360), (356, 360)], RED, 4))
    o.append('<text x="316" y="250" class="wl" transform="rotate(90 316 250)">0.75 красный на COM</text>')
    o.append('<rect x="210" y="86" width="24" height="18" rx="5" fill="#ffffffcc" stroke="#c0392b" stroke-width="2"/>')
    o.append('<text x="222" y="72" class="sp" text-anchor="middle">Y</text>')
    # ground
    o.append(line([(150, 495), (545, 495), (545, 150), (556, 150)], BLK, 3))
    o.append('<text x="290" y="512" class="wl">0.25 чёрный, прямо в «−» DinMeter</text>')
    # K2 side
    o.append(line([(356, 395), (335, 395), (335, 460), (215, 460), (215, 320), (144, 320)], RED, 4))
    o.append('<text x="262" y="476" class="wl">0.75 на фиолетовый ARB</text>')
    o.append(diode(215, 410, "up")); o.append('<text x="224" y="414" class="pl">D1</text>')
    o.append(line([(150, 185), (190, 185), (190, 320)], RED, 3))
    o.append(diode(190, 260, "down")); o.append('<text x="168" y="264" class="pl" text-anchor="end">D2</text>')
    o.append('<circle cx="190" cy="320" r="4" fill="#111"/>')
    o.append('<text x="20" y="615" class="wl">F1: MINI 3 А в держателе TC-Mini FK1 · D1, D2: диоды MINI в таких же держателях · Y и выводы держателей: термоусадочные соединители с припоем</text>')
    # sensor: +5 V from the Grove red wire through a Scotchlok UB2A tap, GND and SIG to the ADC terminal
    ux = 700 + 4.5 * 1.5
    o.append(line([(851, 440), (830, 440), (830, 275), (ux + 8, 275)], RED, 3))
    o.append(splice(ux, 275, "пайка", 12, -12))
    o.append(line([(851, 480), (730, 480), (730, 434)], YEL, 3))
    o.append(line([(851, 520), (660, 520), (660, 434)], BLK, 3))
    o.append('<text x="855" y="565" class="wl">3 × 0.25 в термоусадке,</text><text x="855" y="582" class="wl">в коробку через паз</text>')
    o.append("</svg>")
    return "".join(o)


if __name__ == "__main__" and True:
    open("/tmp/power2.html", "w").write('<meta charset="utf-8"><body style="margin:0;--f:Inter,system-ui,sans-serif">' + power_svg_nosolder())
