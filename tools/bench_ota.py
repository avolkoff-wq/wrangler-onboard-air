"""Wi-Fi update test (bench U12), run as one command because the Mac loses its internet while on the DinMeter AP.
DinMeter: menu CONNECT > WIFI UPDATE + LOG over USB; Mac joins WranglerAir, fetches / and /log, uploads the firmware to
/update, watches the reboot on USB, then waits until the Mac is back on the home network."""
import datetime, glob, re, subprocess, sys, time
import serial

REPO = __import__('os').path.dirname(__import__('os').path.dirname(__import__('os').path.abspath(__file__)))
BIN = REPO + '/firmware/bin/wrangler_air.bin'
LOG = open(REPO + '/logs/bench_ota_%s.txt' % datetime.date.today().strftime('%Y%m%d'), 'a', buffering=1)
PORT = [p for p in glob.glob('/dev/cu.usbmodem*') if 'RFCY' not in p][0]
s = serial.Serial(PORT, 115200, timeout=0.05)
buf = b''

def say(t):
    line = '%s # %s' % (datetime.datetime.now().strftime('%H:%M:%S'), t)
    print(line, flush=True); LOG.write(line + '\n')

def pump(sec):
    global buf
    out = []; end = time.time() + sec
    while time.time() < end:
        try: d = s.read(4096)
        except Exception: d = b''
        if not d: continue
        buf += d
        while b'\n' in buf:
            l, buf = buf.split(b'\n', 1); l = l.decode('utf-8', 'replace').rstrip('\r')
            LOG.write('%s < %s\n' % (datetime.datetime.now().strftime('%H:%M:%S.%f')[:-3], l)); out.append(l)
    return out

def cmd(c, wait=0.5):
    LOG.write('%s > %s\n' % (datetime.datetime.now().strftime('%H:%M:%S.%f')[:-3], c))
    s.write((c + '\n').encode()); s.flush(); return pump(wait)

def ui():
    for _ in range(4):
        for l in cmd('ui', 0.3):
            if l.startswith('UI,'):
                f = l.split(','); return int(f[2]), int(f[3]), int(f[5])
    raise RuntimeError('no UI')

def sh(*a, timeout=60):
    r = subprocess.run(a, capture_output=True, text=True, timeout=timeout)
    return r.returncode, (r.stdout + r.stderr).strip()

home = sh('networksetup', '-listpreferredwirelessnetworks', 'en0')[1].splitlines()[1].strip()
result = {}
try:
    # main screen, then menu CONNECT (top row 4 since 0.8.0) > WIFI UPDATE + LOG (row 1)
    for _ in range(6):
        scr, pos, st = ui()
        if scr == 0 and st == 0: break
        cmd('press' if st in (7, 8) else 'hold', 0.9)
    boot0 = [l for l in cmd('status', 0.8) if 'boot' in l]
    cmd('hold', 0.9)
    scr, pos, _ = ui(); assert scr == 1, 'menu did not open'
    for _ in range(4): cmd('right', 0.3)
    cmd('press', 0.6)
    scr, pos, _ = ui(); assert scr == 1 and pos == 0, 'not in CONNECT'
    cmd('right', 0.3); cmd('press', 2.0)
    scr, _, _ = ui(); assert scr == 4, 'WIFI UPDATE screen did not open (screen %d)' % scr
    texts = [l.split(',', 7)[7] for l in cmd('dl', 0.6) if l.startswith('DLT,')]
    pw = next((t[5:].strip() for t in texts if t.startswith('pass ')), 'jeepjeep')
    say('DinMeter in UPDATE MODE, AP WranglerAir up')
    time.sleep(3)
    t0 = time.time()
    rc, out = sh('networksetup', '-setairportnetwork', 'en0', 'WranglerAir', pw, timeout=40)
    say('Mac joins WranglerAir: rc %d %s' % (rc, out[:80]))
    ip = ''
    for _ in range(30):
        ip = sh('ipconfig', 'getifaddr', 'en0')[1]
        if ip.startswith('192.168.4.'): break
        time.sleep(1)
    say('Mac address %s after %.0f s' % (ip, time.time() - t0)); result['join_s'] = round(time.time() - t0)
    rc, page = sh('curl', '-s', '-m', '10', 'http://192.168.4.1/')
    result['page'] = 'Update' in page; say('page /: %s (%d bytes)' % ('ok' if result['page'] else 'FAILED', len(page)))
    rc, lg = sh('curl', '-s', '-m', '20', '-o', '/dev/null', '-w', '%{http_code} %{size_download}', 'http://192.168.4.1/log')
    result['log'] = lg; say('log download: http %s bytes' % lg)
    t1 = time.time()
    rc, up = sh('curl', '-s', '-m', '180', '-F', 'fw=@' + BIN, 'http://192.168.4.1/update', timeout=200)
    result['upload'] = up; result['upload_s'] = round(time.time() - t1, 1)
    say('upload %s: "%s" in %.1f s' % (BIN.split('/')[-1], up, time.time() - t1))
    # the DinMeter reboots: USB re-enumerates
    s.close(); time.sleep(8)
    for _ in range(20):
        ports = [p for p in glob.glob('/dev/cu.usbmodem*') if 'RFCY' not in p]
        if ports:
            try: s = serial.Serial(ports[0], 115200, timeout=0.05); break
            except Exception: pass
        time.sleep(1)
    pump(2)
    st = [l for l in cmd('status', 1.0) if l.startswith('#')]
    result['status'] = st
    for l in st: say('after: ' + l)
    say('boot before: %s' % (boot0[0] if boot0 else '?'))
finally:
    # back home: the AP is gone after the reboot, macOS rejoins; push it if it does not
    t2 = time.time(); back = False
    for k in range(60):
        rc, _ = sh('ping', '-c', '1', '-t', '1', '192.168.1.1', timeout=5)
        if rc == 0: back = True; break
        if k == 15: sh('networksetup', '-setairportnetwork', 'en0', home, timeout=40)
        time.sleep(1)
    say('Mac back on the home network: %s after %.0f s' % ('yes' if back else 'NO', time.time() - t2))
    print('RESULT', result, flush=True)
