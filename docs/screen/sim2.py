"""DinMeter screen mock-up v2: big, clear preset icons; landscape 240x135 and portrait 135x240. Native pixels, scaled x3."""
import math
from PIL import Image, ImageDraw, ImageFont

K = 3
F = "/usr/share/fonts/truetype/freefont/"
FS = lambda n: ImageFont.truetype(F + "FreeSans.ttf", n)
FB = lambda n: ImageFont.truetype(F + "FreeSansBold.ttf", n)
WHITE, GREY, LGREY, DGREY = (255, 255, 255), (150, 150, 150), (200, 200, 200), (90, 90, 90)
ORANGE, SKY, CYAN, GY, SANDC, ROCKC = (255, 165, 0), (135, 206, 235), (0, 255, 255), (173, 255, 47), (222, 184, 120), (160, 160, 170)
PRESETS = [("HIGHWAY", 2.5), ("ROUGH ROAD", 1.8), ("GRAVEL", 1.5), ("TRAIL", 1.2), ("ROCK", 1.0), ("SAND", 0.8), ("RECOVERY", 0.5), ("CUSTOM", 2.0)]


def suv(d, x, y, s, ang=0.0, sink=0.0):
    """filled boxy SUV side view, 40 x 22 units, (x, y) = rear wheel ground contact, s = px per unit"""
    a = math.radians(ang)
    def R(px, py):
        return (x + s * (px * math.cos(a) + py * math.sin(a)), y - s * (-px * math.sin(a) + py * math.cos(a)))
    body = [(-4, 3), (36, 3), (36, 11), (31, 12), (27, 20), (4, 20), (2, 12), (-4, 12)]
    d.polygon([R(*p) for p in body], fill=WHITE)
    for win in ([(5, 13), (14, 13), (14, 18), (6, 18)], [(16, 13), (26, 13), (24.5, 18), (16, 18)]):
        d.polygon([R(*p) for p in win], fill=(0, 0, 0))
    d.polygon([R(*p) for p in [(-6, 6), (-4, 6), (-4, 11), (-6, 11)]], fill=WHITE)   # spare wheel hint
    for wx in (4, 28):
        cx, cy = R(wx, 4.5 - sink)
        r = 5.2 * s
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(0, 0, 0), outline=WHITE, width=max(1, int(s * 1.6)))
        d.ellipse([cx - r * .35, cy - r * .35, cx + r * .35, cy + r * .35], fill=WHITE)


TREE, CACT, SUN, MOUNT, CONE = (40, 125, 60), (70, 160, 80), (255, 200, 40), (105, 118, 140), (255, 120, 0)
TOWER, LITWIN, SEA, FOAM = (85, 95, 115), (255, 220, 120), (40, 110, 190), (170, 210, 240)


def skyline(d, X, g, s):
    """city towers behind the road: (u0, width, height) in units, lit windows"""
    for u0, wd, ht in ((14, 7, 30), (22, 6, 22), (29, 8, 34), (38, 6, 26), (45, 8, 31)):
        d.rectangle([X(u0), g - ht * s, X(u0 + wd), g], fill=TOWER)
        for wy in range(4, int(ht) - 2, 4):
            for wx in (1.5, wd - 2.5):
                if (wy * 7 + u0) % 3:
                    d.rectangle([X(u0 + wx), g - (ht - wy) * s, X(u0 + wx + 1), g - (ht - wy) * s + s], fill=LITWIN)
    d.rectangle([X(32.5), g - 38 * s, X(33.2), g - 34 * s], fill=TOWER)    # antenna


def sea(d, X, g, s):
    """sea band behind the beach with a couple of wave crests"""
    top = g - 15 * s
    d.rectangle([X(0), top, X(52), g - 1 * s], fill=SEA)
    for u in range(2, 52, 9):
        d.arc([X(u), top - 1.5 * s, X(u + 5), top + 2 * s], 200, 340, fill=FOAM, width=max(1, int(s)))


