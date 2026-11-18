"""Wrangler Air in a stock Hammond 1591 ABS box, DinMeter in the short end wall.

Two boxes, picked with the environment variable WA_BOX (default 1591s):
  1591s  Hammond 1591SBK, 110.1 x 82.2 x 44, drawing 1591S REV 06.16.2023
  1591c  Hammond 1591CBK, 120 x 65 x 40, drawing 1591C REV 06.16.2023. The end face is close to the DinMeter bezel, so the
         two lid bosses at the DinMeter end are cut down below BOSS_CUT_Z (they keep their top with the brass insert),
         and the DinMeter sits low in the end wall so its back clip passes under what is left of the bosses.

The box stands upright with its lid against the tunnel wall. The DinMeter goes into the UPPER END WALL (the short side), screen up. Relay and ADC lie flat on the box bottom (the face towards the driver), Grove plugs towards the DinMeter. Car wires enter through
a slot in the lower end wall.
Axes: x along the car (box width 82.2), y up (box length 110.1), z from the box bottom (0) to the lid (44).

Prints collisions and clearances; cables_box.py adds the cables and writes the STEP and GLB.
"""
import os
import cadquery as cq

TAG = os.environ.get("WA_BOX", "1591s")
PCB_BOSS = None
GLANDS = None          # (x, z) of round M12 cable gland holes in the lower end wall, instead of the slot
GLAND_D = 12.2
if TAG == "f100":
    # AliExpress IP65 box F-100x68x50, black ABS base 36 + clear PC lid 14 (seller drawing). Inner sizes, wall, floor and
    # the corner posts are measured off the seller drawing (scale 0.35 mm/px): to be checked on the real box.
    W, L, HB, HL = 68.0, 100.0, 36.0, 14.0
    IN_W, IN_L, FLOOR = 63.0, 95.0, 2.5
    BOSS_X, BOSS_Y, BOSS_D, BOSS_HOLE = 57.6, 88.2, 7.7, 3.0     # corner posts with brass inserts for the lid screws
    RIB_D = None
    PCB_BOSS = (43.0, 75.0, 6.0, 5.0)                            # floor PCB bosses: pitch x, pitch y, dia, height (estimated)
    DIN_X, DIN_Z = 0.0, 19.0          # housing bottom at 7.9 clears the floor PCB bosses near the DinMeter end
    BOSS_CUT_Z = None
    SLOT_X, SLOT_Z = 8.0, 18.0        # kept for cables_box.py; the box itself gets two M12 glands
    GLANDS = ((-12.0, 18.0), (12.0, 18.0))   # left: power (BK + 2x RD), right: sensor + ARB (OG, BU, YE, VT)
    RXC, RY0, AXC, AY0 = -14.5, -34.0, 15.5, -18.0
    CH_X = 0.0
elif TAG == "1591c":
    W, L, HB, HL = 65.0, 120.0, 36.0, 4.0
    IN_W, IN_L, FLOOR = 58.53, 113.53, 2.0
    BOSS_X, BOSS_Y, BOSS_D, BOSS_HOLE = 42.35, 97.35, 7.6, 2.5
    RIB_D, RIB_W, RIB_H, RIB_PITCH = 1.0, 3.0, 30.9, 6.0
    DIN_X, DIN_Z = 0.0, 15.6          # low: the back clip (26.4 high) ends at z 28.8, under the boss stubs
    BOSS_CUT_Z = 29.5                 # the two bosses at the DinMeter end are cut away from the floor up to this height
    SLOT_X, SLOT_Z = -8.0, 18.0
    RXC, RY0, AXC, AY0 = -15.5, -32.5, 15.5, -18.0
    CH_X = 0.0                        # wire channel between relay and ADC
