#!/usr/bin/env python3
"""Jeep Wrangler JL Unlimited Rubicon, side view, nose right: the owner's car for the phone scenes.

Geometry is read off the owner's reference photos (24.11.2026): the body from the photo taken square to the side
(1500 x 1000, wheel centres at x 340 and 1238, ground y 836, roof y 272), colours, wheels and details from the grey
Rubicon and from the owner's own car (Sting Gray, body-colour hardtop, stock 33 in tyres).

Photo px -> mask units (512 x 226, ground at y 226):  X = (x - 55) * K,  Y = 226 - (836 - (y + DROP)) * K,
K = 512 / 1370. DROP lowers the body 16 photo px: the photo car is lifted on 35 in tyres, the owner's is stock.

Output: android/.../JeepShape.kt = an ordered list of layers (filled polygons with a vertical gradient, or stroked
lines) that SceneArt draws as they are, plus the wheel places. With a second argument it also writes an SVG preview of
the same layers, so the picture can be checked without the phone.
Usage: jl_blueprint.py <JeepShape.kt> [preview.svg]"""
import math, sys

K = 512 / 1370.0
DROP = 16
GROUND = 226.0
def X(x): return (x - 55) * K
def Y(y): return GROUND - (836 - (y + DROP)) * K
def P(pts): return [(X(x), Y(y)) for x, y in pts]

def rounded(pts, r, n=4):
    """polygon (photo px) with every corner rounded by radius r"""
    out = []
    m = len(pts)
    for i in range(m):
        p0, v, p1 = pts[i - 1], pts[i], pts[(i + 1) % m]
        d0 = math.hypot(p0[0] - v[0], p0[1] - v[1]); d1 = math.hypot(p1[0] - v[0], p1[1] - v[1])
        d = min(r, d0 / 2, d1 / 2)
        if d < 0.5: out.append(v); continue
        a = (v[0] + (p0[0] - v[0]) / d0 * d, v[1] + (p0[1] - v[1]) / d0 * d)
        b = (v[0] + (p1[0] - v[0]) / d1 * d, v[1] + (p1[1] - v[1]) / d1 * d)
        for k in range(n + 1):
            t = k / n
            out.append(((1 - t) ** 2 * a[0] + 2 * t * (1 - t) * v[0] + t * t * b[0],
                        (1 - t) ** 2 * a[1] + 2 * t * (1 - t) * v[1] + t * t * b[1]))
    return out
def rect(x0, y0, x1, y1, r=0): return rounded([(x0, y0), (x1, y0), (x1, y1), (x0, y1)], r) if r else [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]

layers = []   # (kind, pts(mask), c0, c1, y0, y1, stroke, sw, tint)
def fill(pts, c0, c1=None, tint=False, stroke=0, sw=0.0):
    m = P(pts); ys = [q[1] for q in m]
    layers.append(('F', m, c0, c0 if c1 is None else c1, min(ys), max(ys), stroke, sw, tint))
def line(pts, col, sw):
    layers.append(('L', P(pts), 0, 0, 0, 0, col, sw, False))

# ---------------- colours ----------------
BODY_HI, BODY, BODY_LO = 0xFFA4AAAE, 0xFF868C90, 0xFF5E6469      # Sting Gray: a cool medium grey
BLACK, TRIM, TRIM_HI = 0xFF121315, 0xFF26272A, 0xFF3B3D42          # plastic flares, rails, bumpers
GLASS_T, GLASS_B = 0xFF323B44, 0xFF0E1216
SEAM = 0xFF3C4044

# ---------------- spare wheel on the tailgate (behind the body), seen on its tread ----------------
fill(rect(62, 391, 148, 641, 30), 0xFF1A1B1E, 0xFF0B0B0D)
for y in range(408, 628, 20):                      # tread blocks, inside the outline
    fill(rect(66, y, 100, y + 11, 3), 0xFF2A2B2F)
    fill(rect(106, y + 9, 144, y + 20, 3), 0xFF222326)
fill(rect(110, 366, 150, 378, 3), 0xFF7A1418)      # high-mount stop lamp over the spare

# ---------------- body: tub, hood and the body-colour hardtop in one outline ----------------
body = [(150, 604), (143, 540), (142, 476), (146, 453), (170, 447), (170, 302), (174, 287), (184, 277), (198, 272),
        (856, 272), (866, 275), (872, 284), (962, 428), (986, 430), (1040, 427), (1150, 432), (1250, 444), (1298, 455),
        (1320, 466), (1331, 481), (1336, 500), (1337, 548), (1332, 574), (1320, 602), (1300, 645), (180, 645), (160, 632)]