def fir(d, X, g, s, u, hgt):
    """fir tree at unit u, height hgt units"""
    d.rectangle([X(u) - 0.8 * s, g - 3 * s, X(u) + 0.8 * s, g], fill=(110, 80, 50))
    for k in range(3):
        y0 = g - 3 * s - k * hgt * 0.25 * s
        wdt = (4.5 - k * 1.2) * s
        d.polygon([(X(u) - wdt, y0), (X(u) + wdt, y0), (X(u), y0 - hgt * 0.42 * s)], fill=TREE)


def cactus(d, X, g, s, u, hgt):
    c = 1.3 * s
    d.rounded_rectangle([X(u) - c, g - hgt * s, X(u) + c, g], radius=c, fill=CACT)
    d.rounded_rectangle([X(u) - 4 * s, g - hgt * 0.75 * s, X(u) - 4 * s + 2 * c, g - hgt * 0.4 * s], radius=c, fill=CACT)
    d.rectangle([X(u) - 4 * s, g - hgt * 0.45 * s, X(u), g - hgt * 0.45 * s + 2 * c], fill=CACT)
    d.rounded_rectangle([X(u) + 4 * s - 2 * c, g - hgt * 0.85 * s, X(u) + 4 * s, g - hgt * 0.5 * s], radius=c, fill=CACT)
    d.rectangle([X(u), g - hgt * 0.55 * s, X(u) + 4 * s, g - hgt * 0.55 * s + 2 * c], fill=CACT)


def sun(d, X, g, s, u, v, r=3.5):
    cx, cy = X(u), g - v * s
    for k in range(8):
        a = k * math.pi / 4
        d.line([(cx + math.cos(a) * r * 1.4 * s, cy + math.sin(a) * r * 1.4 * s), (cx + math.cos(a) * r * 2 * s, cy + math.sin(a) * r * 2 * s)], fill=SUN, width=max(1, int(s)))
    d.ellipse([cx - r * s, cy - r * s, cx + r * s, cy + r * s], fill=SUN)


def peak(d, X, g, s, u0, u1, top, snow=True):
    um = (u0 + u1) / 2
    d.polygon([(X(u0), g), (X(um), g - top * s), (X(u1), g)], fill=MOUNT)
    if snow:
        f = 0.28
        d.polygon([(X(um - (um - u0) * f), g - top * s * (1 - f)), (X(um), g - top * s), (X(um + (u1 - um) * f), g - top * s * (1 - f))], fill=WHITE)


def scenery(d, name, X, g, s):
    """background drawn before terrain and SUV"""
    if name == "HIGHWAY":
        skyline(d, X, g, s)
    elif name == "ROUGH ROAD":
        x = X(46); d.polygon([(x - 3.5 * s, g), (x + 3.5 * s, g), (x + 1 * s, g - 11 * s), (x - 1 * s, g - 11 * s)], fill=CONE)
        d.rectangle([x - 2.2 * s, g - 6 * s, x + 2.2 * s, g - 4.5 * s], fill=WHITE)
        d.rectangle([x - 4.5 * s, g - 1 * s, x + 4.5 * s, g], fill=CONE)
    elif name == "GRAVEL":
        fir(d, X, g, s, 48, 26); fir(d, X, g, s, 3, 18)
    elif name == "TRAIL":
        fir(d, X, g, s, 3, 22); fir(d, X, g, s, 47, 28); fir(d, X, g, s, 41, 20)
    elif name == "ROCK":
        peak(d, X, g, s, -6, 22, 30); peak(d, X, g, s, 38, 60, 24)
    elif name == "SAND":
        sun(d, X, g, s, 40, 25); cactus(d, X, g, s, 49, 19)
    elif name == "RECOVERY":
        sea(d, X, g, s); sun(d, X, g, s, 30, 26, r=3)


