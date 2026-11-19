"""Builds box_f100_3d.html from the 1591C 3D page (same viewer, scale objects and photos) with the F100 model.
Run after cables_f100.py:  python3 make_3d_f100.py
"""
import base64
import os
import re

TAG = os.environ.get("WA_BOX", "f100")
ACROSS = TAG == "f100x"
CUST = TAG.startswith("cust")
PFX = "box" if CUST else "hammond"

t = open("hammond_1591c_3d.html", encoding="utf-8").read()
glb = base64.b64encode(open(f"wrangler_air_{TAG}.glb", "rb").read()).decode()
t = re.sub(r'const GLB="[^"]*"', lambda m: f'const GLB="{glb}"', t, count=1)

t = t.replace("<title>Hammond 1591C 3D</title>", "<title>Box F100 3D</title>" if not ACROSS else "<title>Box F100 Across 3D</title>")
t = re.sub(r"<h1>.*?</h1>", "<h1>Wrangler Air in the F-100×68×50 box, schematic A" + (", relay across" if ACROSS else ", relay along") + "</h1>", t, count=1, flags=re.S)
t = re.sub(r'<span class="sub">.*?</span>',
           '<span class="sub">AliExpress IP65 box, black base 36 mm and clear lid 14 mm (sizes from the seller drawing, check on the real box). '
           'DinMeter in one end wall, two M12 cable glands in the other. '
           + ('Relay across the box next to the glands (Grove 10 cm from PORT.B; the two floor bosses under it are cut flush). '
              'Right gland: the 2 m Grove from PORT.A to the sensor and the violet ARB wire from relay NO. Left gland: power. '
              if ACROSS else
              'Relay along the box on the left (Grove 5 cm from PORT.B). '
              'Left gland: the 2 m Grove from PORT.A to the sensor and the violet ARB wire straight from relay NO. Right gland: power. ')
           + 'Power: black GND, red +12 V to the DinMeter and red +12 V to relay COM (silicone 20 AWG). No ADC. '
           'Hover or tap a part for its name. For scale: Galaxy Z Fold7 folded and a cigarette pack (photo: Wikimedia Commons, AKS.9955, CC BY-SA 4.0).</span>',
           t, count=1, flags=re.S)
t = t.replace('hammond_1591c_lid', f'{PFX}_{TAG}_lid').replace('hammond_1591c_body', f'{PFX}_{TAG}_body').replace('wrangler_air_1591c', f'wrangler_air_{TAG}')
t = re.sub(r"const NAMES=\{.*?\};", f"""const NAMES={{{PFX}_{TAG}_body:"Box base, black ABS 100×68×36 (window and gland holes cut)",{PFX}_{TAG}_lid:"Clear PC lid, 14 mm",
bezel:"DinMeter",screen:"DinMeter screen",knob:"DinMeter knob",housing:"DinMeter",grove_plug_port_A:"Grove plug in PORT.A (sensor)",grove_plug_port_B:"Grove plug in PORT.B (relay)",
power_terminal_plug:"DinMeter 6-36 V terminal",relay:"Mini 3A Relay",case:"Mini 3A Relay",terminal_COM_NO_NC:"Relay terminal: COM red, NO violet, NC unused",
gland_M12_power:"M12 gland: power (black, red, red)",gland_M12_sensor_ARB:"M12 gland: sensor Grove 2 m and violet ARB wire",
W1_grove_PORT_B_to_relay:"W1 Grove {10 if ACROSS else 5} cm, PORT.B to relay",W2_grove_PORT_A_to_sensor:"W2 Grove 2 m, PORT.A to sensor: black GND, red +5 V, yellow G13 signal, white unused",
GND_to_DinMeter_minus:"GND to DinMeter -, black 20 AWG",'12V_to_DinMeter_plus':"+12 V to DinMeter +, red 20 AWG",'12V_to_relay_COM':"+12 V to relay COM, red 20 AWG",
relay_NO_to_ARB:"Relay NO to the ARB (through V1), violet 20 AWG"}};""", t, count=1, flags=re.S)
# clear lid: see-through by default
t = t.replace("addPhotos(); addScale();",
              "addPhotos(); addScale();\n  if(parts."+PFX+"_"+TAG+"_lid)parts."+PFX+"_"+TAG+"_lid.traverse(m=>{if(m.isMesh){m.material.color.set(0xbfe3ff);m.material.transparent=true;m.material.opacity=.28;m.material.depthWrite=false}});", 1)
