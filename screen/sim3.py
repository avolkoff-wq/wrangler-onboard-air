"""Idle preset screen with the current pressure and the direction it will go (inflate / deflate / ok / no air)."""
import importlib.util, math
spec = importlib.util.spec_from_file_location("m", "sim2.py"); m = importlib.util.module_from_spec(spec)
import sys; sys.argv = ["x"]; spec.loader.exec_module(m)
from PIL import Image, ImageDraw
GREY, LGREY, DGREY = m.GREY, m.LGREY, (110, 110, 110)

def direction(now, target):
    if now < 0.05: return "NO AIR", GREY
    if abs(now - target) <= 0.1: return "OK", (0, 220, 0)
    return ("INFLATE", m.ORANGE) if now < target else ("DEFLATE", m.SKY)

def idle2(name, target, now, portrait=False):
    W, H = (135, 240) if portrait else (240, 135)
    im = Image.new("RGB", (W, H)); d = ImageDraw.Draw(im)
    word, col = direction(now, target)
    dead = word == "NO AIR"
    if dead:   # everything grey: draw normally then desaturate
        pass
    val = f"{target:.1f}"
    numc = DGREY if dead else m.GY
    namec = DGREY if dead else (m.ORANGE if target < 0.8 else m.WHITE)
    arrow = {"INFLATE": "▲", "DEFLATE": "▼", "OK": "=", "NO AIR": ""}[word]
    nowline = f"actual {now:.1f}  {arrow} {word}".replace("  ", "  ")
    if not portrait:
        m.icon(d, name, 6, 6, 104)
        fb = m.FB(50); d.text((W - 8 - m.tw(d, val, fb), 2), val, font=fb, fill=numc)
        fu = m.FB(14); d.text((W - 8 - m.tw(d, "bar", fu), 56), "bar", font=fu, fill=LGREY if not dead else DGREY)
        fn = m.FB(19 if len(name) < 9 else 16)
        t = name + ("  LOW" if target < 0.8 else "")
        d.text(((W - m.tw(d, t, fn)) / 2, 72), t, font=fn, fill=namec)
        y = 98
        d.rectangle([0, y - 3, W, H], fill=(0, 0, 0))
        d.line([(10, y - 3), (W - 10, y - 3)], fill=col if not dead else DGREY, width=2)
        ft = m.FB(15); d.text(((W - m.tw(d, nowline, ft)) / 2, y + 1), nowline, font=ft, fill=col)
        fh = m.FS(11); h = "press to start" if word in ("INFLATE", "DEFLATE") else ""
        d.text(((W - m.tw(d, h, fh)) / 2, 120), h, font=fh, fill=GREY)
    else:
        m.icon(d, name, 6, 8, 123)
        fn = m.FB(17 if len(name) < 9 else 14); d.text(((W - m.tw(d, name, fn)) / 2, 90), name, font=fn, fill=namec)
        fb = m.FB(50); d.text(((W - m.tw(d, val, fb)) / 2, 110), val, font=fb, fill=numc)
        fu = m.FB(14); d.text(((W - m.tw(d, "bar", fu)) / 2, 164), "bar" + ("  LOW" if target < 0.8 else ""), font=fu, fill=(m.ORANGE if target < 0.8 and not dead else (LGREY if not dead else DGREY)))
        d.line([(10, 184), (W - 10, 184)], fill=col if not dead else DGREY, width=2)
        f1 = m.FB(15); t1 = f"actual {now:.1f}"; d.text(((W - m.tw(d, t1, f1)) / 2, 188), t1, font=f1, fill=col)
        t2 = f"{arrow} {word}".strip(); d.text(((W - m.tw(d, t2, f1)) / 2, 206), t2, font=f1, fill=col)
        fh = m.FS(11); h = "press to start" if word in ("INFLATE", "DEFLATE") else ""
        d.text(((W - m.tw(d, h, fh)) / 2, 226), h, font=fh, fill=GREY)
    if dead:   # grey out the icon too
        px = im.load()
        for x in range(W):
            for y in range(H):
                r, g, b = px[x, y]; v = int(0.3 * r + 0.59 * g + 0.11 * b) // 2
                px[x, y] = (v, v, v) if (r, g, b) != (0, 0, 0) else (0, 0, 0)
        d = ImageDraw.Draw(im)
        if not portrait:
            d.rectangle([0, 97, W, H], fill=(0, 0, 0))
            ft = m.FB(15); t = "CONNECT SYSTEM"; d.text(((W - m.tw(d, t, ft)) / 2, 99), t, font=ft, fill=LGREY)
            fh = m.FS(11); h = "actual 0.0 bar"; d.text(((W - m.tw(d, h, fh)) / 2, 120), h, font=fh, fill=GREY)
        else:
            f1 = m.FB(15); d.rectangle([0, 186, W, H], fill=(0, 0, 0)); d.text(((W - m.tw(d, "CONNECT", f1)) / 2, 188), "CONNECT", font=f1, fill=LGREY); d.text(((W - m.tw(d, "SYSTEM", f1)) / 2, 206), "SYSTEM", font=f1, fill=LGREY)
            fh = m.FS(11); d.text(((W - m.tw(d, "actual 0.0", fh)) / 2, 226), "actual 0.0", font=fh, fill=GREY)
    return im

cases = [("HIGHWAY", 2.5, 1.0, "Off-road done: HIGHWAY, now 1.0 -> inflate"), ("SAND", 0.8, 2.4, "Before the dunes: SAND, now 2.4 -> deflate"),
         ("ROUGH ROAD", 1.8, 1.85, "Already there: OK"), ("TRAIL", 1.2, 0.0, "0 bar: CONNECT SYSTEM, all grey")]
L = [m.frame(idle2(n, t, p), lab) for n, t, p, lab in cases]
P = [m.frame(idle2(n, t, p, portrait=True), lab.split(":")[0]) for n, t, p, lab in cases]
m.sheet(L, 2, "idle_dir_landscape.png"); m.sheet(P, 4, "idle_dir_portrait.png")