def icon(d, name, ox, oy, w):
    """icon box w x 0.62w, (ox, oy) top-left"""
    s = w / 52.0                       # SUV ~42 units long incl. spare
    h = w * 0.62
    g = oy + h - 6 * s                 # ground line
    lw = max(2, int(s * 1.4))
    X = lambda u: ox + u * s
    def ground(col=GREY):
        d.line([(ox, g), (ox + w, g)], fill=col, width=lw)
    scenery(d, name, X, g, s)
    if name == "HIGHWAY":
        ground()
        for i in range(0, 52, 10):
            d.line([(X(i), g + 4 * s), (X(i + 5), g + 4 * s)], fill=WHITE, width=lw)
        for k, yy in enumerate((10, 14, 18)):
            d.line([(X(0), g - yy * s), (X(5 - k), g - yy * s)], fill=LGREY, width=lw)
        suv(d, X(12), g, s)
    elif name == "ROUGH ROAD":
        pts = [(X(0), g), (X(20), g), (X(23), g + 5 * s), (X(31), g + 5 * s), (X(34), g), (X(52), g)]
        d.line(pts, fill=GREY, width=lw)
        d.line([(X(40), g + s), (X(42), g + 3 * s), (X(45), g + 2 * s), (X(47), g + 4 * s)], fill=GREY, width=max(1, lw - 1))
        d.line([(X(6), g + s), (X(8), g + 3 * s), (X(11), g + 2 * s)], fill=GREY, width=max(1, lw - 1))
        suv(d, X(9), g, s, ang=-3)
    elif name == "GRAVEL":
        ground()
        import random; random.seed(3)
        for _ in range(26):
            px, py = random.uniform(0, 52), random.uniform(1.5, 5.5)
            r = random.choice((0.8, 1.1, 1.4)) * s
            d.ellipse([X(px) - r, g + py * s - r, X(px) + r, g + py * s + r], fill=LGREY)
        suv(d, X(9), g, s)
    elif name == "TRAIL":
        pts = [(X(u), g + 2.2 * s * math.sin(u / 4.5)) for u in range(0, 53, 1)]
        d.line(pts, fill=GREY, width=lw)
        for u in (6, 18, 30, 42):      # tyre tracks
            d.line([(X(u), g + 4.5 * s), (X(u + 3), g + 4.5 * s)], fill=DGREY, width=lw)
        suv(d, X(9), g - 1.5 * s, s, ang=-5)
    elif name == "ROCK":
        ground()
        d.polygon([(X(24), g), (X(28), g - 8 * s), (X(34), g - 13 * s), (X(41), g - 12 * s), (X(47), g - 6 * s), (X(50), g)], fill=ROCKC)
        d.polygon([(X(3), g), (X(6), g - 3 * s), (X(10), g)], fill=ROCKC)
        suv(d, X(8), g - 0.5 * s, s, ang=-22)
    elif name == "SAND":
        pts = [(X(u), g + 2 * s - 11 * s * math.exp(-((u - 40) / 14.0) ** 2)) for u in range(0, 53)]
        d.polygon(pts + [(X(52), g + 6 * s), (X(0), g + 6 * s)], fill=SANDC)
        suv(d, X(8), g + 1.5 * s, s, ang=-12)
    elif name == "RECOVERY":
        suv(d, X(8), g + 3 * s, s, sink=0)
        pts = [(X(u), g - 1 * s + 1.8 * s * math.sin(u / 6.0)) for u in range(0, 53)]
        d.polygon(pts + [(X(52), g + 6 * s), (X(0), g + 6 * s)], fill=SANDC)
        ax = X(47)
        d.line([(ax, g - 20 * s), (ax, g - 9 * s)], fill=ORANGE, width=lw + 1)
        d.polygon([(ax - 4 * s, g - 10 * s), (ax + 4 * s, g - 10 * s), (ax, g - 4 * s)], fill=ORANGE)
    elif name == "CUSTOM":
        ground()
        suv(d, X(6), g, s)
        cx, cy, r = X(46), g - 17 * s, 5.5 * s
        d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=CYAN, width=lw)
        d.line([(cx - r * .55, cy), (cx + r * .55, cy)], fill=CYAN, width=lw)
        d.line([(cx, cy - r * .55), (cx, cy + r * .55)], fill=CYAN, width=lw)