fill(rounded(body, 7), BODY_HI, BODY_LO, tint=True, stroke=0x99000000, sw=0.9)
# light from above: roof edge, the top of the tub under the belt line, the hood side
fill([(172, 276), (864, 276), (868, 286), (172, 286)], 0x55FFFFFF, 0x00FFFFFF)
fill([(146, 454), (958, 454), (960, 470), (146, 470)], 0x40FFFFFF, 0x00FFFFFF)
fill([(986, 432), (1150, 434), (1298, 457), (1330, 484), (1330, 496), (986, 452)], 0x38FFFFFF, 0x00FFFFFF)
fill([(150, 560), (1318, 560), (1300, 645), (180, 645)], 0x00000000, 0x55000000)      # lower body falls into shade
# hardtop joints: over the tub, the roof gutter, the Freedom panel joint
line([(172, 448), (432, 448)], SEAM, 1.1)
line([(176, 290), (862, 290)], 0x66000000, 0.8)
line([(668, 273), (668, 296)], SEAM, 0.9)

# ---------------- glass: black rubber frames, tinted panes, a soft reflection ----------------
frames = [rect(186, 291, 420, 436, 16), rect(452, 296, 646, 437, 11),
          rounded([(688, 302), (869, 302), (911, 431), (911, 446), (688, 446)], 8)]
panes = [rect(197, 302, 409, 425, 10), rect(461, 304, 637, 429, 6),
         rounded([(697, 310), (862, 310), (901, 428), (901, 438), (697, 438)], 5)]
for f in frames: fill(f, BLACK)
for g in panes: fill(g, GLASS_T, GLASS_B)
fill(rect(506, 304, 513, 429), BLACK)                                   # rear door glass divider
for x0, x1 in ((215, 300), (470, 500), (712, 800)):                    # reflections, top left to bottom right
    w = x1 - x0
    fill([(x0 + w * 0.55, 306), (x0 + w, 306), (x0 + w * 0.45, 424), (x0, 424)], 0x1EFFFFFF)

# ---------------- doors: seams, hinges on the pillars, handles, mirror ----------------
line([(432, 297), (432, 505), (446, 548), (470, 602), (488, 632), (668, 632)], SEAM, 1.1)   # rear door, round the arch
line([(670, 297), (670, 632)], SEAM, 1.1)                                                      # B pillar
line([(672, 632), (955, 632), (957, 442)], SEAM, 1.1)                                          # front door
for x0, y0, x1, y1 in ((617, 473, 660, 500), (617, 566, 660, 592), (910, 475, 952, 500), (910, 568, 952, 594)):
    fill(rect(x0, y0, x1, y1, 4), 0xFFB4B9BD, 0xFF7C8286, tint=True, stroke=0xFF2E3134, sw=0.8)
    line([(x0 + 14, y0 + 2), (x0 + 14, y1 - 2)], 0xFF4A4E52, 0.7)
    fill(rect(x1 - 12, (y0 + y1) / 2 - 4, x1 - 4, (y0 + y1) / 2 + 4, 3), 0xFF5A5F63)
for x0, y0, x1, y1 in ((465, 472, 536, 489), (688, 475, 759, 492)):
    fill(rect(x0 - 3, y0 + 3, x1 + 3, y1 + 6, 6), 0x33000000)             # recess
    fill(rect(x0, y0, x1, y1, 7), 0xFF1B1C1F, 0xFF0C0C0E)
fill(rect(884, 440, 918, 452, 3), BLACK)                                    # mirror arm
fill(rounded([(882, 372), (931, 367), (937, 438), (889, 446)], 9), 0xFF2A2B2F, 0xFF0E0F11)
fill(rect(950, 424, 986, 437, 3), BLACK)                                    # cowl / wiper tray
line([(972, 470), (966, 296)], 0xFF1C1D20, 0.7)                             # antenna

# ---------------- hood side: joint to the fender, latch, RUBICON goes here (text, see DECAL) ----------------
line([(990, 449), (1150, 453), (1250, 465), (1304, 479)], SEAM, 0.9)
fill(rounded([(1288, 458), (1306, 461), (1305, 480), (1289, 477)], 3), BLACK)

