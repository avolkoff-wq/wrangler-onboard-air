#!/usr/bin/env python3
"""Wrangler Air bench: save the DinMeter USB live stream to CSV and optionally plot it.

Needs firmware 0.5.0+ (bin/wrangler_air.bin, USB is always on; menu DEMO for the desk) and pyserial (pip install pyserial;
--plot also needs matplotlib).

Usage:
  python3 tools/serial_log.py /dev/ttyACM0 run.csv            # Linux; macOS: /dev/cu.usbmodem*; Windows: COM5
  python3 tools/serial_log.py /dev/ttyACM0 run.csv --plot     # after Ctrl+C draw pressure, target, compressor / valve bands
  python3 tools/serial_log.py --plot-only run.csv             # draw a saved CSV again
  python3 tools/serial_log.py /dev/ttyACM0 run.csv --cmd "pulse k1"   # send one command first (status, log, runs, run <id>, clear, pulse k1, pulse valve, help)

Every line from the device is echoed to the terminal. LIVE lines (twice per second during runs, LEARN and OUTPUT TEST)
go to the CSV: t_s, bar, est, phase, k1, valve, target. The target comes from the last RUN line of the log.
Stop with Ctrl+C.
"""
import argparse
import csv
import re
import sys
import time

COLS = ["t_s", "bar", "est", "phase", "k1", "valve", "target"]


def record(port, out, cmds):
    import serial  # pyserial
    target = ""
    rows = 0
    with serial.Serial(port, 115200, timeout=0.5) as ser, open(out, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(COLS)
        time.sleep(0.3)
        for c in cmds:
            ser.write((c + "\n").encode())
        print(f"recording {port} -> {out}, Ctrl+C to stop", file=sys.stderr)
        try:
            while True:
                line = ser.readline().decode(errors="replace").strip()
                if not line:
                    continue
                print(line)
                if line.startswith("LIVE,"):
                    p = line.split(",")
                    if len(p) >= 7:
                        w.writerow(p[1:7] + [target])
                        rows += 1
                        if rows % 20 == 0:
                            f.flush()
                    continue
                m = re.search(r",RUN,.*target=([0-9.]+)", line)
                if m:
                    target = m.group(1)
        except KeyboardInterrupt:
            pass
    print(f"{rows} live rows saved to {out}", file=sys.stderr)


def plot(path):
    import matplotlib.pyplot as plt
    t, bar, est, k1, valve, target = [], [], [], [], [], []
    with open(path) as f:
        for r in csv.DictReader(f):
            t.append(float(r["t_s"]))
            bar.append(float(r["bar"]))
            est.append(float(r["est"]))
            k1.append(r["k1"] == "1")
            valve.append(r["valve"] == "1")
            target.append(float(r["target"]) if r["target"] else float("nan"))
    if not t:
        print("no LIVE rows in", path, file=sys.stderr)
        return
    fig, ax = plt.subplots(figsize=(11, 5))
    tr = ax.get_xaxis_transform()
    ax.fill_between(t, 0, 1, where=k1, transform=tr, color="#ffd9a8", step="post", label="compressor")
    ax.fill_between(t, 0, 1, where=valve, transform=tr, color="#cfe6fb", step="post", label="valve open")
    ax.plot(t, est, color="#999", lw=1, label="estimate")
    ax.plot(t, bar, color="#1a4fa0", lw=2, label="sensor")
    ax.plot(t, target, color="#c00", ls="--", lw=1.5, label="target")
    ax.set_xlabel("time since power-up, s")
    ax.set_ylabel("bar")
    ax.grid(alpha=0.3)
    ax.legend(loc="best")
    ax.set_title(path)
    plt.tight_layout()
    plt.show()


def main():
    a = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    a.add_argument("port", nargs="?", help="serial port of the DinMeter")
    a.add_argument("out", nargs="?", help="CSV file to write (or to plot with --plot-only)")
    a.add_argument("--plot", action="store_true", help="plot after Ctrl+C")
    a.add_argument("--plot-only", action="store_true", help="only plot an existing CSV")
    a.add_argument("--cmd", action="append", default=[], help="command to send at the start (repeatable)")
    o = a.parse_args()
    if o.plot_only and not o.out:
        o.out, o.port = o.port, None
    if not o.out:
        a.error("out.csv is required")
    if not o.plot_only:
        if not o.port:
            a.error("port is required unless --plot-only")
        record(o.port, o.out, o.cmd)
    if o.plot or o.plot_only:
        plot(o.out)


if __name__ == "__main__":
    main()