def tw(d, t, f):
    return d.textlength(t, font=f)


def idle(name, bar, psi=False, edit=False, portrait=False):
    W, H = (135, 240) if portrait else (240, 135)
    im = Image.new("RGB", (W, H)); d = ImageDraw.Draw(im)
    val = f"{bar * 14.5038:.0f}" if psi else f"{bar:.1f}"
    unit = "psi" if psi else "bar"
    low = bar < 0.8
    numc = CYAN if edit else GY
    namec = CYAN if edit else (ORANGE if low else WHITE)
    if not portrait:
        icon(d, name, 6, 8, 112)                                    # 112 x 69
        fb = FB(50); d.text((W - 8 - tw(d, val, fb), 4), val, font=fb, fill=numc)
        fu = FB(14); d.text((W - 8 - tw(d, unit, fu), 60), unit, font=fu, fill=LGREY)
        fn = FB(20 if len(name) < 9 else 17)
        t = name + ("  LOW" if low and not edit else "")
        d.text(((W - tw(d, t, fn)) / 2, 86), t, font=fn, fill=namec)
        h = "turn 0.1   press: OK + start" if edit else "press: start"
        fh = FS(12); d.text(((W - tw(d, h, fh)) / 2, 115), h, font=fh, fill=GREY)
    else:
        icon(d, name, 6, 8, 123)                                    # 123 x 76
        fn = FB(17 if len(name) < 9 else 14)
        d.text(((W - tw(d, name, fn)) / 2, 92), name, font=fn, fill=namec)
        fb = FB(52); d.text(((W - tw(d, val, fb)) / 2, 116), val, font=fb, fill=numc)
        fu = FB(14); d.text(((W - tw(d, unit, fu)) / 2, 172), unit, font=fu, fill=LGREY)
        if low and not edit:
            fl = FB(14); d.text(((W - tw(d, "LOW", fl)) / 2, 192), "LOW", font=fl, fill=ORANGE)
        h = "turn 0.1, press OK" if edit else "press: start"
        fh = FS(12); d.text(((W - tw(d, h, fh)) / 2, 218), h, font=fh, fill=GREY)
    return im


def run(name, p0, target, cur, phase, portrait=False, starts=1, vents=0):
    W, H = (135, 240) if portrait else (240, 135)
    BG = {"PUMP": (90, 38, 0), "VENT": (0, 40, 90), "MEASURE": (0, 0, 0)}[phase]   # dark fill: pumping amber, venting blue
    im = Image.new("RGB", (W, H), BG); d = ImageDraw.Draw(im)
    col = {"PUMP": ORANGE, "VENT": SKY, "MEASURE": CYAN}[phase]
    mode = "DEF" if target < p0 else "INF"
    fs = FS(12)
    d.text((4, 2), f"{mode} 1:12.4" + ("" if portrait else f"  starts {starts}  vents {vents}"), font=fs, fill=LGREY)
    if phase != "MEASURE": d.ellipse([W - 15, 4, W - 4, 15], fill=col)
    val = ("~" if phase != "MEASURE" else "") + f"{cur:.1f}"
    if not portrait:
        icon(d, name, 6, 22, 64)
        fb = FB(48); d.text((W - 10 - tw(d, val, fb), 16), val, font=fb, fill=GY)
        y, x0, x1 = 78, 10, W - 10
    else:
        icon(d, name, 22, 22, 90)
        fb = FB(46); d.text(((W - tw(d, val, fb)) / 2, 82), val, font=fb, fill=GY)
        y, x0, x1 = 146, 8, W - 8
    d.rectangle([x0, y, x1, y + 12], outline=GREY, fill=(0, 0, 0))
    frac = max(0.0, min(1.0, (cur - p0) / (target - p0)))
    d.rectangle([x0 + 2, y + 2, x0 + 2 + int((x1 - x0 - 4) * frac), y + 10], fill=col)
    d.text((x0, y + 14), f"{p0:.1f}", font=fs, fill=GREY)
    d.text((x1 - tw(d, f"{target:.1f}", fs), y + 14), f"{target:.1f}", font=fs, fill=GREY)
    line = {"PUMP": "PUMP 0:42", "VENT": "VENT 12.6s", "MEASURE": "MEASURING"}[phase]
    ft = FB(15)
    d.text(((W - tw(d, line, ft)) / 2, H - 22 if not portrait else H - 50), line, font=ft, fill=col)
    if portrait:
        d.text(((W - tw(d, f"starts {starts} vents {vents}", fs)) / 2, H - 22), f"starts {starts} vents {vents}", font=fs, fill=GREY)
    return im