else:
    W, L, HB, HL = 82.2, 110.1, 40.0, 4.0       # outside width (x), length (y), body height, lid
    IN_W, IN_L, FLOOR = 75.87, 103.77, 2.0
    BOSS_X, BOSS_Y, BOSS_D, BOSS_HOLE = 60.4, 88.3, 7.6, 2.5   # boss centres inside the box (boss dia estimated)
    RIB_D, RIB_W, RIB_H, RIB_PITCH = 1.0, 3.0, 34.9, 6.0      # card guides on the long walls (pitch estimated)
    DIN_X, DIN_Z = 0.0, 20.0
    BOSS_CUT_Z = None
    SLOT_X, SLOT_Z = -16.0, 20.0
    RXC, RY0, AXC, AY0 = -23.5, -36.0, 18.0, -22.0
    CH_X = -7.0
NAME = f"hammond_{TAG}"
X_IN, Y_IN = IN_W / 2, IN_L / 2
TOP_Y = L / 2

box = (cq.Workplane("XY").rect(W, L).extrude(HB).edges("|Z").fillet(5.0).faces("<Z").edges().fillet(1.0))
box = box.cut(cq.Workplane("XY").workplane(offset=FLOOR).rect(IN_W, IN_L).extrude(HB).edges("|Z").fillet(2.0))
boss_cut = None
for sx in (-1, 1):
    for sy in (-1, 1):
        c = (sx * BOSS_X / 2, sy * BOSS_Y / 2)
        b = (cq.Workplane("XY").workplane(offset=FLOOR).center(*c).circle(BOSS_D / 2).extrude(HB - FLOOR)
             .union(cq.Workplane("XY").workplane(offset=FLOOR).center(c[0] + sx * 4, c[1] + sy * 4).rect(6, 6).extrude(HB - FLOOR)))
        if BOSS_CUT_Z and sy > 0:      # DinMeter end: the part below BOSS_CUT_Z is cut off (shown red in the model)
            low = cq.Workplane("XY").box(20, 20, BOSS_CUT_Z - FLOOR).translate((c[0], c[1], (BOSS_CUT_Z + FLOOR) / 2))
            part = b.intersect(low)
            part = part.cut(cq.Workplane("XY").workplane(offset=0).rect(W, L).extrude(HB).cut(cq.Workplane("XY").rect(IN_W, IN_L).extrude(HB)))
            boss_cut = part if boss_cut is None else boss_cut.union(part)
            b = b.cut(low)
        box = box.union(b)
        box = box.cut(cq.Workplane("XY").workplane(offset=HB - 12).center(*c).circle(BOSS_HOLE / 2).extrude(12))
n = int((IN_L - 24) / RIB_PITCH) if RIB_D else -1
for i in range(n + 1):
    y = -(n * RIB_PITCH) / 2 + i * RIB_PITCH
    for sx in (-1, 1):
        box = box.union(cq.Workplane("XY").box(RIB_D, RIB_W, RIB_H).translate((sx * (X_IN - RIB_D / 2), y, HB - RIB_H / 2)))
if PCB_BOSS:
    for sx in (-1, 1):
        for sy in (-1, 1):
            box = box.union(cq.Workplane("XY").workplane(offset=FLOOR).center(sx * PCB_BOSS[0] / 2, sy * PCB_BOSS[1] / 2)
                            .circle(PCB_BOSS[2] / 2).circle(1.2).extrude(PCB_BOSS[3]))
lid = cq.Workplane("XY").rect(W, L).extrude(HL).edges("|Z").fillet(5.0).faces(">Z").edges().fillet(1.0).translate((0, 0, HB))
if TAG == "f100":    # clear lid is a 14 mm deep shell: the space above the units is usable
    lid = lid.cut(cq.Workplane("XY").workplane(offset=HB).rect(IN_W, IN_L).extrude(HL - 2.5).edges("|Z").fillet(2.0))

# ---------- DinMeter in the upper end wall ----------
CUT_W, CUT_H = 46.0, 22.8
box = box.cut(cq.Workplane("XY").box(CUT_W, 10, CUT_H).translate((DIN_X, TOP_Y - 2, DIN_Z)))

# ---------- wire slot in the lower end wall ----------
SLOT_L, SLOT_H = 14.0, 8.0
if GLANDS:
    for gx, gz in GLANDS:
        box = box.cut(cq.Workplane("XZ").workplane(offset=TOP_Y - 5).center(gx, gz).circle(GLAND_D / 2).extrude(8))