# ---------------- wheel wells (dark), then the flares as bands round them ----------------
r_out = [(158, 612), (170, 570), (190, 528), (210, 500), (232, 489), (428, 487), (452, 500), (470, 535), (490, 590), (506, 645)]
r_in = [(196, 612), (206, 575), (222, 540), (238, 520), (252, 515), (415, 514), (432, 524), (446, 550), (462, 600), (474, 645)]
f_out = [(1008, 650), (1022, 610), (1042, 560), (1060, 522), (1080, 504), (1100, 499), (1318, 504), (1340, 514), (1352, 532), (1356, 556), (1350, 566)]
f_in = [(1046, 650), (1058, 612), (1074, 570), (1090, 545), (1106, 534), (1305, 536), (1326, 546), (1336, 562), (1338, 580)]
fill(r_in + [(474, 650), (196, 650)], 0xFF0A0A0C)
fill(f_in + [(1338, 610), (1326, 650), (1046, 650)], 0xFF0A0A0C)
fill(r_out + r_in[::-1], TRIM_HI, TRIM, stroke=0xFF0C0C0E, sw=0.6)
fill(f_out + f_in[::-1], TRIM_HI, TRIM, stroke=0xFF0C0C0E, sw=0.6)
line([(234, 491), (426, 489)], 0xFF5C5F65, 0.8)                             # flare tops catch the light
line([(1102, 501), (1316, 506)], 0xFF5C5F65, 0.8)
fill(rounded([(1056, 536), (1064, 506), (1086, 502), (1081, 536)], 3), BLACK)   # fender vent, three gills
for y in (512, 520, 528): line([(1064, y), (1081, y)], 0xFF4A4C52, 0.6)
fill(rect(1326, 522, 1352, 531, 3), 0xFFF09A2A)                              # amber marker on the flare tip

# ---------------- rock rail, steel bumpers, lamps ----------------
fill(rect(512, 646, 1010, 668, 9), TRIM_HI, 0xFF101113, stroke=0xFF0A0A0B, sw=0.6)
line([(520, 650), (1002, 650)], 0xFF5C5F65, 0.7)
fill(rounded([(1320, 610), (1384, 610), (1372, 634), (1316, 634)], 4), 0xFF17181A)          # skid under the bumper
fill(rounded([(1338, 548), (1408, 548), (1420, 562), (1420, 598), (1408, 612), (1338, 612)], 6), TRIM_HI, 0xFF101113, stroke=0xFF0A0A0B, sw=0.6)
fill(rect(1392, 538, 1410, 552, 3), 0xFFC81E28)                              # red tow hook
fill(rounded([(102, 600), (158, 600), (158, 648), (114, 648), (100, 636)], 5), TRIM_HI, 0xFF101113, stroke=0xFF0A0A0B, sw=0.6)
fill(rect(121, 468, 159, 538, 5), BLACK)                                     # tail lamp housing
fill(rect(126, 473, 154, 512, 3), 0xFFE0262E, 0xFF8E1218)
fill(rect(126, 516, 154, 532, 3), 0xFFD9DDE0, 0xFF9AA0A5)
fill(rounded([(1326, 480), (1338, 484), (1340, 518), (1328, 522)], 4), 0xFF1A1B1E)          # headlamp bucket, side on
fill(rounded([(1331, 486), (1339, 489), (1340, 514), (1332, 517)], 3), 0xFFF2F5F7, 0xFFB9C2C9)

# ---------------- places the app needs ----------------
REAR_C, FRONT_C, TYRE = 340, 1238, 125          # wheel centres (photo x) and the 33 in tyre radius (photo px)
WR = TYRE * K
consts = {
    'W': 512.0, 'H': GROUND,
    'PIVOT_X': X(REAR_C), 'PIVOT_Y': GROUND,
    'REAR_X': X(REAR_C), 'FRONT_X': X(FRONT_C), 'WHEEL_R': WR, 'WHEEL_Y': GROUND - WR,
    'HEAD_X': X(1338), 'HEAD_Y': Y(500), 'TAIL_X': X(128), 'TAIL_Y': Y(492),
    'DECAL_X0': X(1064), 'DECAL_X1': X(1284), 'DECAL_Y': Y(476), 'DECAL_H': 23 * K,
    'BADGE_X': X(972), 'BADGE_Y': Y(592), 'BADGE_H': 20 * K,
}

def hexc(c): return f'0x{c & 0xFFFFFFFF:08X}.toInt()'
kt = ['package air.wrangler', '',
      '/** The owner\'s Jeep (JL Unlimited Rubicon, Sting Gray) as drawing layers, read off his reference photos.',
      ' *  Generated by tools/jl_blueprint.py: do not edit by hand. Mask units 512 x 226, nose right, ground at y = 226. */',
      'object JeepShape {',
      '    /** kind 0 = filled polygon (vertical gradient c0 at y0 to c1 at y1, optional outline), 1 = stroked line.',
      '     *  tint: a body-colour part, the scene may tint it */',
      '    class Layer(val kind: Int, val pts: FloatArray, val c0: Int, val c1: Int, val y0: Float, val y1: Float,',
      '                val stroke: Int, val sw: Float, val tint: Boolean)']