def frame(im, label):
    w, h = im.size
    big = im.resize((w * K, h * K), Image.NEAREST)
    out = Image.new("RGB", (w * K + 40, h * K + 72), (232, 234, 237))
    dr = ImageDraw.Draw(out)
    dr.rounded_rectangle([8, 8, w * K + 32, h * K + 32], 14, fill=(30, 30, 32))
    out.paste(big, (20, 20))
    dr.text((20, h * K + 40), label, font=FS(22), fill=(40, 40, 40))
    return out


def sheet(tiles, cols, path):
    tw_, th_ = tiles[0].size
    s = Image.new("RGB", (cols * tw_, math.ceil(len(tiles) / cols) * th_), (232, 234, 237))
    for i, t in enumerate(tiles):
        s.paste(t, ((i % cols) * tw_, (i // cols) * th_))
    s.save(path); print(path, s.size)


L = [frame(idle(n, b), f"{n} {b:.1f} bar") for n, b in PRESETS]
L += [frame(idle("HIGHWAY", 2.5, psi=True), "HIGHWAY in psi"), frame(idle("CUSTOM", 2.0, edit=True), "CUSTOM: setting"),
      frame(run("SAND", 2.5, 0.8, 1.6, "VENT", starts=0, vents=1), "Deflate to SAND"), frame(run("HIGHWAY", 0.8, 2.5, 1.9, "PUMP"), "Inflate to HIGHWAY")]
sheet(L, 3, "screens_landscape.png")
R = [frame(run("SAND", 2.5, 0.8, 1.6, "VENT", starts=0, vents=1), "Deflate: blue fill"), frame(run("HIGHWAY", 0.8, 2.5, 1.9, "PUMP"), "Inflate: amber fill"),
     frame(run("HIGHWAY", 0.8, 2.5, 2.3, "MEASURE"), "Pause: black"),
     frame(run("SAND", 2.5, 0.8, 1.6, "VENT", portrait=True, starts=0, vents=1), "Deflate"), frame(run("HIGHWAY", 0.8, 2.5, 1.9, "PUMP", portrait=True), "Inflate"),
     frame(run("HIGHWAY", 0.8, 2.5, 2.3, "MEASURE", portrait=True), "Pause")]
sheet(R[:3], 3, "screens_run_landscape.png"); sheet(R[3:], 3, "screens_run_portrait.png")
P = [frame(idle(n, b, portrait=True), f"{n} {b:.1f}") for n, b in PRESETS]
P += [frame(idle("CUSTOM", 2.0, edit=True, portrait=True), "CUSTOM: setting"),
      frame(run("SAND", 2.5, 0.8, 1.6, "VENT", portrait=True, starts=0, vents=1), "Deflate to SAND"),
      frame(run("HIGHWAY", 0.8, 2.5, 1.9, "PUMP", portrait=True), "Inflate to HIGHWAY"),
      frame(run("HIGHWAY", 0.8, 2.5, 2.3, "MEASURE", portrait=True), "Pause: measuring")]
sheet(P, 6, "screens_portrait.png")
