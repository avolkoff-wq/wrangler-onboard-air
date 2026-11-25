#!/usr/bin/env python3
"""Hands-off bench pass over USB (owner, 24.11: "do it automatically, over the M5 cable").

1. Layout check: walks the DinMeter screens with the USB knob commands and asks the firmware for the screen as drawn
   (`dl`: every text with its measured box). Flags a text that leaves the screen or two texts that overlap, in the
   current orientation and in the next one (menu SCREEN), then puts the orientation back.
2. DEMO: LEARN INFLATE and LEARN DEFLATE on the virtual tyres.
3. CALIBRATE with `fake` as the gauge (0 / ~1 / ~2 / ~3 bar, the dial left at the raw value): must end CAL_DONE near a = 1.
Leaves the device as found: DEMO off, fake off, learning forgotten, main screen.

Usage: python3 tools/bench_auto.py [port] [log file]     (needs pyserial; the PlatformIO python has it)"""
import datetime, glob, sys, time
import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else [p for p in glob.glob('/dev/cu.usbmodem*') if 'RFCY' not in p][0]
LOG = sys.argv[2] if len(sys.argv) > 2 else 'logs/bench_auto_%s.txt' % datetime.date.today().strftime('%Y%m%d')
s = serial.Serial(PORT, 115200, timeout=0.05)
log = open(LOG, 'a', buffering=1)
buf = b''
events = []          # firmware log lines (boot,t,EVENT,...)
issues = []

def pump(sec):
    """read for sec seconds, return the lines"""
    global buf
    out = []; end = time.time() + sec
    while time.time() < end:
        d = s.read(4096)
        if not d: continue
        buf += d
        while b'\n' in buf:
            l, buf = buf.split(b'\n', 1); l = l.decode('utf-8', 'replace').rstrip('\r')
            log.write('%s < %s\n' % (datetime.datetime.now().strftime('%H:%M:%S.%f')[:-3], l))
            out.append(l)
            if not l.startswith(('LIVE', 'UI,', 'IO,', 'DL', '#')) and l.count(',') >= 3: events.append(l)
    return out
def cmd(c, wait=0.45):
    log.write('%s > %s\n' % (datetime.datetime.now().strftime('%H:%M:%S.%f')[:-3], c))
    s.write((c + '\n').encode()); s.flush()
    return pump(wait)
def ui():
    for _ in range(4):
        for l in cmd('ui', 0.3):
            if l.startswith('UI,'):
                f = l.split(',')
                return dict(screen=int(f[2]), pos=int(f[3]), preset=int(f[4]), state=int(f[5]), err=int(f[6]), arm=int(f[7]), bar=float(f[8]))
    raise RuntimeError('no UI answer')
def dl():
    for _ in range(4):
        lines = cmd('dl', 0.5)
        head = [l for l in lines if l.startswith('DL,')]
        if not head: continue
        W, H, n, scr = [int(x) for x in head[0].split(',')[1:5]]
        items = []
        for l in lines:
            if l.startswith('DLT,'):
                f = l.split(',', 7)
                items.append(dict(x=int(f[1]), y=int(f[2]), w=int(f[3]), h=int(f[4]), datum=int(f[5]), font=int(f[6]), t=f[7]))
        return W, H, scr, items
    raise RuntimeError('no DL answer')
def box(i):
    left = i['x'] - (i['w'] / 2 if i['datum'] & 3 == 1 else i['w'] if i['datum'] & 3 == 2 else 0)
    v = i['datum'] & 12
    top = i['y'] - (i['h'] / 2 if v == 4 else i['h'] if v == 8 else 0)
    return left, top, left + i['w'], top + i['h']
