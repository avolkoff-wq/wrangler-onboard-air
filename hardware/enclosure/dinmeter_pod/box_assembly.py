"""Wrangler Air box: full assembly with the DinMeter, Mini 3A Relay, ADC v1.1, connectors and wires.

Runs pod_cq.py for the printed parts, then adds mock-ups of the bought parts and the wiring.
Writes:
  wrangler_air_box.step   every part and every wire as its own component, opens in Fusion 360
  wrangler_air_box.glb    the same for the 3D viewer

Mock-up sizes come from the M5Stack docs and product photos; connector positions on the DinMeter
back are taken from its photo. Measure the real parts and correct the numbers below.
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

# ---------- DinMeter (screen up) ----------
cz = B.CUT_Z
din = cq.Assembly(name="dinmeter")
din.add(box(53, 5, 30, 0, TOP_Y + 2.5, cz), name="bezel", color=cq.Color(*C["din"]))
din.add(box(26, 0.3, 15, 9, TOP_Y + 5.15, cz + 1), name="screen", color=cq.Color(*C["screen"]))
din.add(cq.Workplane("XZ").workplane(offset=-(TOP_Y + 5)).center(-15, cz).circle(7).extrude(-7), name="knob", color=cq.Color(*C["knob"]))
din.add(box(44.5, 27, 22, 0, TOP_Y - 13.5, cz), name="housing", color=cq.Color(*C["din_body"]))
clip = box(49, 8, 26.4, 0, TOP_Y - B.TOP - 4, cz).cut(box(44.6, 9, 22.1, 0, TOP_Y - B.TOP - 4, cz))
din.add(clip, name="back_clip", color=cq.Color(*C["clip"]))
DB = TOP_Y - 27                                   # DinMeter back face (bottom)
PA, PB = (9.5, DB, cz + 2), (16.5, DB, cz + 2)    # PORT.A, PORT.B
TERM = (14.0, DB, cz - 7)                         # green 6-36 V terminal
din.add(box(5, 8, 9.5, PA[0], DB - 4, PA[2]), name="grove_plug_port_A", color=cq.Color(*C["grove_plug"]))
din.add(box(5, 8, 9.5, PB[0], DB - 4, PB[2]), name="grove_plug_port_B", color=cq.Color(*C["grove_plug"]))
din.add(box(7.6, 10, 6, TERM[0], DB - 5, TERM[2]), name="power_terminal_plug", color=cq.Color(*C["green"]))
asm.add(din)

# ---------- Mini 3A Relay (flat on the floor) ----------
rx, rz = B.RELAY_X, B.RELAY_Z
rl, rh, rw = B.RELAY
r_x0, r_x1 = rx - rl / 2, rx + rl / 2
ry = FLOOR + rh / 2
rel = cq.Assembly(name="mini_relay")
rel.add(box(rl, rh, rw, rx, ry, rz), name="case", color=cq.Color(*C["unit"]))
rel.add(box(15.2, 7, 10.5, r_x0 + 21.5, FLOOR + rh + 3.5, rz), name="relay", color=cq.Color(*C["relay_coil"]))
VH = (r_x0 + 5.5, FLOOR + rh + 5.5, rz)            # 3.96 terminal plug on top, wires out to -x
rel.add(box(9, 11, 12, *VH), name="terminal_COM_NO_NC", color=cq.Color(*C["vh"]))
rel.add(box(6.5, 4.6, 9, r_x1 + 3.25, ry, rz), name="grove_plug", color=cq.Color(*C["grove_plug"]))
asm.add(rel)
R_COM = (r_x0 + 1, VH[1], rz - 4)
R_NO = (r_x0 + 1, VH[1], rz)

# ---------- ADC v1.1 (on its long edge against the front wall) ----------
ax, ay = B.ADC_X, B.ADC_Y
al, ah, at = B.ADC
az = B.T + at / 2
a_x0, a_x1 = ax - al / 2, ax + al / 2
adc = cq.Assembly(name="adc_v11")
adc.add(box(al, ah, at, ax, ay, az), name="case", color=cq.Color(*C["unit"]))
AT = (a_x0 + 5.5, ay, B.T + at + 5.5)              # 2-pin terminal on the back face, wires out to -x
adc.add(box(9, 8, 11, *AT), name="terminal_IN_GND", color=cq.Color(*C["vh"]))
adc.add(box(6.5, 9, 4.6, a_x1 + 3.25, ay, az), name="grove_plug", color=cq.Color(*C["grove_plug"]))
asm.add(adc)
A_IN = (a_x0 + 1, ay + 2, AT[2])
A_GND = (a_x0 + 1, ay - 2, AT[2])

# ---------- wires ----------
xw = -B.W / 2 + B.T            # inside of the left wall
SL = (-B.W / 2 - 1, B.EXIT_Y, B.EXIT_Z)   # slot centre, outside
out_y = -B.H / 2 - 25          # wires run on down to the car harness
xl = xw + 3.0                  # vertical run along the left wall, past the cable-tie anchor
SP12 = (xl + 1, 1.0, 26.0)     # +12 V splice
SPG = (xl + 1, 4.0, 29.5)      # GND splice
SPS = (25.0, ay, 18.0)         # sensor +5 V spliced into the PORT.A Grove cable
BRK = (xl, ay - 9, 21.0)       # sensor cable breakout


def wire(name, pts, col, r):
    asm.add(tube(pts, r), name=name, color=cq.Color(*C[col]))


def out(dz):                   # entry path from the harness through the slot
    return [(SL[0] - 10, out_y, SL[2] + dz), (SL[0] - 10, SL[1] - 2, SL[2] + dz), (SL[0], SL[1], SL[2] + dz), (xl, SL[1] + 3, SL[2] + dz)]

# car side: +12 V, GND, wire to K2, sensor cable
wire("car_12V_AUX1_red_1.5", out(-2.5) + [(xl, 0, 22), (SP12[0], SP12[1] - 5, SP12[2])], "red", 1.3)
wire("car_GND_black_1.5", out(2.5) + [(xl + 1, -2, 29.5), (SPG[0], SPG[1] - 5, SPG[2])], "black", 1.3)
wire("to_K2_86_via_D1_brown_1.5", [R_NO, (xw + 1.5, R_NO[1], R_NO[2]), (xw + 1.5, SL[1], SL[2]), (SL[0], SL[1], SL[2]), (SL[0] - 10, SL[1] - 4, SL[2]), (SL[0] - 10, out_y, SL[2])], "brown", 1.3)
wire("sensor_cable_4core", out(0)[:-1] + [(xl, SL[1] + 3, SL[2]), BRK], "sensor", 2.0)
# +12 V splice: car +12 -> relay COM (red) and DinMeter + (orange)
wire("12V_to_relay_COM_red", [(SP12[0], SP12[1] - 5, SP12[2]), (xl + 1, -16, 26), (xw + 1.5, R_COM[1], 26), (xw + 1.5, R_COM[1], R_COM[2]), R_COM], "red", 1.0)
wire("12V_to_DinMeter_plus_orange_0.25", [(SP12[0], SP12[1] + 5, SP12[2]), (SP12[0], 0.5, 24), (TERM[0] - 1.7, 0.5, 24), (TERM[0] - 1.7, 0.5, TERM[2]), (TERM[0] - 1.7, DB - 10, TERM[2])], "orange", 0.6)
wire("GND_to_DinMeter_minus_blue_0.25", [(SPG[0], SPG[1] + 5, SPG[2]), (SPG[0], 2.0, 29.5), (TERM[0] + 1.7, 2.0, 29.5), (TERM[0] + 1.7, 2.0, TERM[2]), (TERM[0] + 1.7, DB - 10, TERM[2])], "blue", 0.6)
# sensor breakout: SIG and GND to the ADC terminal, +5 V up to the Grove splice
wire("sensor_SIG_yellow", [BRK, (xl, A_IN[1], 21), (A_IN[0] - 1, A_IN[1], A_IN[2]), A_IN], "yellow", 0.6)
wire("sensor_GND_black", [BRK, (xl + 1.2, A_GND[1], 20), (A_GND[0] - 1, A_GND[1], A_GND[2]), A_GND], "black", 0.6)
wire("sensor_5V_red", [BRK, (xl + 2, ay - 9, 29.5), (SPS[0], ay - 9, 29.5), (SPS[0], SPS[1] - 1, SPS[2] + 5)], "red", 0.6)
# Grove cables
wire("grove_PORT_A_to_ADC", [(PA[0], DB - 8, PA[2]), (PA[0], DB - 10, 22), (25, DB - 10, 22), (25, ay, 18), (25, ay, az), (a_x1 + 6.5, ay, az)], "grove", 1.2)
wire("grove_PORT_B_to_relay", [(PB[0], DB - 8, PB[2]), (PB[0], DB - 11, 26), (28.6, DB - 11, 26), (28.6, ry, 26), (28.6, ry, rz), (r_x1 + 6.5, ry, rz)], "grove", 1.2)
# heat-shrink over the splices
asm.add(sleeve(SP12, (0, 1, 0)), name="shrink_12V_splice", color=cq.Color(*C["shrink"]))
asm.add(sleeve(SPG, (0, 1, 0)), name="shrink_GND_splice", color=cq.Color(*C["shrink"]))
asm.add(sleeve(SPS, (0, 0, 1), 8, 1.9), name="shrink_sensor_5V_splice", color=cq.Color(*C["shrink"]))
asm.add(sleeve((xl, BRK[1] - 3, BRK[2]), (0, 1, 0), 8, 2.6), name="shrink_sensor_breakout", color=cq.Color(*C["shrink"]))

asm.save("wrangler_air_box.step")
asm.save("wrangler_air_box.glb", tolerance=0.25, angularTolerance=0.4)
print("ok")