# names of wire meshes carry the colour suffix: look the prefix up too
t = t.replace("function pick(e){", "function lbl(n){if(!n)return '';if(NAMES[n]!==undefined)return NAMES[n];for(const k in NAMES)if(n.startsWith(k))return NAMES[k];return ''}\nfunction pick(e){", 1)
t = t.replace("label=NAMES[o.name]||''", "label=lbl(o.name)", 1)
if CUST:
    adc = TAG == "cust_adc"
    t = t.replace("<title>Box F100 3D</title>", f"<title>Snap Box {'ADC' if adc else 'Direct'} 3D</title>")
    dfl = TAG == "cust_def"
    t = re.sub(r"<h1>.*?</h1>", "<h1>Wrangler Air, own printed box 60×100×36 with snap lid, " + ("schematic B (ADC)" if adc else "schematic C (Unit 2Relay + deflate valve)" if dfl else "schematic A (no ADC)") + "</h1>", t, count=1, flags=re.S)
    t = re.sub(r'<span class="sub">.*?</span>', '<span class="sub">Printed box, walls 2 mm, flat 2 mm lid on four snap hooks. DinMeter in the upper end wall, '
               'two M12 cable glands in the lower end wall. ' + ('Relay left, ADC right (Grove 5 cm and 10 cm); right gland: sensor wires orange, blue, yellow to the ADC and the violet ARB wire, left gland: power.'
               if adc else 'Unit 2Relay along the left wall, Grove 5 cm from PORT.B (yellow = channel 1 compressor, white = channel 2 valve); 0-25 V voltage module on the floor at the right, S to G15; left gland: 2 m Grove to the sensor and the violet ARB wire, right gland: power and the orange valve wire.'
               if dfl else 'Relay along on the left, Grove 5 cm from PORT.B; left gland: 2 m Grove to the sensor and the violet ARB wire, right gland: power.')
               + ' Hover or tap a part for its name. For scale: Galaxy Z Fold7 folded and a cigarette pack (photo: Wikimedia Commons, AKS.9955, CC BY-SA 4.0).</span>', t, count=1, flags=re.S)
    t = t.replace("Box base, black ABS 100×68×36 (window and gland holes cut)", "Printed box, walls 2 mm").replace("Clear PC lid, 14 mm", "Lid 2 mm on 4 snap hooks")
    t = t.replace('relay_NO_to_ARB:"Relay NO', 'W2_grove_PORT_A_to_ADC:"W2 Grove 10 cm, PORT.A to ADC",sensor_GND_to_ADC:"Sensor ground to ADC GND, blue",sensor_signal_to_ADC:"Sensor signal to ADC IN, yellow",sensor_5V_to_X4:"Sensor +5 V, orange, soldered onto the red conductor of W2 (-X4)",relay_K2_NO_to_valve:"2Relay channel 2 NO to the deflate valve, orange 20 AWG",jumper_COM1_COM2:"Jumper COM1 to COM2, red 20 AWG",vmod_board:"Voltage module 0-25 V (divider 30k/7.5k) on 3M VHB",vmod_terminal_VCC_GND:"Voltage module input: VCC red, GND black, own wires from the splices outside the box",vmod_header_S_plus_minus:"Voltage module output S to G15","12V_to_vmod_VCC":"+12 V to voltage module VCC, red 20 AWG",GND_to_vmod_GND:"GND to voltage module GND, black 20 AWG",G15_to_vmod_S:"G15 (white conductor of W2, spliced -X6a) to voltage module S",relay_1:"2Relay channel 1 (compressor)",relay_2:"2Relay channel 2 (deflate valve)",terminal_K1:"Channel 1 terminal: COM red, NO violet",terminal_K2:"Channel 2 terminal: COM red jumper, NO orange",relay_NO_to_ARB:"Relay NO', 1)
open(f"box_{TAG}_3d.html", "w", encoding="utf-8").write(t)
print(len(t))
