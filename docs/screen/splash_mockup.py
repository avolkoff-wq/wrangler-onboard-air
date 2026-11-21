"""Boot splash mock-up: VOLKOV AIR, a generic SUV drives over bumps. 240x135 and portrait 135x240 (firmware splash()), 1.6 s at 25 fps, saved x2 as GIF."""
import math
from PIL import Image, ImageDraw, ImageFont

K, FPS, N = 2, 25, 40
F = "/usr/share/fonts/truetype/freefont/"
f_big = ImageFont.truetype(F + "FreeSansBold.ttf", 30)
f_sm = ImageFont.truetype(F + "FreeSansBold.ttf", 12)
WHITE, GREY, AMBER, GY = (255, 255, 255), (110, 110, 110), (255, 165, 0), (173, 255, 47)

import importlib.util, sys
_spec = importlib.util.spec_from_file_location("sim2", __file__.replace("splash_mockup.py", "sim2.py"))
_m = importlib.util.module_from_spec(_spec); _argv = sys.argv; sys.argv = ["x"]; _spec.loader.exec_module(_m); sys.argv = _argv
SS = 1.4


def render(W, H, gif, sheet_png):
    port = H > W
    k = W / 240.0
    g = H - 23 * (2 if port else 1)

    def ground(x):
        """bumpy track: two humps and a dip, fixed in the world (firmware splashGround)"""
        x /= k
        return g - k * (12 * math.exp(-((x - 95) / 10) ** 2) + 9 * math.exp(-((x - 165) / 8) ** 2) - 3 * math.exp(-((x - 200) / 6) ** 2))

    frames = []
    for i in range(N):
        t = i / (N - 1)
        im = Image.new("RGB", (W, H)); d = ImageDraw.Draw(im)
        d.line([(x, ground(x)) for x in range(0, W)], fill=GREY)
        for x in range(4, W, 11):
            d.point((x, ground(x) + 4), fill=GREY)
        xw = (-40 + t * 300) * k
        wb = _m.SUV_WB * SS
        yr, yf = ground(xw), ground(xw + wb)
        _m.suv(d, xw, yr, SS, ang=math.degrees(math.atan2(yf - yr, wb)))
        f = min(1.0, t * 2.2)
        c = tuple(int(v * f) for v in WHITE); ca = tuple(int(v * f) for v in AMBER)
        if port:
            for txt, y, col in (("VOLKOV", 40, c), ("AIR", 80, ca)):
                d.text(((W - d.textlength(txt, font=f_big)) / 2, y), txt, font=f_big, fill=col)
        else:
            tw = d.textlength("VOLKOV", font=f_big); aw = d.textlength(" AIR", font=f_big)
            x0 = (W - tw - aw) / 2
            d.text((x0, 30), "VOLKOV", font=f_big, fill=c)
            d.text((x0 + tw, 30), " AIR", font=f_big, fill=ca)
        frames.append(im.resize((W * K, H * K), Image.NEAREST))
    frames += [frames[-1]] * 8
    frames[0].save(gif, save_all=True, append_images=frames[1:], duration=1000 // FPS, loop=0)
    picks = (14, 18, 27)
    if port:
        sh = Image.new("RGB", ((W * K + 4) * 3, H * K), (60, 60, 60))
        for j, i in enumerate(picks): sh.paste(frames[i], (j * (W * K + 4), 0))
    else:
        sh = Image.new("RGB", (W * K, (H * K + 4) * 3), (60, 60, 60))
        for j, i in enumerate(picks): sh.paste(frames[i], (0, j * (H * K + 4)))
    sh.save(sheet_png)


render(240, 135, "splash.gif", "splash_frames.png")
render(135, 240, "splash_portrait.gif", "splash_portrait_frames.png")
print("ok")
