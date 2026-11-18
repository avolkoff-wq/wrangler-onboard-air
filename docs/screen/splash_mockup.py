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


def suv(d, xr):
    """generic SUV, 40 long; xr = rear wheel x. wheels sit on the ground, body follows their line"""
    wb = 26
    xf = xr + wb
    yr, yf = ground(xr) - 5, ground(xf) - 5
    a = math.atan2(yf - yr, wb)
    ca, sa = math.cos(a), math.sin(a)
    def P(px, py):          # body coords: x along the wheelbase from the rear axle, y up from the axles
        return (xr + px * ca + py * sa, yr + px * sa - py * ca)
    body = [(-7, 2), (33, 2), (33, 10), (27, 10), (23, 18), (1, 18), (-1, 10), (-7, 10)]
    d.polygon([P(*p) for p in body], outline=WHITE)
    d.line([P(11, 10), P(11, 17)], fill=WHITE)
    d.line([P(-7, 7), P(-9, 7)], fill=WHITE)                      # spare wheel bump
    for x, y in ((xr, yr), (xf, yf)):
        d.ellipse([x - 5, y - 5, x + 5, y + 5], outline=WHITE, fill=(0, 0, 0))
        d.point((x, y), fill=WHITE)


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
    d.text((x0, 22), "VOLKOV", font=f_big, fill=c)
    d.text((x0 + tw, 22), " AIR", font=f_big, fill=tuple(int(v * k) for v in AMBER))
    d.text((W / 2 - d.textlength("0.4.0", font=f_sm) / 2, 60), "0.4.0", font=f_sm, fill=tuple(int(v * k) for v in GREY))
    frames.append(im.resize((W * K, H * K), Image.NEAREST))
frames += [frames[-1]] * 8
frames[0].save("splash.gif", save_all=True, append_images=frames[1:], duration=1000 // FPS, loop=0)
for i in (14, 18, 27):
    frames[i].save(f"splash_{i}.png")
print("ok")
