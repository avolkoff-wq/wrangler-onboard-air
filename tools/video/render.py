"""Video: DinMeter calibration, then a real inflation 0.2 -> 2.3 bar.
Screen contents come from timeline.csv, logged by running the real firmware logic on virtual tyres.
usage: python3 render.py test   -> a few PNG frames
       python3 render.py        -> scene mp4s + final video
"""
import csv, math, subprocess, sys, os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
TL = os.path.join(HERE, '..', 'harness', 'timeline.csv')
W, H, FPS = 1280, 720, 30
BG, PANEL, INK, MUTED = (21, 25, 31), (31, 36, 44), (232, 236, 240), (150, 160, 172)
ORANGE, CYAN, GREEN, GREY, YEL = (255, 140, 40), (64, 214, 232), (182, 243, 74), (170, 170, 170), (255, 200, 60)
F = '/usr/share/fonts/opentype/inter/'
def font(name, size): return ImageFont.truetype(F + name, size)
SANS_B = lambda s: font('Inter-Bold.otf', s)
SANS_M = lambda s: font('Inter-Medium.otf', s)
SANS_R = lambda s: font('Inter-Regular.otf', s)
MONO_B = lambda s: ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf', s)

rows = list(csv.DictReader(open(TL)))
for r in rows:
    for k in ('t', 'shown', 'est', 'static_bar', 'raw', 'simp', 'entry', 'a', 'b', 'fit'):
        r[k] = float(r[k])
    for k in ('state', 'idx', 'on', 'starts', 'run_ms', 'phase_ms', 'phase_len', 'pause_left'):
        r[k] = int(float(r[k]))
cal_rows = [r for r in rows if r['scene'] == 'cal']
pump_rows = [r for r in rows if r['scene'] == 'pump']

def mmss(ms):
    t = ms // 100; s = t // 10
    return f"{s // 60}:{s % 60:02d}.{t % 10}"

# ---------------- DinMeter screen, 240 x 135, drawn at 2x then scaled into the bezel ----------------
SW, SH, K = 240, 135, 2
def screen_img(lines, top=None, dot=False):
    """lines: list of (text, kind, colour); kind: small/text/mid/digit"""
    im = Image.new('RGB', (SW * K, SH * K), (0, 0, 0)); d = ImageDraw.Draw(im)
    fonts = {'small': SANS_R(13 * K), 'text': SANS_B(14 * K), 'mid': SANS_B(18 * K), 'digit': MONO_B(46 * K)}
    y0 = 0
    if top:
        d.text((4 * K, 2 * K), top, font=fonts['small'], fill=GREY); y0 = 18 * K
    hs = []
    for t, k, c in lines:
        bb = d.textbbox((0, 0), t, font=fonts[k]); hs.append(bb[3] - bb[1])
    gap = max(2 * K, (SH * K - y0 - sum(hs)) / (len(lines) + 1))
    y = y0 + gap
    for (t, k, c), h in zip(lines, hs):
        bb = d.textbbox((0, 0), t, font=fonts[k])
        d.text(((SW * K - (bb[2] - bb[0])) / 2 - bb[0], y - bb[1]), t, font=fonts[k], fill=c)
        y += h + gap
    if dot: d.ellipse((SW * K - 26, 8, SW * K - 8, 26), fill=ORANGE)
    return im

def cal_screen(r):
    st, i = r['state'], r['idx']
    top = mmss(r['run_ms']) if st not in (5, 6) else None
    n = f"CAL {i + 1}/4"
    if st == 1: return screen_img([("CAL 1/4", 'text', INK), ("hose open to air", 'text', INK), ("press when ready", 'small', GREY)], top)
    if st == 2: return screen_img([(f"{n}: pumping to ~{i}", 'text', INK), (mmss(r['phase_ms']), 'mid', ORANGE), ("press: abort", 'small', GREY)], top, dot=True)
    if st == 3:
        left = max(0, r['phase_len'] - r['phase_ms']) / 1000
        return screen_img([(f"{n}: settling", 'text', INK), (f"{left:.1f}", 'digit', CYAN)], top)
    if st == 4: return screen_img([("Gauge shows?", 'text', INK), (f"{r['entry']:.2f}", 'digit', GREEN), ("turn + press", 'small', GREY)], top)
    if st == 5:
        pct = (r['a'] - 1) * 100; z = -r['b']
        l1 = "Sensor slope: exact" if abs(pct) < 0.2 else f"Sensor reads {abs(pct):.1f}% {'low' if pct > 0 else 'high'}"
        l2 = "Zero: exact" if abs(z) < 0.01 else f"Zero: {abs(z):.2f} bar {'high' if z > 0 else 'low'}"
        return screen_img([("CALIBRATION SAVED", 'text', (80, 220, 100)), (l1, 'text', INK), (l2, 'text', INK), (f"Accuracy +/-{r['fit']:.2f} bar", 'text', INK)])
    return screen_img([("", 'text', INK)])