for k, v in consts.items(): kt.append(f'    const val {k} = {v:.2f}f')
kt.append('    val LAYERS = listOf(')
for kind, pts, c0, c1, y0, y1, stroke, sw, tint in layers:
    arr = ', '.join(f'{x:.2f}f, {y:.2f}f' for x, y in pts)
    kt.append(f'        Layer({0 if kind == "F" else 1}, floatArrayOf({arr}), {hexc(c0)}, {hexc(c1)}, {y0:.2f}f, {y1:.2f}f, {hexc(stroke)}, {sw}f, {"true" if tint else "false"}),')
kt += ['    )', '}', '']
open(sys.argv[1], 'w').write('\n'.join(kt))
print(len(layers), 'layers;', 'wheels at', round(X(REAR_C), 1), round(X(FRONT_C), 1), 'r', round(WR, 1))

if len(sys.argv) > 2:      # preview with the same layers
    def rgba(c):
        a = (c >> 24) & 0xFF
        return f'rgb({(c >> 16) & 255},{(c >> 8) & 255},{c & 255})', a / 255.0
    S = 2
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{int(532 * S)}" height="{int(250 * S)}" viewBox="-10 -4 532 250">',
           '<defs>']
    body_svg = []
    for i, (kind, pts, c0, c1, y0, y1, stroke, sw, tint) in enumerate(layers):
        d = ' '.join(f'{x:.2f},{y:.2f}' for x, y in pts)
        if kind == 'F':
            a0, o0 = rgba(c0); a1, o1 = rgba(c1)
            out.append(f'<linearGradient id="g{i}" gradientUnits="userSpaceOnUse" x1="0" y1="{y0:.2f}" x2="0" y2="{y1:.2f}"><stop offset="0" stop-color="{a0}" stop-opacity="{o0:.3f}"/><stop offset="1" stop-color="{a1}" stop-opacity="{o1:.3f}"/></linearGradient>')
            st = ''
            if sw: sc, so = rgba(stroke); st = f' stroke="{sc}" stroke-opacity="{so:.2f}" stroke-width="{sw}"'
            body_svg.append(f'<polygon points="{d}" fill="url(#g{i})"{st}/>')
        else:
            sc, so = rgba(stroke)
            body_svg.append(f'<polyline points="{d}" fill="none" stroke="{sc}" stroke-opacity="{so:.2f}" stroke-width="{sw}" stroke-linecap="round" stroke-linejoin="round"/>')
    out.append('</defs>')
    out.append('<rect x="-10" y="-4" width="532" height="250" fill="#b9a27a"/><rect x="-10" y="226" width="532" height="24" fill="#7d6a4c"/>')
    out += body_svg
    c = consts
    for cx in (c['REAR_X'], c['FRONT_X']):
        out.append(f'<circle cx="{cx:.1f}" cy="{c["WHEEL_Y"]:.1f}" r="{WR:.1f}" fill="#141517"/><circle cx="{cx:.1f}" cy="{c["WHEEL_Y"]:.1f}" r="{WR * 0.5:.1f}" fill="#0d0d0f" stroke="#8a8e95" stroke-width="1.5"/>')
        for k in range(5):
            a = math.radians(k * 72 - 90)
            out.append(f'<line x1="{cx + WR * 0.12 * math.cos(a):.1f}" y1="{c["WHEEL_Y"] + WR * 0.12 * math.sin(a):.1f}" x2="{cx + WR * 0.45 * math.cos(a):.1f}" y2="{c["WHEEL_Y"] + WR * 0.45 * math.sin(a):.1f}" stroke="#a9adb3" stroke-width="{WR * 0.17:.1f}"/>')
    dw = c['DECAL_X1'] - c['DECAL_X0']
    out.append(f'<text x="{c["DECAL_X0"]:.1f}" y="{c["DECAL_Y"]:.1f}" font-family="Arial Black, Arial" font-weight="900" font-size="{c["DECAL_H"] * 1.25:.1f}" textLength="{dw:.1f}" lengthAdjust="spacingAndGlyphs" fill="#b4232a" stroke="#3a0a0d" stroke-width="0.3">RUBICON</text>')
    out.append(f'<text x="{c["BADGE_X"]:.1f}" y="{c["BADGE_Y"]:.1f}" font-family="Arial" font-weight="700" font-style="italic" font-size="{c["BADGE_H"] * 1.2:.1f}" fill="#8e1f2a">Jeep</text>')
    out.append('</svg>')
    open(sys.argv[2], 'w').write('\n'.join(out))
