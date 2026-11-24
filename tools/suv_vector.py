#!/usr/bin/env python3
"""SUV mask (firmware suv_bitmap.h) -> smooth vector contours for the phone app (android/.../JeepShape.kt).
Pure Python: blur the 1-bit mask, trace the 0.5 level with marching squares (linear interpolation), drop tiny
contours, simplify (Douglas-Peucker) and round the corners (Chaikin). Coordinates stay in mask pixels."""
import re, sys, math
src, dst = sys.argv[1], sys.argv[2]
s = open(src).read()
W = int(re.search(r'SUV_BMP_W = (\d+)', s).group(1)); H = int(re.search(r'SUV_BMP_H = (\d+)', s).group(1))
px_ = float(re.search(r'SUV_PIVOT_X = ([\d.]+)f', s).group(1)); py_ = float(re.search(r'SUV_PIVOT_Y = ([\d.]+)f', s).group(1))
body = s[s.index('SUV_BMP[] = {') + 13:]; body = body[:body.index('}')]
vals = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', body)]
P = 2  # border
m = [[0.0] * (W + 2 * P) for _ in range(H + 2 * P)]
for y in range(H):
    for x in range(W):
        if (vals[(y * W + x) // 8] >> (7 - x % 8)) & 1: m[y + P][x + P] = 1.0
# blur 3x3 (weights 1 2 1)
k = [1, 2, 1]
def blur(a):
    h, w = len(a), len(a[0])
    t = [[sum(a[y][min(w - 1, max(0, x + i - 1))] * k[i] for i in range(3)) / 4 for x in range(w)] for y in range(h)]
    return [[sum(t[min(h - 1, max(0, y + i - 1))][x] * k[i] for i in range(3)) / 4 for x in range(w)] for y in range(h)]
f = blur(m)
h, w = len(f), len(f[0])
iso = 0.5
def lerp(p, q, a, b): t = (iso - a) / (b - a); return (p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t)
segs = []
for y in range(h - 1):
    for x in range(w - 1):
        a, b, c, d = f[y][x], f[y][x + 1], f[y + 1][x + 1], f[y + 1][x]
        idx = (a > iso) * 8 + (b > iso) * 4 + (c > iso) * 2 + (d > iso)
        if idx in (0, 15): continue
        e = {'t': lambda: lerp((x, y), (x + 1, y), a, b), 'r': lambda: lerp((x + 1, y), (x + 1, y + 1), b, c),
             'b': lambda: lerp((x, y + 1), (x + 1, y + 1), d, c), 'l': lambda: lerp((x, y), (x, y + 1), a, d)}
        table = {1: ['lb'], 2: ['br'], 3: ['lr'], 4: ['tr'], 5: ['lt', 'br'], 6: ['tb'], 7: ['lt'], 8: ['lt'], 9: ['tb'],
                 10: ['tr', 'lb'], 11: ['tr'], 12: ['lr'], 13: ['br'], 14: ['lb']}
        for pair in table[idx]: segs.append((e[pair[0]](), e[pair[1]]()))
# link segments into closed loops
key = lambda p: (round(p[0], 4), round(p[1], 4))
adj = {}
for i, (p, q) in enumerate(segs):
    adj.setdefault(key(p), []).append(i); adj.setdefault(key(q), []).append(i)
used = [False] * len(segs); loops = []
for i in range(len(segs)):
    if used[i]: continue
    used[i] = True; p, q = segs[i]; loop = [p, q]
    while True:
        nxt = [j for j in adj.get(key(loop[-1]), []) if not used[j]]
        if not nxt: break
        j = nxt[0]; used[j] = True; a2, b2 = segs[j]
        loop.append(b2 if key(a2) == key(loop[-1]) else a2)
    loops.append(loop)
def area(l): return abs(sum(l[i][0] * l[i - 1][1] - l[i - 1][0] * l[i][1] for i in range(len(l)))) / 2
loops = [l for l in loops if area(l) > 2.0]
def dp(pts, eps):
    if len(pts) < 3: return pts
    a, b = pts[0], pts[-1]; dx, dy = b[0] - a[0], b[1] - a[1]; L = math.hypot(dx, dy) or 1e-9
    i, dmax = 0, 0
    for k2 in range(1, len(pts) - 1):
        d = abs(dy * pts[k2][0] - dx * pts[k2][1] + b[0] * a[1] - b[1] * a[0]) / L
        if d > dmax: i, dmax = k2, d
    if dmax > eps: return dp(pts[:i + 1], eps)[:-1] + dp(pts[i:], eps)
    return [a, b]
def chaikin(pts, n=2):
    for _ in range(n):
        out = []
        for i in range(len(pts)):
            p, q = pts[i], pts[(i + 1) % len(pts)]
            out += [(0.75 * p[0] + 0.25 * q[0], 0.75 * p[1] + 0.25 * q[1]), (0.25 * p[0] + 0.75 * q[0], 0.25 * p[1] + 0.75 * q[1])]
        pts = out
    return pts
res = []
for l in loops:
    l = dp(l[:-1] if key(l[0]) == key(l[-1]) else l, 0.25)
    l = chaikin(l, 2)
    res.append([(round(x - P + 0.5, 2), round(y - P + 0.5, 2)) for x, y in l])
lines = []
for l in res: lines.append('        floatArrayOf(' + ', '.join(f'{x}f, {y}f' for x, y in l) + '),')
open(dst, 'w').write(f'''package air.wrangler

/** Jeep side silhouette as smooth contours, traced from the DinMeter's mask (tools/suv_vector.py, generated).
 *  Mask pixels, nose to the right; fill even-odd (windows are holes). Pivot = ground under the rear wheel. */
object JeepShape {{
    const val W = {W}f; const val H = {H}f
    const val PIVOT_X = {px_}f; const val PIVOT_Y = {py_}f
    val CONTOURS = listOf(
{chr(10).join(lines)}
    )
}}
''')
print(len(res), 'contours', sum(len(l) for l in res), 'points')
