"""Wrangler Air box: full assembly with the DinMeter, Mini 3A Relay, ADC v1.1, connectors and wires.

Runs pod_cq.py for the printed parts, then adds mock-ups of the bought parts and the wiring.
Writes:
  wrangler_air_box.step   every part and every wire as its own component, opens in Fusion 360
  wrangler_air_box.glb    the same for the 3D viewer

Sizes and connector positions come from M5Stack's official drawings (DinMeter K134 assembly drawing)
and 3D shells (github.com/m5stack/M5_Hardware); connector details from product photos. Measure the real parts and correct the numbers below.
Wire paths are a routing plan, not exact lengths.

Box coordinates: x along the car, y up, z from the front face (0) to the tunnel wall (32).
"""
import math
import cadquery as cq
import pod_cq as B

C = {  # colours
    "body": (0.23, 0.26, 0.31), "cover": (0.49, 0.55, 0.64),
    "din": (0.89, 0.38, 0.16), "din_body": (0.95, 0.64, 0.50), "clip": (0.94, 0.54, 0.14),
    "screen": (0.05, 0.06, 0.07), "knob": (0.85, 0.85, 0.85),
    "unit": (0.93, 0.94, 0.95), "relay_coil": (0.98, 0.55, 0.10), "vh": (0.98, 0.45, 0.15),
    "grove_plug": (0.97, 0.97, 0.97), "socket": (0.12, 0.12, 0.12), "green": (0.20, 0.65, 0.30),
    "red": (0.85, 0.12, 0.12), "black": (0.10, 0.10, 0.10), "orange": (1.00, 0.55, 0.05),
    "blue": (0.15, 0.40, 0.90), "yellow": (0.95, 0.80, 0.10), "brown": (0.50, 0.25, 0.10),
    "grove": (0.70, 0.72, 0.75), "sensor": (0.35, 0.37, 0.40), "shrink": (0.05, 0.05, 0.05),
}
TOP_Y = B.H / 2
FLOOR = -B.H / 2 + B.T


def box(sx, sy, sz, cx, cy, cz):
    return cq.Workplane("XY").box(sx, sy, sz).translate((cx, cy, cz))


def tube(pts, r):
    """Wire along a polyline: cylinders on the segments, spheres in the corners."""
    solids = []
    for a, b in zip(pts, pts[1:]):
        va, vb = cq.Vector(*a), cq.Vector(*b)
        d = vb - va
        if d.Length < 1e-6:
            continue
        solids.append(cq.Solid.makeCylinder(r, d.Length, va, d.normalized()))
    for p in pts[1:-1]:
        solids.append(cq.Solid.makeSphere(r, cq.Vector(*p), angleDegrees1=-90, angleDegrees2=90))
    return cq.Workplane("XY").add(cq.Compound.makeCompound(solids))


def sleeve(p, axis, length=10.0, r=2.3):
    v = cq.Vector(*axis).normalized()
    start = cq.Vector(*p) - v * (length / 2)
    return cq.Workplane("XY").add(cq.Solid.makeCylinder(r, length, start, v))


asm = cq.Assembly(name="wrangler_air_box")
asm.add(B.body, name="box_body", color=cq.Color(*C["body"]))
asm.add(B.cover, name="box_cover", loc=cq.Location((0, 0, B.D + B.P), (0, 1, 0), 180), color=cq.Color(*C["cover"]))