def pump_screen(r):
    st = r['state']
    top = f"{mmss(r['run_ms'])}  starts {r['starts']}" if st != 0 else None
    L = r['phase_len']; e = r['phase_ms']
    tm = f"{max(0, L - e) / 1000:.1f}s" if L else mmss(e)
    tgt = "2.3"
    line, col = {0: ("SET 2.3  press: start", INK), 1: (f"SETTLING -> {tgt}", CYAN), 2: (f"MEASURING -> {tgt}", CYAN),
                 3: (f"LEARNING {tm}", ORANGE), 4: (f"PUMP -> {tgt}  {tm}", ORANGE), 5: (f"TOP-UP {tm}", ORANGE),
                 6: (f"DONE {r['static_bar']:.1f}  {mmss(r['run_ms'])}", (80, 220, 100))}.get(st, ("ERR", (230, 60, 60)))
    if st in (1, 2):
        return screen_img([(f"{r['pause_left'] / 1000:.1f}", 'digit', CYAN), ("s", 'text', GREY), (line, 'text', col)], top, dot=bool(r['on']))
    unit = ("~ bar" if r['on'] else "bar")
    return screen_img([(f"{max(0, r['shown']):.1f}", 'digit', GREEN), (unit, 'text', GREY), (line, 'text', col)], top, dot=bool(r['on']))

# ---------------- the device ----------------
def device(canvas, scr, knob_angle, pressed):
    d = ImageDraw.Draw(canvas)
    cx, cy, bw, bh = 400, 380, 640, 362          # bezel 53 x 30 mm
    x0, y0 = cx - bw // 2, cy - bh // 2
    d.rounded_rectangle((x0 + 10, y0 + 14, x0 + bw + 10, y0 + bh + 14), 26, fill=(10, 12, 15))      # shadow
    d.rounded_rectangle((x0, y0, x0 + bw, y0 + bh), 26, fill=(226, 96, 42))
    d.rounded_rectangle((x0 + 6, y0 + 6, x0 + bw - 6, y0 + bh - 6), 22, outline=(240, 130, 80), width=2)
    sx, sy, sw, sh = x0 + 34, y0 + 66, 392, 220                      # screen 240x135 scaled
    d.rounded_rectangle((sx - 10, sy - 10, sx + sw + 10, sy + sh + 10), 10, fill=(12, 14, 16))
    canvas.paste(scr.resize((sw, sh), Image.LANCZOS), (sx, sy))
    kx, ky, kr = x0 + bw - 108, cy, 74                                 # knob
    d.ellipse((kx - kr - 6, ky - kr - 6, kx + kr + 6, ky + kr + 6), fill=(150, 60, 25))
    for i in range(36):
        a = math.radians(i * 10 + knob_angle)
        d.line((kx + (kr - 2) * math.cos(a), ky + (kr - 2) * math.sin(a), kx + (kr - 9) * math.cos(a), ky + (kr - 9) * math.sin(a)), fill=(140, 140, 140), width=3)
    d.ellipse((kx - kr + 10, ky - kr + 10, kx + kr - 10, ky + kr - 10), fill=(205, 205, 208) if not pressed else (170, 170, 175))
    a = math.radians(knob_angle - 90)
    d.ellipse((kx + 40 * math.cos(a) - 6, ky + 40 * math.sin(a) - 6, kx + 40 * math.cos(a) + 6, ky + 40 * math.sin(a) + 6), fill=(110, 110, 115))
    if pressed: d.ellipse((kx - kr - 18, ky - kr - 18, kx + kr + 18, ky + kr + 18), outline=YEL, width=6)
    d.text((x0 + 34, y0 + 24), "DIN METER", font=SANS_B(20), fill=(255, 225, 205))

