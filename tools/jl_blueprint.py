#!/usr/bin/env python3
"""Jeep Wrangler JL Unlimited Rubicon, side view, nose right, traced from the owner's reference photo (a grey JL Rubicon
in profile, 1900 x 1425; owner 24.11: "it looks like a UAZ Patriot, not a JL"). Photo px -> mask: x' = 30 + (x - 55) * K,
y' = 226 - (1060 - y) * K, K = 0.27 (the spare wheel takes mask x 0..32). Writes android/.../JeepShape.kt: the outer body,
the glass, the seams, and the places of every part SceneArt draws on top (flares, hinges, handles, lamps, decal ...)."""
import sys
dst = sys.argv[1]
K = 0.27
def X(x): return round(30 + (x - 55) * K, 2)
def Y(y): return round(226 - (1060 - y) * K, 2)
def P(pts): return [(X(x), Y(y)) for x, y in pts]
def chaikin(pts, n=2):
    for _ in range(n):
        out = []
        for i in range(len(pts)):
            p, q = pts[i], pts[(i + 1) % len(pts)]
            out += [(0.8 * p[0] + 0.2 * q[0], 0.8 * p[1] + 0.2 * q[1]), (0.2 * p[0] + 0.8 * q[0], 0.2 * p[1] + 0.8 * q[1])]
        pts = out
    return pts
def rect(x0, y0, x1, y1): return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
outer = P([(100, 840), (95, 570), (100, 360), (115, 300), (150, 282), (985, 280), (1005, 300),   # tail, roof
           (1118, 512), (1175, 520), (1450, 532), (1610, 548), (1690, 572), (1712, 605),        # windshield, cowl, hood
           (1722, 700), (1715, 760), (1700, 840)])                                              # grille face, chin
glass = [P([(140, 352), (385, 352), (385, 525), (140, 525)]),                                     # rear quarter
         P([(447, 357), (672, 357), (672, 535), (447, 535)]),                                     # rear door
         P([(752, 368), (952, 368), (1015, 470), (1020, 545), (752, 545)])]                      # front door
seams = [P(rect(418, 345, 424, 805)), P(rect(708, 345, 714, 815)), P([(1095, 520), (1101, 520), (1104, 820), (1098, 820)]),
         P(rect(424, 800, 1100, 806))]                                                            # door edges, door bottoms
contours = [chaikin(outer, 2)] + [chaikin(g, 2) for g in glass] + seams
def kt_arr(c): return 'floatArrayOf(' + ', '.join(f'{x:.2f}f, {y:.2f}f' for x, y in c) + ')'
def kt_r(x0, y0, x1, y1): return f'floatArrayOf({X(x0)}f, {Y(y0)}f, {X(x1)}f, {Y(y1)}f)'
rear_c, front_c, wr = 325, 1490, 157
parts = {
    # flares: outer bottom-left, top-left, top-right, outer bottom-right (photo); drawn as a band round the arch
    'FLARE_REAR': kt_arr(P([(98, 800), (175, 622), (452, 618), (540, 812)])),
    'FLARE_FRONT': kt_arr(P([(1175, 860), (1290, 640), (1690, 646), (1745, 735)])),
    'ROCK_RAIL': kt_r(520, 818, 1190, 862),
    'BUMPER_FRONT': kt_r(1700, 690, 1835, 860),
    'BUMPER_REAR': kt_r(52, 718, 140, 790),
    'SPARE': kt_r(-40, 440, 100, 800),
    'TAIL_LAMP': kt_r(60, 572, 104, 652),
    'MIRROR': kt_r(968, 468, 1052, 552),
    'HINGES': 'listOf(' + ', '.join(kt_r(*r) for r in [(648, 600, 700, 638), (648, 728, 700, 764), (1032, 612, 1092, 650), (1032, 740, 1092, 776)]) + ')',
    'HANDLES': 'listOf(' + ', '.join(kt_r(*r) for r in [(455, 594, 542, 614), (735, 604, 826, 624)]) + ')',
    'VENT': kt_r(1238, 650, 1300, 706),
    'ANTENNA': kt_r(1128, 255, 1136, 640),
    'BADGE': f'floatArrayOf({X(1150)}f, {Y(765)}f)',
    'DECAL': f'floatArrayOf({X(1255)}f, {Y(614)}f, {X(1598)}f)',      # RUBICON: left, baseline, right
    'MARKER': kt_r(1640, 700, 1690, 720),
    'HEADLAMP': f'floatArrayOf({X(1712)}f, {Y(630)}f)',
}
lines = ['        ' + kt_arr(c) + ',' for c in contours]
pl = '\n'.join(f'    val {k} = {v}' for k, v in parts.items())
open(dst, 'w').write(f'''package air.wrangler

/** Jeep Wrangler JL Unlimited Rubicon, side view, traced from the owner's reference photo (tools/jl_blueprint.py,
 *  generated). Mask units 512 x 226, nose right, ground at y = 226; pivot = ground under the rear wheel.
 *  CONTOURS: the outer body, the three windows, the door seams. Below: where SceneArt draws each part. */
object JeepShape {{
    const val W = 512f; const val H = 226f
    const val PIVOT_X = {X(rear_c)}f; const val PIVOT_Y = 226f
    const val REAR_X = {X(rear_c)}f; const val FRONT_X = {X(front_c)}f
    const val WHEEL_R = {wr * K:.2f}f; const val WHEEL_Y = {226 - wr * K:.2f}f
    const val ROOF_Y = {Y(280)}f; const val BELT_Y = {Y(548)}f; const val BODY_BOTTOM = {Y(840)}f
    const val ROOF_FRONT_X = {X(985)}f
    val CONTOURS = listOf(
{chr(10).join(lines)}
    )
{pl}
}}
''')
print('ok', X(rear_c), X(front_c), wr * K)
