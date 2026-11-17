"""Wrangler Air in a stock Hammond 1591CBK (120 x 65 x 40, black ABS), sizes from Hammond drawing REV 06.16.2023.

The box lies with its lid against the tunnel wall. Axes as in the printed pod:
x along the car (120), y up (65), z from the box bottom (0, towards the driver) to the lid (40, at the tunnel).
The DinMeter goes into the upper long wall, screen up. Relay flat on the lower long wall, ADC on edge against
the box bottom. Car wires enter through a slot in the left end wall.

Writes: wrangler_air_1591c.step, wrangler_air_1591c.glb, and prints the cut-out coordinates and clearances.
"""
import cadquery as cq

# ---------- Hammond 1591C (drawing 1591C REV 06.16.2023) ----------
L, Wd, HB, HL = 120.0, 65.0, 36.0, 3.8      # length, width, body height, lid (39.8 overall)
IN_L, IN_W = 113.53, 58.53                  # inside length, width
FLOOR = 2.0                                 # box bottom thickness
R_OUT = 5.0
BOSS_X, BOSS_Y, BOSS_D, BOSS_HOLE = 97.35, 42.35, 7.6, 2.5  # boss centres (inside box), boss dia (est.), M3 insert
RIB_D, RIB_W, RIB_H, RIB_PITCH = 1.0, 3.0, 30.9, 6.0       # card guides on the long walls (pitch estimated)
WALL_L = (Wd - IN_W) / 2                    # long-wall thickness incl. draft ~3.2
Y_IN = IN_W / 2

box = (cq.Workplane("XY").rect(L, Wd).extrude(HB).edges("|Z").fillet(R_OUT)
       .faces("<Z").edges().fillet(1.0))
box = box.cut(cq.Workplane("XY").workplane(offset=FLOOR).rect(IN_L, IN_W).extrude(HB).edges("|Z").fillet(2.0))
for sx in (-1, 1):
    for sy in (-1, 1):
        boss = cq.Workplane("XY").workplane(offset=FLOOR).center(sx * BOSS_X / 2, sy * BOSS_Y / 2).circle(BOSS_D / 2).extrude(HB - FLOOR)
        hole = cq.Workplane("XY").workplane(offset=HB - 12).center(sx * BOSS_X / 2, sy * BOSS_Y / 2).circle(BOSS_HOLE / 2).extrude(12)
        box = box.union(boss).cut(hole)
        # webs from the boss to the corner
        box = box.union(cq.Workplane("XY").workplane(offset=FLOOR).center(sx * (BOSS_X / 2 + 4), sy * (BOSS_Y / 2 + 4)).rect(6, 6).extrude(HB - FLOOR))
ribs = None
n = int((IN_L - 20) / RIB_PITCH)
for i in range(n + 1):
    x = -(n * RIB_PITCH) / 2 + i * RIB_PITCH
    for sy in (-1, 1):
        r = cq.Workplane("XY").box(RIB_W, RIB_D, RIB_H).translate((x, sy * (Y_IN - RIB_D / 2), HB - RIB_H / 2))
        box = box.union(r)
lid = (cq.Workplane("XY").rect(L, Wd).extrude(HL).edges("|Z").fillet(R_OUT).faces(">Z").edges().fillet(1.0)
       .translate((0, 0, HB)))

# box coordinates -> our axes: CadQuery X = x, CadQuery Y = y (up), CadQuery Z = z (towards the lid)
TOP_Y = Wd / 2            # outer face of the upper long wall
FLOOR_Y = -Y_IN + RIB_D   # modules sit on the rib tops of the lower long wall

# ---------- DinMeter in the upper long wall ----------
DIN_X, DIN_Z = -20.0, 18.0
CUT_W, CUT_H = 46.0, 22.8
box = box.cut(cq.Workplane("XY").box(CUT_W, 10, CUT_H).translate((DIN_X, TOP_Y - 2, DIN_Z)))
# clip seat: the card-guide ribs under the DinMeter are cut back flush (file them off)
box = box.cut(cq.Workplane("XY").box(52, RIB_D + 0.01, 30).translate((DIN_X, Y_IN - RIB_D / 2, DIN_Z)))

# ---------- car-wire slot in the left end wall ----------
SLOT_Y, SLOT_Z, SLOT_L, SLOT_H = -8.0, 18.0, 14.0, 8.0
box = box.cut(cq.Workplane("YZ").workplane(offset=-L / 2 - 1).center(SLOT_Y, SLOT_Z).slot2D(SLOT_L, SLOT_H, 0).extrude(6))

def bx(sx, sy, sz, cx, cy, cz):
    return cq.Workplane("XY").box(sx, sy, sz).translate((cx, cy, cz))

