"""Boot splash mock-up: VOLKOV AIR, a generic SUV drives over bumps. 240x135, 1.6 s at 25 fps, saved x2 as GIF."""
import math
from PIL import Image, ImageDraw, ImageFont

W, H, K, FPS, N = 240, 135, 2, 25, 40
F = "/usr/share/fonts/truetype/freefont/"
f_big = ImageFont.truetype(F + "FreeSansBold.ttf", 30)
f_sm = ImageFont.truetype(F + "FreeSansBold.ttf", 12)
WHITE, GREY, AMBER, GY = (255, 255, 255), (110, 110, 110), (255, 165, 0), (173, 255, 47)
GROUND = 112


def ground(x):
    """bumpy track: two humps and a dip, fixed in the world"""
    return GROUND - 12 * math.exp(-((x - 95) / 10) ** 2) - 9 * math.exp(-((x - 165) / 8) ** 2) + 3 * math.exp(-((x - 200) / 6) ** 2)


import importlib.util, sys
_spec = importlib.util.spec_from_file_location("sim2", __file__.replace("splash_mockup.py", "sim2.py"))
_m = importlib.util.module_from_spec(_spec); _argv = sys.argv; sys.argv = ["x"]; _spec.loader.exec_module(_m); sys.argv = _argv
SS = 1.4


def suv(d, xw):
    """the shared silhouette (sim2.suv); xw = rear wheel x, both wheels on the track, body pitched along them"""
    wb = _m.SUV_WB * SS
    yr, yf = ground(xw), ground(xw + wb)
    a = math.atan2(yf - yr, wb)
    _m.suv(d, xw, yr, SS, ang=math.degrees(a))


frames = []
for i in range(N):
    t = i / (N - 1)
    im = Image.new("RGB", (W, H)); d = ImageDraw.Draw(im)
    d.line([(x, ground(x)) for x in range(0, W)], fill=GREY)
    for x in range(4, W, 11):
        d.point((x, ground(x) + 4), fill=GREY)
    suv(d, -40 + t * 300)
    # title fades in during the first half, stays
    k = min(1.0, t * 2.2)
    c = tuple(int(v * k) for v in WHITE)
    tw = d.textlength("VOLKOV", font=f_big)
    aw = d.textlength(" AIR", font=f_big)
    x0 = (W - tw - aw) / 2
    d.text((x0, 30), "VOLKOV", font=f_big, fill=c)
    d.text((x0 + tw, 30), " AIR", font=f_big, fill=tuple(int(v * k) for v in AMBER))
    frames.append(im.resize((W * K, H * K), Image.NEAREST))
frames += [frames[-1]] * 8
frames[0].save("splash.gif", save_all=True, append_images=frames[1:], duration=1000 // FPS, loop=0)
sheet = Image.new("RGB", (W * K, (H * K + 4) * 3), (60, 60, 60))
for j, i in enumerate((14, 18, 27)):
    sheet.paste(frames[i], (0, j * (H * K + 4)))
sheet.save("splash_frames.png")
print("ok")