# ---------- DinMeter (screen up), sizes from the official K134 drawing ----------
# Mounted turned 180 deg (knob at +x, ports at -x, next to the cable entry); the screen is turned in SCREEN menu.
cz = B.CUT_Z
din = cq.Assembly(name="dinmeter")
din.add(box(53, 2.5, 30, 0, TOP_Y + 1.25, cz).edges("|Y").fillet(4), name="bezel", color=cq.Color(*C["din"]))
din.add(box(26.5, 0.3, 17.4, -5.5, TOP_Y + 2.6, cz), name="screen", color=cq.Color(*C["screen"]))
din.add(cq.Workplane("XZ").workplane(offset=-(TOP_Y + 2.5)).center(14.25, cz).circle(6).extrude(-8.6), name="knob", color=cq.Color(*C["knob"]))
din.add(box(45.6, 20.4, 22.2, 0, TOP_Y - 10.2, cz), name="housing", color=cq.Color(*C["din_body"]))
clip = box(49, 8, 26.4, 0, TOP_Y - B.TOP - 4, cz).cut(box(45.7, 9, 22.3, 0, TOP_Y - B.TOP - 4, cz))
din.add(clip, name="back_clip", color=cq.Color(*C["clip"]))
DB = TOP_Y - 20.4                                  # DinMeter back face (bottom)
PA, PB = (-11.7, DB, cz - 3.0), (-18.0, DB, cz - 3.0)   # PORT.A, PORT.B (HY2.0-4P, plug from below)
TERM = (-15.1, DB, cz + 7.6)                       # green 6-36 V terminal
din.add(box(4.6, 8, 9.9, PA[0], DB - 4, PA[2]), name="grove_plug_port_A", color=cq.Color(*C["grove_plug"]))
din.add(box(4.6, 8, 9.9, PB[0], DB - 4, PB[2]), name="grove_plug_port_B", color=cq.Color(*C["grove_plug"]))
din.add(box(8.8, 10, 7, TERM[0], DB - 5, TERM[2]), name="power_terminal_plug", color=cq.Color(*C["green"]))
asm.add(din)
T_PLUS, T_MINUS = (TERM[0] + 1.3, DB - 10, TERM[2]), (TERM[0] - 1.3, DB - 10, TERM[2])

# ---------- Unit Relay U023 (flat on the floor), terminal end at -x ----------
rx, rz = B.RELAY_X, B.RELAY_Z
rl, rh, rw = B.RELAY
r_x0, r_x1 = rx - rl / 2, rx + rl / 2
ry = FLOOR + rh / 2
rel = cq.Assembly(name="mini_relay")
rel.add(box(rl, rh, rw, rx, ry, rz).edges("|Y").fillet(2), name="case", color=cq.Color(*C["unit"]))
rel.add(box(15.2, 6, 10.5, r_x0 + 17.6, FLOOR + rh + 3, rz), name="relay", color=cq.Color(*C["relay_coil"]))
VH = (r_x0 + 5, FLOOR + rh + 6, rz)                # VH3.96 plug on the header: COM, NO, NC; wires out to -x
rel.add(box(10, 12, 12, *VH), name="terminal_COM_NO_NC", color=cq.Color(*C["vh"]))
rel.add(box(6.5, 4.6, 9.9, r_x1 + 3.25, ry, rz), name="grove_plug", color=cq.Color(*C["grove_plug"]))
asm.add(rel)
R_COM, R_NO = (r_x0, VH[1], rz - 4), (r_x0, VH[1], rz)

# ---------- Unit ADC v1.1 (on its long edge against the front wall), terminal end at +x ----------
ax, ay = B.ADC_X, B.ADC_Y
al, ah, at = B.ADC
az = B.T + at / 2
a_x0, a_x1 = ax - al / 2, ax + al / 2
adc = cq.Assembly(name="adc_v11")
adc.add(box(al, ah, at, ax, ay, az).edges("|Z").fillet(2), name="case", color=cq.Color(*C["unit"]))
AT = (a_x1 - 4.5, ay, B.T + at + 5.5)              # 3.96 terminal on the back face: IN+, GND; wires out to +x
adc.add(box(9, 8, 11, *AT), name="terminal_IN_GND", color=cq.Color(*C["vh"]))
adc.add(box(6.5, 9.9, 4.6, a_x0 - 3.25, ay, az), name="grove_plug", color=cq.Color(*C["grove_plug"]))
asm.add(adc)
A_IN, A_GND = (a_x1, ay + 2, AT[2]), (a_x1, ay - 2, AT[2])

# ---------- wires ----------
xw = -B.W / 2 + B.T            # inside of the left wall
SL = (-B.W / 2, B.EXIT_Y, B.EXIT_Z)
out_x = SL[0] - 9
out_y = -B.H / 2 - 22          # outside: on down to the car harness
SP12 = (-25.0, 3.0, 24.0)      # +12 V splice
SPG = (-24.0, 2.0, 18.0)       # GND splice
BRK = (-21.0, -15.0, 26.0)     # sensor cable breakout
SPS = (-18.5, -4.5, 8.0)       # sensor +5 V spliced into the PORT.A Grove red wire