def wrap(d, text, f, width):
    out, cur = [], ''
    for w in text.split():
        c = (cur + ' ' + w).strip()
        if d.textlength(c, font=f) > width and cur: out.append(cur); cur = w
        else: cur = c
    if cur: out.append(cur)
    return out

def panel(canvas, title, step, what, do, simp, on, speed, chips):
    d = ImageDraw.Draw(canvas)
    px = 790
    d.rounded_rectangle((px, 40, W - 40, H - 40), 18, fill=PANEL)
    d.text((px + 30, 66), title, font=SANS_B(30), fill=INK)
    d.text((px + 30, 112), step, font=SANS_M(22), fill=ORANGE)
    y = 156
    for ln in wrap(d, what, SANS_R(22), 390): d.text((px + 30, y), ln, font=SANS_R(22), fill=INK); y += 30
    y += 10
    if do:
        d.text((px + 30, y), "Ты:", font=SANS_B(22), fill=YEL); y += 30
        for ln in wrap(d, do, SANS_M(22), 390): d.text((px + 30, y), ln, font=SANS_M(22), fill=YEL); y += 30
    # real tyre pressure, as the gauge would show it
    by = 500
    d.text((px + 30, by), "Реально в колёсах (манометр)", font=SANS_R(18), fill=MUTED)
    d.text((px + 30, by + 26), f"{simp:.2f} bar", font=MONO_B(36), fill=INK)
    bx0, bx1 = px + 30, W - 70
    d.rounded_rectangle((bx0, by + 76, bx1, by + 92), 8, fill=(48, 55, 66))
    fx = bx0 + (bx1 - bx0) * min(1, simp / 3.0)
    d.rounded_rectangle((bx0, by + 76, max(bx0 + 16, fx), by + 92), 8, fill=(90, 160, 255))
    for v in (1, 2, 3):
        x = bx0 + (bx1 - bx0) * v / 3; d.line((x, by + 72, x, by + 96), fill=MUTED, width=1)
        d.text((x - 6, by + 98), str(v), font=SANS_R(14), fill=MUTED)
    # compressor chip + speed badge
    cy = 640
    d.rounded_rectangle((px + 30, cy, px + 250, cy + 40), 20, fill=ORANGE if on else (55, 62, 74))
    d.text((px + 50, cy + 8), "компрессор ВКЛ" if on else "компрессор стоит", font=SANS_B(19), fill=(20, 20, 20) if on else MUTED)
    if speed > 1:
        d.rounded_rectangle((W - 150, cy, W - 70, cy + 40), 20, fill=(70, 80, 95))
        d.text((W - 132, cy + 7), f"×{speed}", font=SANS_B(22), fill=INK)
    for i, c in enumerate(chips):
        d.text((px + 270, cy + 10 + i * 0), c, font=SANS_R(16), fill=MUTED)

CAL_TXT = {1: ("Точка 0", "Шланг снят с колеса, датчик видит атмосферу. Манометр показывает 0.", "Нажимаешь кнопку"),
           2: ("Качает к точке", "Компрессор качает сам и остановится чуть раньше точки. Секундомер считает время качания.", ""),
           3: ("Пауза", "Компрессор стоит, давление в шланге выравнивается с колёсами. Крупно обратный отсчёт.", ""),
           4: ("Ввод с манометра", "Программа спрашивает, что показывает манометр. Число уже стоит на показании датчика.", "Крутишь ручку до значения манометра и нажимаешь"),
           5: ("Сохранено", "Прямая по 4 точкам. Датчик почти точный, разброс 0.01 bar.", "Нажимаешь: возврат на главный экран")}
PUMP_TXT = {0: ("Цель", "Цель 2.3 bar уже выставлена ручкой. В колёсах 0.2 bar после спуска.", "Нажимаешь: старт"),
            1: ("Успокоение", "Компрессор стоит. Программа ждёт, пока показание перестанет меняться.", ""),
            2: ("Замер", "Замер без потока: это реальное давление в колёсах.", ""),
            3: ("Обучающий импульс", "Первая подкачка после калибровки: 15 с, чтобы выучить, насколько датчик завышает на ходу.", ""),
            4: ("Основная подкачка", "Качает без остановок. Значок ~ перед bar: это оценка колёс, датчик минус выученная поправка на поток.", ""),
            5: ("Добивка", "Короткое включение на время по выученной скорости, с недолётом, а не перелётом.", ""),
            6: ("Готово", "Цель достигнута: в допуске 0.05 bar от 2.3.", "Нажимаешь, экран возвращается к цели")}