checked = 0
def check(name):
    """texts inside the screen, no two texts on top of each other"""
    global checked
    time.sleep(0.25)
    W, H, scr, items = dl()
    checked += 1
    bx = [(i, box(i)) for i in items]
    for i, (l, t, r, b) in bx:
        if l < -1 or r > W + 1:
            issues.append('%dx%d %s: "%s" leaves the screen sideways (%d..%d of %d)' % (W, H, name, i['t'], l, r, W))
        if t < -1 or b > H + 3:   # font boxes carry a descender line below the glyphs
            issues.append('%dx%d %s: "%s" leaves the screen vertically (%d..%d of %d)' % (W, H, name, i['t'], t, b, H))
    for a in range(len(bx)):
        for c in range(a + 1, len(bx)):
            (ia, (l1, t1, r1, b1)), (ic, (l2, t2, r2, b2)) = bx[a], bx[c]
            if ia['t'] in ('BT', 'DEMO', 'FAKE') or ic['t'] in ('BT', 'DEMO', 'FAKE'): continue
            ox = min(r1, r2) - max(l1, l2); oy = min(b1, b2) - max(t1, t2)
            # the box height includes line spacing: count only a real overlap of the glyph rows
            if ox > 3 and oy > 0.55 * min(ia['h'], ic['h']):
                issues.append('%dx%d %s: "%s" overlaps "%s"' % (W, H, name, ia['t'], ic['t']))
    return W, H, [i['t'] for i in items]

def go_main():
    for _ in range(8):
        u = ui()
        if u['screen'] == 0 and u['state'] == 0 and u['arm'] == 0: return
        if u['state'] in (7, 8) or u['state'] in range(1, 7): cmd('press', 0.8)
        elif u['arm']: cmd('left', 0.5)
        elif u['screen'] != 0: cmd('hold', 0.8)
    raise RuntimeError('cannot reach the main screen')
def open_menu(group=None, item=0):
    """main -> menu top; optionally into group g (0 SETUP, 1 SERVICE, 2 CONNECT, 3 SAFETY, 4 PUMP TUNING) on item"""
    go_main(); cmd('hold', 0.8)
    assert ui()['screen'] == 1, 'menu did not open'
    if group is None: return
    for _ in range(group + 1): cmd('right', 0.25)
    cmd('press', 0.5)
    for _ in range(item): cmd('right', 0.25)
def start_run():
    for _ in range(6):
        cmd('press', 0.7)
        if ui()['state'] not in (0, 7, 8): return True
    return False

GROUPS = [('SETUP', 5), ('SERVICE', 5), ('CONNECT', 3), ('SAFETY', 7), ('PUMP TUNING', 3)]
def layout_pass(tag):
    go_main(); cmd('fake off', 0.6)
    for k in range(8):                                   # every preset, no pressure
        W, H, t = check('main, no pressure, preset %d' % ui()['preset']); cmd('right', 0.4)
    cmd('fake 1.0', 1.6)
    for k in range(8):                                   # every preset, 1.0 bar in the line
        check('main, 1.0 bar, preset %d' % ui()['preset']); cmd('right', 0.4)
    go_main()
    u = ui()
    if u['preset'] == 7: cmd('right', 0.4)               # CUSTOM opens its editor instead of arming
    cmd('press', 0.5); check('armed: press again'); cmd('press', 0.5)
    if ui()['arm'] == 2: check('armed: engine confirm')
    cmd('left', 0.5); go_main()
    # CUSTOM editor, both steps
    while ui()['preset'] != 7: cmd('right', 0.4)
    cmd('press', 0.6); check('CUSTOM editor, pressure'); cmd('press', 0.6); check('CUSTOM editor, wheels'); cmd('hold', 0.9); cmd('hold', 0.9)
    go_main()
    # menu: top and every group at its first and last row
    open_menu(); check('menu top');
    for _ in range(6): cmd('right', 0.25)
    check('menu top, last row')
    for g, (name, n) in enumerate(GROUPS):
        open_menu(g); check('menu %s' % name)
        for _ in range(n): cmd('right', 0.25)
        check('menu %s, last row' % name)
    # leaf screens
    for g, item, name, leave in ((1, 0, 'SENSOR LIVE', 'press'), (1, 1, 'OUTPUT TEST', 'hold'), (1, 2, 'COMPRESSOR HOURS', 'press'),
                                 (1, 4, 'VERSION + VALUES', 'press'), (2, 0, 'BLUETOOTH', 'hold'), (2, 2, 'WIFI PASSWORD', 'hold'),
                                 (4, 0, 'FLOW OFFSET editor', 'hold'), (4, 1, 'OFFSET LIMIT editor', 'hold'), (4, 2, 'SETTLE TIME editor', 'hold'),
                                 (0, 1, 'LEARN TEST', 'hold'), (0, 0, 'CALIBRATE GAUGE', 'hold')):
        open_menu(g, item); cmd('press', 0.8); check(name); cmd(leave, 0.9)
    go_main()
    # run and error screens on a typed pressure
    while ui()['preset'] != 0: cmd('right', 0.4)
    cmd('fake 1.0', 1.5)
    if start_run():
        check('run: start measurement'); pump(3.5); check('run: pumping')
        cmd('fake 4.0', 1.5); check('error: over pressure'); cmd('press', 0.8)
    else: issues.append('%s: the run did not start' % tag)
    cmd('fake 2.3', 1.5)
    go_main(); check('main, within the band (OK)')
    cmd('fake off', 0.8); go_main()
    return W, H