else:
    box = box.cut(cq.Workplane("XZ").workplane(offset=TOP_Y - 5).center(SLOT_X, SLOT_Z).slot2D(SLOT_L, SLOT_H, 0).extrude(8))

def bx(sx, sy, sz, cx, cy, cz):
    return cq.Workplane("XY").box(sx, sy, sz).translate((cx, cy, cz))

din = cq.Assembly(name="dinmeter")
din.add(bx(53, 2.5, 30, DIN_X, TOP_Y + 1.25, DIN_Z).edges("|Y").fillet(4), name="bezel", color=cq.Color(0.89, 0.38, 0.16))
din.add(bx(26.5, 0.3, 17.4, DIN_X - 5.5, TOP_Y + 2.6, DIN_Z), name="screen", color=cq.Color(0.05, 0.06, 0.07))
din.add(cq.Workplane("XZ").workplane(offset=-(TOP_Y + 2.5)).center(DIN_X + 14.25, DIN_Z).circle(6).extrude(-8.6), name="knob", color=cq.Color(0.85, 0.85, 0.85))
din.add(bx(45.6, 20.4, 22.2, DIN_X, TOP_Y - 10.2, DIN_Z), name="housing", color=cq.Color(0.95, 0.64, 0.50))
DB = TOP_Y - 20.4
for nm, xx, zz, sx, sz, h, col in (("grove_plug_port_A", -11.7, -3, 4.6, 9.9, 8, (0.97,) * 3), ("grove_plug_port_B", -18.0, -3, 4.6, 9.9, 8, (0.97,) * 3),
                                    ("power_terminal_plug", -15.1, 7.6, 8.8, 7, 10, (0.2, 0.65, 0.3))):
    din.add(bx(sx, h, sz, DIN_X + xx, DB - h / 2, DIN_Z + zz), name=nm, color=cq.Color(*col))

# ---------- Unit Relay flat on the box bottom, long side along y: Grove plug up (towards the DinMeter), terminal down ----------
# Both Grove plugs face the DinMeter, so 10 cm Grove cables reach; the relay terminal faces the wire slot.
Z0 = FLOOR
rel = cq.Assembly(name="mini_relay")
rel.add(bx(24, 48, 8, RXC, RY0 + 24, Z0 + 4), name="case", color=cq.Color(0.93, 0.94, 0.95))
rel.add(bx(10.5, 15.2, 6, RXC, RY0 + 17.6, Z0 + 8 + 3), name="relay", color=cq.Color(0.98, 0.55, 0.10))
rel.add(bx(12, 10, 12, RXC, RY0 + 5, Z0 + 8 + 6), name="terminal_COM_NO_NC", color=cq.Color(0.98, 0.45, 0.15))
rel.add(bx(9.9, 6.5, 4.6, RXC, RY0 + 48 + 3.25, Z0 + 4), name="grove_plug", color=cq.Color(0.97, 0.97, 0.97))
REL_GROVE = (RXC, RY0 + 48 + 6.5, Z0 + 4)          # where the Grove cable leaves the relay plug (+y)
REL_PIN = {"COM": (RXC - 3.96, RY0, Z0 + 15), "NO": (RXC, RY0, Z0 + 15), "NC": (RXC + 3.96, RY0, Z0 + 15)}

# ---------- Unit ADC v1.1 flat on the box bottom, long side along y: Grove plug up, terminal down ----------
adc = cq.Assembly(name="adc_v11")
adc.add(bx(24, 32, 8, AXC, AY0 + 16, Z0 + 4), name="case", color=cq.Color(0.93, 0.94, 0.95))
adc.add(bx(9, 8, 11, AXC, AY0 + 4.5, Z0 + 8 + 5.5), name="terminal_IN_GND", color=cq.Color(0.98, 0.45, 0.15))
adc.add(bx(9.9, 6.5, 4.6, AXC, AY0 + 32 + 3.25, Z0 + 4), name="grove_plug", color=cq.Color(0.97, 0.97, 0.97))
ADC_GROVE = (AXC, AY0 + 32 + 6.5, Z0 + 4)
ADC_PIN = {"IN": (AXC - 2, AY0 + 0.5, Z0 + 13.5), "GND": (AXC + 2, AY0 + 0.5, Z0 + 13.5)}