def frames_for(rows_, kind):
    """yields (row, speed, knob_angle, pressed, cal_index) at 30 fps; pumping runs x3"""
    vt, nf, ang = 0.0, 0, 0.0
    press_until = -1
    for i, r in enumerate(rows_):
        speed = 3 if r['on'] else 1
        if r['action'] == 'turn': ang += 10
        if r['action'] == 'press': press_until = vt + 0.35
        vt += 0.1 / speed
        while nf / FPS < vt:
            yield r, speed, ang, (nf / FPS) < press_until
            nf += 1

def frame(kind, r, speed, ang, pressed):
    im = Image.new('RGB', (W, H), BG)
    if kind == 'cal':
        scr = cal_screen(r); step, what, do = CAL_TXT.get(r['state'], ("", "", ""))
        if r['state'] in (2, 3, 4): step += f" {r['idx']} из 3"
        title = "1. Калибровка"
    else:
        scr = pump_screen(r); step, what, do = PUMP_TXT.get(r['state'], ("", "", ""))
        title = "2. Подкачка 0.2 → 2.3 bar"
    device(im, scr, ang, pressed)
    panel(im, title, step, what, do, r['simp'], r['on'], speed, [])
    return im

def card(text1, text2):
    im = Image.new('RGB', (W, H), BG); d = ImageDraw.Draw(im)
    for t, f, y, c in ((text1, SANS_B(54), 290, INK), (text2, SANS_R(28), 370, MUTED)):
        w = d.textlength(t, font=f); d.text(((W - w) / 2, y), t, font=f, fill=c)
    return im

def encode(name, gen):
    tmp = os.path.join(HERE, name + '.part.mp4')
    p = subprocess.Popen(['ffmpeg', '-y', '-loglevel', 'error', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', f'{W}x{H}', '-r', str(FPS),
                          '-i', '-', '-c:v', 'libx264', '-preset', 'veryfast', '-crf', '20', '-pix_fmt', 'yuv420p', tmp], stdin=subprocess.PIPE)
    for im in gen: p.stdin.write(im.tobytes())
    p.stdin.close()
    if p.wait() == 0: os.replace(tmp, os.path.join(HERE, name + '.mp4'))

def hold(im, sec):
    for _ in range(int(sec * FPS)): yield im

if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'test':
        pick = [('cal', next(r for r in cal_rows if r['state'] == 2 and r['idx'] == 2 and r['phase_ms'] > 9000)),
                ('cal', next(r for r in cal_rows if r['state'] == 4 and r['idx'] == 2)),
                ('cal', cal_rows[-1]),
                ('pump', next(r for r in pump_rows if r['state'] == 4 and r['phase_ms'] > 8000)),
                ('pump', next(r for r in pump_rows if r['state'] == 1 and r['starts'] == 2)),
                ('pump', pump_rows[-1])]
        for i, (k, r) in enumerate(pick):
            frame(k, r, 3 if r['on'] else 1, 30, False).save(os.path.join(HERE, f'test{i}.png'))
        sys.exit()
    intro = card("Wrangler Air: DinMeter", "калибровка, потом настоящая подкачка · симуляция по коду прошивки")
    encode('s0', hold(intro, 3))
    encode('s1', (frame('cal', *f[:4]) for f in frames_for(cal_rows, 'cal')))
    mid = card("Потом стравил колёса до 0.2 bar", "первая подкачка после калибровки")
    encode('s2', hold(mid, 2.5))
    def pump_gen():
        last = None
        for f in frames_for(pump_rows, 'pump'):
            last = frame('pump', *f[:4]); yield last
        yield from hold(last, 2)
    encode('s3', pump_gen())
    with open(os.path.join(HERE, 'list.txt'), 'w') as fl:
        for n in ('s0', 's1', 's2', 's3'): fl.write(f"file '{n}.mp4'\n")
    subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-f', 'concat', '-safe', '0', '-i', os.path.join(HERE, 'list.txt'), '-c', 'copy',
                    os.path.join(HERE, 'dinmeter_demo.mp4')], check=True)
    print('done')