print('port', PORT, 'log', LOG)
pump(0.5); cmd('dl', 0.6)                     # switch the recording on
go_main()
u0 = ui()
print('layout check, orientation as found ...')
W0, H0 = layout_pass('first')
print('  %d x %d: %d screens so far, %d issues' % (W0, H0, checked, len(issues)))
open_menu(0, 3); cmd('press', 1.2)            # SCREEN: the next orientation
go_main()
print('layout check, next orientation ...')
W1, H1 = layout_pass('second')
print('  %d x %d: %d screens, %d issues' % (W1, H1, checked, len(issues)))
open_menu(0, 3)
for _ in range(3): cmd('press', 1.0)          # back to the orientation found
go_main()

# ---------------- DEMO: LEARN INFLATE, LEARN DEFLATE ----------------
print('DEMO: learn tests ...')
n0 = len(events)
open_menu(0, 4); cmd('press', 0.8); go_main()                 # DEMO on
open_menu(0, 1); cmd('press', 0.8)                            # LEARN TEST -> pick, LEARN INFLATE first
cmd('press', 0.8)                                             # -> confirm screen
# press until the test runs, never once more: a press during the test stops it (24.11: LEARN DEFLATE stopped at 0.8 s,
# it needs no engine confirmation, so the third blind press was one too many)
if not start_run(): issues.append('LEARN INFLATE did not start')
pump(24)
cmd('press', 0.8); go_main()
open_menu(0, 1); cmd('press', 0.8); cmd('right', 0.3)         # LEARN DEFLATE
cmd('press', 0.8)
if not start_run(): issues.append('LEARN DEFLATE did not start')
pump(30)
cmd('press', 0.8); go_main()
open_menu(0, 4); cmd('press', 0.8); go_main()                 # DEMO off
learn = [e for e in events[n0:] if 'LEARN' in e]

# ---------------- CALIBRATE with fake as the gauge ----------------
print('CALIBRATE on typed pressures ...')
n1 = len(events)
cmd('fake 0', 1.2)
open_menu(0, 0); cmd('press', 0.8)                            # CAL 1/4, hose open
cmd('press', 0.6); cmd('press', 0.6)                          # engine confirm, then the zero point: the compressor starts
for target in (1.0, 2.0, 3.0):
    v = 0.3
    while v < target + 0.42:                                  # "pump": the firmware stops at point - 0.05 + offset 0.4
        v += 0.25; cmd('fake %.2f' % v, 0.25)
    pump(4.0)                                                 # settle, then the dial opens at the raw value
    cmd('press', 0.8)                                         # accept it as the gauge reading; the next point pumps
pump(1.0)
cal = [e for e in events[n1:] if 'CAL_' in e]
cmd('press', 0.8)
cmd('fake off', 0.8); go_main()
# forget what the desk taught
open_menu(1, 3); cmd('press', 0.6); cmd('press', 0.9); go_main()
st = [l for l in cmd('status', 0.8) if l.startswith('#')]

print('\n==== summary')
print('screens checked: %d (two orientations: %dx%d and %dx%d)' % (checked, W0, H0, W1, H1))
print('layout issues: %d' % len(issues))
for i in sorted(set(issues)): print('  -', i)
print('learn events:'); [print('  ', e[:200]) for e in learn]
print('calibration events:'); [print('  ', e[:200]) for e in cal]
print('status after:'); [print('  ', l) for l in st]
log.write('==== summary: screens %d, issues %d\n' % (checked, len(issues)))
for i in sorted(set(issues)): log.write('ISSUE %s\n' % i)
