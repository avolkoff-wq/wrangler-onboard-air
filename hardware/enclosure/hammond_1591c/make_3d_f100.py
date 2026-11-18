"""Builds box_f100_3d.html from the 1591C 3D page (same viewer, scale objects and photos) with the F100 model.
Run after cables_f100.py:  python3 make_3d_f100.py
"""
import base64
import re

t = open("hammond_1591c_3d.html", encoding="utf-8").read()
glb = base64.b64encode(open("wrangler_air_f100.glb", "rb").read()).decode()
t = re.sub(r'const GLB="[^"]*"', lambda m: f'const GLB="{glb}"', t, count=1)

t = t.replace("<title>Hammond 1591C 3D</title>", "<title>Box F100 3D</title>")
t = re.sub(r"<h1>.*?</h1>", "<h1>Wrangler Air in the F-100×68×50 box, schematic A</h1>", t, count=1, flags=re.S)
t = re.sub(r'<span class="sub">.*?</span>',
           '<span class="sub">AliExpress IP65 box, black base 36 mm and clear lid 14 mm (sizes from the seller drawing, check on the real box). '
           'DinMeter in one end wall, two M12 cable glands in the other. Relay flat on the floor, Grove 5 cm from PORT.B; '
           'the 2 m Grove from PORT.A leaves through the right gland to the sensor, together with the violet ARB wire. '
           'Left gland: black GND, red +12 V to the DinMeter and red +12 V to relay COM (silicone 20 AWG). No ADC. '
           'Hover or tap a part for its name. For scale: Galaxy Z Fold7 folded and a cigarette pack (photo: Wikimedia Commons, AKS.9955, CC BY-SA 4.0).</span>',
           t, count=1, flags=re.S)
t = t.replace('hammond_1591c_lid', 'hammond_f100_lid').replace('hammond_1591c_body', 'hammond_f100_body').replace('wrangler_air_1591c', 'wrangler_air_f100')
t = re.sub(r"const NAMES=\{.*?\};", """const NAMES={hammond_f100_body:"Box base, black ABS 100×68×36 (window and gland holes cut)",hammond_f100_lid:"Clear PC lid, 14 mm",
bezel:"DinMeter",screen:"DinMeter screen",knob:"DinMeter knob",housing:"DinMeter",grove_plug_port_A:"Grove plug in PORT.A (sensor)",grove_plug_port_B:"Grove plug in PORT.B (relay)",
power_terminal_plug:"DinMeter 6-36 V terminal",relay:"Mini 3A Relay",case:"Mini 3A Relay",terminal_COM_NO_NC:"Relay terminal: COM red, NO violet, NC unused",
gland_M12_power:"M12 gland: power (black, red, red)",gland_M12_sensor_ARB:"M12 gland: sensor Grove 2 m and violet ARB wire",
W1_grove_PORT_B_to_relay:"W1 Grove 5 cm, PORT.B to relay",W2_grove_PORT_A_to_sensor:"W2 Grove 2 m, PORT.A to sensor: black GND, red +5 V, yellow G13 signal, white unused",
GND_to_DinMeter_minus:"GND to DinMeter -, black 20 AWG",'12V_to_DinMeter_plus':"+12 V to DinMeter +, red 20 AWG",'12V_to_relay_COM':"+12 V to relay COM, red 20 AWG",
relay_NO_to_ARB:"Relay NO to the ARB (through V1), violet 20 AWG"};""", t, count=1, flags=re.S)
# clear lid: see-through by default
t = t.replace("addPhotos(); addScale();",
              "addPhotos(); addScale();\n  if(parts.hammond_f100_lid)parts.hammond_f100_lid.traverse(m=>{if(m.isMesh){m.material.color.set(0xbfe3ff);m.material.transparent=true;m.material.opacity=.28;m.material.depthWrite=false}});", 1)
# names of wire meshes carry the colour suffix: look the prefix up too
t = t.replace("function pick(e){", "function lbl(n){if(!n)return '';if(NAMES[n]!==undefined)return NAMES[n];for(const k in NAMES)if(n.startsWith(k))return NAMES[k];return ''}\nfunction pick(e){", 1)
t = t.replace("label=NAMES[o.name]||''", "label=lbl(o.name)", 1)
open("box_f100_3d.html", "w", encoding="utf-8").write(t)
print(len(t))