def wire(name, pts, col, r):
    asm.add(tube(pts, r), name=name, color=cq.Color(*C[col]))


def entry(dz, inside):
    return [(out_x, out_y, SL[2] + dz), (out_x, SL[1] - 3, SL[2] + dz), (SL[0], SL[1], SL[2] + dz)] + inside

wire("car_12V_AUX1_red_1.5", entry(-2.5, [(-30.5, -13, 13.5), (-28.5, -6, 19), (SP12[0], SP12[1] - 5, SP12[2])]), "red", 1.3)
wire("car_GND_black_1.5", entry(2.5, [(-30.5, -13, 18.5), (-28.5, -8, 17), (SPG[0], SPG[1] - 5, SPG[2])]), "black", 1.3)
wire("to_K2_86_via_D1_brown_1.5", [R_NO, (-30.5, R_NO[1], R_NO[2]), (SL[0], SL[1], SL[2]), (out_x, SL[1] - 3, SL[2] - 5), (out_x, out_y, SL[2] - 5)], "brown", 1.3)
wire("sensor_cable_4core", entry(0, [(-30.5, -14, 22), (-26, -15, 26), BRK]), "sensor", 2.0)
wire("12V_to_relay_COM_red", [(SP12[0], SP12[1] - 5, SP12[2]), (-29, -6, 18), (-29.5, R_COM[1], R_COM[2]), R_COM], "red", 1.0)
wire("12V_to_DinMeter_plus_orange_0.25", [(SP12[0], SP12[1] + 5, SP12[2]), (-23.5, -3, 27), (T_PLUS[0], -3, 27), (T_PLUS[0], -3, T_PLUS[2]), T_PLUS], "orange", 0.6)
wire("GND_to_DinMeter_minus_blue_0.25", [(SPG[0], SPG[1] + 5, SPG[2]), (-23, -4, 20.5), (T_MINUS[0], -4, 20.5), (T_MINUS[0], -4, T_MINUS[2]), T_MINUS], "blue", 0.6)
wire("sensor_SIG_yellow", [BRK, (24.5, -15, 26), (25.5, A_IN[1], A_IN[2]), A_IN], "yellow", 0.6)
wire("sensor_GND_black", [BRK, (24, -16, 27), (26.5, A_GND[1], A_GND[2]), A_GND], "black", 0.6)
wire("sensor_5V_red", [BRK, (-20, -5, 26), (-20, -5, SPS[2]), (SPS[0], SPS[1], SPS[2])], "red", 0.6)
wire("grove_PORT_A_to_ADC", [(PA[0], DB - 8, PA[2]), (PA[0], -1.5, PA[2]), (-18.5, -3, 10), (-18.5, ay, az), (a_x0 - 6.5, ay, az)], "grove", 1.2)
wire("grove_PORT_B_to_relay", [(PB[0], DB - 8, PB[2]), (PB[0], -8, PB[2]), (PB[0], -8, 25), (30, -8, 25), (30, ry, 25), (30, ry, rz), (r_x1 + 6.5, ry, rz)], "grove", 1.2)
asm.add(sleeve(SP12, (0, 1, 0)), name="shrink_12V_splice", color=cq.Color(*C["shrink"]))
asm.add(sleeve(SPG, (0, 1, 0)), name="shrink_GND_splice", color=cq.Color(*C["shrink"]))
asm.add(sleeve(SPS, (0, 1, 0), 7, 1.9), name="shrink_sensor_5V_splice", color=cq.Color(*C["shrink"]))
asm.add(sleeve((BRK[0] - 3, BRK[1], BRK[2]), (1, 0, 0), 8, 2.6), name="shrink_sensor_breakout", color=cq.Color(*C["shrink"]))

asm.save("wrangler_air_box.step")
asm.save("wrangler_air_box.glb", tolerance=0.25, angularTolerance=0.4)
print("ok")
