"""Wrangler Air launcher icon: a tyre with tread, a gauge dial inside, an orange needle.
Writes icon.svg (preview/source) and the Android vector drawables (res/drawable/ic_fg.xml, ic_bg.xml, ic_mono.xml).
108x108 adaptive-icon viewport, everything inside the 66 dp safe circle (r 33)."""
import math, pathlib
C = 54.0
def circ(r, cx=C, cy=C):
    return f"M{cx-r:.2f},{cy:.2f} a{r:.2f},{r:.2f} 0 1,0 {2*r:.2f},0 a{r:.2f},{r:.2f} 0 1,0 {-2*r:.2f},0 Z"
def ring(ro, ri):  # even-odd ring
    return circ(ro) + " " + circ(ri)
def poly(pts):
    return "M" + " L".join(f"{x:.2f},{y:.2f}" for x, y in pts) + " Z"
def pol(r, a):  # a in degrees, 0 = up, clockwise
    t = math.radians(a); return (C + r * math.sin(t), C - r * math.cos(t))
def block(a, r0, r1, w0, w1):
    return poly([pol(r0, a - w0), pol(r1, a - w1), pol(r1, a + w1), pol(r0, a + w0)])

TREAD = " ".join(block(i * 360 / 28, 29.0, 33.0 if i % 2 else 32.0, 4.6, 3.6) for i in range(28))
SIDE = circ(30.2)
SHOULDER = ring(26.4, 25.6)
SIPES = " ".join(block(i * 360 / 28 + 6.4, 27.4, 30.0, 0.9, 0.9) for i in range(28))
DIAL = circ(20.0)
BEZEL = ring(20.6, 18.6)
TICKS = " ".join(block(-120 + i * 240 / 8, 13.8 if i % 2 else 11.6, 17.0, 1.6 if i % 2 else 2.6, 1.2 if i % 2 else 1.8) for i in range(9))
NA = 40  # needle angle, deg from up
tip = pol(16.0, NA); bl = pol(2.6, NA - 90); br = pol(2.6, NA + 90); tail = pol(5.0, NA + 180)
NEEDLE = poly([tip, br, tail, bl])
HUB = circ(3.0); HUBI = circ(1.3)

layers = [  # (path, colour, even-odd)
    (TREAD, "#1B1C18", False), (SIDE, "#1B1C18", False), (SHOULDER, "#3B3D36", True), (SIPES, "#33352F", False), (DIAL, "#4D5A3A", False), (BEZEL, "#8E9483", True),
    (TICKS, "#D9DCCF", False), (NEEDLE, "#F5A01F", False), (HUB, "#1A1C16", False), (HUBI, "#F5A01F", False)]
BG = "#8A5A26"

svg = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 108 108" width="432" height="432">',
       f'<rect width="108" height="108" fill="{BG}"/>']
svg += [f'<path d="{d}" fill="{c}"' + (' fill-rule="evenodd"' if eo else '') + '/>' for d, c, eo in layers]
svg.append('</svg>')
pathlib.Path("icon.svg").write_text("\n".join(svg))

V = '<vector xmlns:android="http://schemas.android.com/apk/res/android" android:width="108dp" android:height="108dp" android:viewportWidth="108" android:viewportHeight="108">\n'
def vpath(d, c, eo): return f'    <path android:fillColor="{c}"' + (' android:fillType="evenOdd"' if eo else '') + f' android:pathData="{d}" />\n'
res = pathlib.Path(__file__).resolve().parents[2] / "app/src/main/res/drawable"
(res / "ic_fg.xml").write_text(V + "".join(vpath(*l) for l in layers) + "</vector>\n")
(res / "ic_bg.xml").write_text(V + vpath("M0,0 L108,0 L108,108 L0,108 Z", BG, False) + "</vector>\n")
# themed icon (Android 13): one colour, the tyre, the ticks and the needle cut out of the dial
mono = [(TREAD, "#FFFFFF", False), (ring(29.6, 20.6), "#FFFFFF", True), (BEZEL, "#FFFFFF", True), (TICKS + " " + NEEDLE + " " + HUB, "#FFFFFF", False)]
(res / "ic_mono.xml").write_text(V + "".join(vpath(*l) for l in mono) + "</vector>\n")
print("ok")