# DinMeter connectors: where the cables leave (downwards, -y)
DIN_A = (DIN_X - 11.7, DB - 8, DIN_Z - 3)
DIN_B = (DIN_X - 18.0, DB - 8, DIN_Z - 3)
DIN_PLUS, DIN_MINUS = (DIN_X - 15.1 + 1.3, DB - 10, DIN_Z + 7.6), (DIN_X - 15.1 - 1.3, DB - 10, DIN_Z + 7.6)
SLOT_IN = (SLOT_X, -Y_IN, SLOT_Z)

asm = cq.Assembly(name=f"wrangler_air_{TAG}")
asm.add(box, name=f"{NAME}_body", color=cq.Color(0.16, 0.17, 0.19))
asm.add(lid, name=f"{NAME}_lid", color=cq.Color(0.30, 0.32, 0.35))
if boss_cut is not None:
    asm.add(boss_cut, name="boss_part_to_cut_off", color=cq.Color(0.90, 0.10, 0.10, 0.55))
asm.add(din); asm.add(rel)
if TAG != "f100":         # f100 = schematic A: sensor straight on PORT.A, no ADC
    asm.add(adc)
if GLANDS:                # M12 cable glands: lock nut inside, hex and dome nut outside
    for i, (gx, gz) in enumerate(GLANDS):
        g = (cq.Workplane("XZ").workplane(offset=-(-Y_IN + 5)).center(gx, gz).polygon(6, 18).extrude(5)
             .union(cq.Workplane("XZ").workplane(offset=TOP_Y).center(gx, gz).polygon(6, 17).extrude(4))
             .union(cq.Workplane("XZ").workplane(offset=TOP_Y + 4).center(gx, gz).circle(6.5).extrude(10)))
        g = g.cut(cq.Workplane("XZ").workplane(offset=-(-Y_IN + 6)).center(gx, gz).circle(3.3).extrude(40))
        asm.add(g, name=f"gland_M12_{'power' if i == 0 else 'sensor_ARB'}", color=cq.Color(0.10, 0.10, 0.11))

if __name__ == "__main__":
    import itertools
    from OCP.BRepExtrema import BRepExtrema_DistShapeShape
    items = []
    def walk(a, loc=cq.Location()):
        l = loc * a.loc
        if a.obj is not None:
            items.append((a.name, a.obj.val().moved(l)))
        for c in a.children:
            walk(c, l)
    walk(asm)
    ok = {(f"{NAME}_body", "housing"), (f"{NAME}_body", "bezel"), ("relay", "case"), ("terminal_COM_NO_NC", "case"),
          (f"{NAME}_body", "case"), ("bezel", "screen")}
    bad = [(round(s1.intersect(s2).Volume(), 2), n1, n2) for (n1, s1), (n2, s2) in itertools.combinations(items, 2)
           if (n1, n2) not in ok and (n2, n1) not in ok and s1.intersect(s2).Volume() > 0.01]
    print("collisions:", bad)
    def d(a, b):
        e = BRepExtrema_DistShapeShape(a.wrapped, b.wrapped); e.Perform(); return round(e.Value(), 1)
    I = {}
    for n, s in items:
        I.setdefault(n, []).append(s)
    body = I[f"{NAME}_body"][0]
    print("din housing - box (bosses)", d(I["housing"][0], body), "| relay grove plug - box", d(I["grove_plug"][0], body))
    print("adc - din plugs", min(d(I["case"][1], I[k][0]) for k in ("grove_plug_port_A", "grove_plug_port_B", "power_terminal_plug")),
          "| adc - relay", d(I["case"][1], I["case"][0]), "| relay - box (bosses)", d(I["case"][0], body))