din = cq.Assembly(name="dinmeter")
din.add(bx(53, 2.5, 30, DIN_X, TOP_Y + 1.25, DIN_Z), name="bezel", color=cq.Color(0.89, 0.38, 0.16))
din.add(bx(45.6, 20.4, 22.2, DIN_X, TOP_Y - 10.2, DIN_Z), name="housing", color=cq.Color(0.95, 0.64, 0.50))
din.add(cq.Workplane("XZ").workplane(offset=-(TOP_Y + 2.5)).center(DIN_X + 14.25, DIN_Z).circle(6).extrude(-8.6), name="knob", color=cq.Color(0.85, 0.85, 0.85))
DB = TOP_Y - 20.4
for nm, xx, zz, sx, sz, h, col in (("grove_plug_port_A", -11.7, -3, 4.6, 9.9, 8, (0.97,) * 3), ("grove_plug_port_B", -18.0, -3, 4.6, 9.9, 8, (0.97,) * 3),
                                    ("power_terminal_plug", -15.1, 7.6, 8.8, 7, 10, (0.2, 0.65, 0.3))):
    din.add(bx(sx, h, sz, DIN_X + xx, DB - h / 2, DIN_Z + zz), name=nm, color=cq.Color(*col))

# ---------- Unit Relay flat on the lower wall, terminal end at -x ----------
RX0, RZ = -40.0, 20.0
rel = cq.Assembly(name="mini_relay")
rel.add(bx(48, 8, 24, RX0 + 24, FLOOR_Y + 4, RZ), name="case", color=cq.Color(0.93, 0.94, 0.95))
rel.add(bx(15.2, 6, 10.5, RX0 + 17.6, FLOOR_Y + 11, RZ), name="relay", color=cq.Color(0.98, 0.55, 0.10))
rel.add(bx(10, 12, 12, RX0 + 5, FLOOR_Y + 14, RZ), name="terminal_COM_NO_NC", color=cq.Color(0.98, 0.45, 0.15))
rel.add(bx(6.5, 4.6, 9.9, RX0 + 48 + 3.25, FLOOR_Y + 4, RZ), name="grove_plug", color=cq.Color(0.97, 0.97, 0.97))

# ---------- Unit ADC v1.1 on edge against the box bottom, Grove end at -x ----------
AX0, AY = 12.0, -6.0
AZ = FLOOR + 4
adc = cq.Assembly(name="adc_v11")
adc.add(bx(32, 24, 8, AX0 + 16, AY, AZ), name="case", color=cq.Color(0.93, 0.94, 0.95))
adc.add(bx(9, 8, 11, AX0 + 32 - 4.5, AY, FLOOR + 8 + 5.5), name="terminal_IN_GND", color=cq.Color(0.98, 0.45, 0.15))
adc.add(bx(6.5, 9.9, 4.6, AX0 - 3.25, AY, AZ), name="grove_plug", color=cq.Color(0.97, 0.97, 0.97))

asm = cq.Assembly(name="wrangler_air_1591c")
asm.add(box, name="hammond_1591c_body", color=cq.Color(0.16, 0.17, 0.19))
asm.add(lid, name="hammond_1591c_lid", color=cq.Color(0.30, 0.32, 0.35))
asm.add(din); asm.add(rel); asm.add(adc)

if __name__ == "__main__":
    import itertools
    items = []
    def walk(a, loc=cq.Location()):
        l = loc * a.loc
        if a.obj is not None:
            items.append((a.name, a.obj.val().moved(l)))
        for c in a.children:
            walk(c, l)
    walk(asm)
    ok = {("hammond_1591c_body", "housing"), ("hammond_1591c_body", "bezel"), ("relay", "case"), ("terminal_COM_NO_NC", "case"),
          ("hammond_1591c_body", "case")}
    bad = []
    for (n1, s1), (n2, s2) in itertools.combinations(items, 2):
        if (n1, n2) in ok or (n2, n1) in ok:
            continue
        v = s1.intersect(s2).Volume()
        if v > 0.01:
            bad.append((round(v, 2), n1, n2))
    print("collisions:", bad)
    from OCP.BRepExtrema import BRepExtrema_DistShapeShape
    def d(a, b):
        e = BRepExtrema_DistShapeShape(a.wrapped, b.wrapped); e.Perform(); return round(e.Value(), 1)
    I = {}
    for n, s in items:
        I.setdefault(n, []).append(s)
    body = I["hammond_1591c_body"][0]
    print("relay grove plug - box", d(I["grove_plug"][0], body), "| adc grove plug - box", d(I["grove_plug"][1], body))
    print("relay terminal - box", d(I["terminal_COM_NO_NC"][0], body), "| din housing - box", d(I["housing"][0], body))
    print("adc - din housing", d(I["case"][1], I["housing"][0]), "| adc - relay", d(I["case"][1], I["case"][0]), "| adc terminal - relay", d(I["terminal_IN_GND"][0], I["case"][0]))
    asm.save("wrangler_air_1591c.step")
    asm.save("wrangler_air_1591c.glb", tolerance=0.25, angularTolerance=0.4)
    print("cut-out: DinMeter %.1f x %.1f, centre %.1f from the left end, %.1f from the box bottom" % (CUT_W, CUT_H, L / 2 + DIN_X, DIN_Z))
    print("slot: %.0f x %.0f in the left end wall, centre %.1f below the top edge... y=%.1f, z=%.1f" % (SLOT_L, SLOT_H, TOP_Y - SLOT_Y, SLOT_Y, SLOT_Z))
