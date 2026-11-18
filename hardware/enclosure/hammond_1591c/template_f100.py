"""1:1 cutting template for the F-100x68x50 base end walls (print at 100 %, A4). Writes template_f100.pdf via Playwright.
Run: WA_BOX=f100 python3 template_f100.py && node <shot script> ... (see bottom)"""
import os
os.environ.setdefault("WA_BOX", "f100")
import layout_box as L

W, H = L.W, L.HB                      # end wall of the base, outside: 68 x 36
win_x0 = W / 2 - L.CUT_W / 2; win_z0 = L.DIN_Z - L.CUT_H / 2
def wall(ox, oy, title, inner):
    o = [f'<g transform="translate({ox},{oy})">',
         f'<text x="0" y="-4" font-size="4" font-weight="700">{title}</text>',
         f'<rect x="0" y="0" width="{W}" height="{H}" rx="2" fill="none" stroke="#000" stroke-width="0.3"/>',
         f'<line x1="0" y1="{H}" x2="{W}" y2="{H}" stroke="#000" stroke-width="0.6"/>',
         f'<text x="{W/2}" y="{H+4.5}" font-size="2.6" text-anchor="middle">дно коробки (низ)</text>',
         f'<text x="{W/2}" y="-0.8" font-size="2.6" text-anchor="middle" fill="#555">кромка под крышку</text>']
    o += inner
    o.append('</g>')
    return o
def cross(x, y):
    return f'<line x1="{x-2}" y1="{y}" x2="{x+2}" y2="{y}" stroke="#000" stroke-width="0.15"/><line x1="{x}" y1="{y-2}" x2="{x}" y2="{y+2}" stroke="#000" stroke-width="0.15"/>'
z = lambda zz: H - zz                 # SVG y from height above the bottom
din = [f'<rect x="{win_x0}" y="{z(win_z0 + L.CUT_H)}" width="{L.CUT_W}" height="{L.CUT_H}" fill="#ffe1e1" stroke="#c00" stroke-width="0.35"/>',
       f'<rect x="{W/2-26.5}" y="{z(L.DIN_Z+15)}" width="53" height="30" rx="4" fill="none" stroke="#888" stroke-width="0.2" stroke-dasharray="1 1"/>',
       f'<text x="{W/2}" y="{z(L.DIN_Z)+1}" font-size="3.2" text-anchor="middle" fill="#c00">окно {L.CUT_W:g} × {L.CUT_H:g}</text>',
       f'<text x="{W/2}" y="{z(L.DIN_Z)+5}" font-size="2.2" text-anchor="middle" fill="#555">пунктир: рамка DinMeter 53×30</text>']
for cx, cz in ((win_x0, win_z0), (win_x0 + L.CUT_W, win_z0), (win_x0, win_z0 + L.CUT_H), (win_x0 + L.CUT_W, win_z0 + L.CUT_H)):
    ix = cx + (2 if cx < W / 2 else -2); iz = cz + (2 if cz < L.DIN_Z else -2)
    din.append(f'<circle cx="{ix}" cy="{z(iz)}" r="1.5" fill="none" stroke="#c00" stroke-width="0.2"/>' + cross(ix, z(iz)))
din.append(f'<text x="{W/2}" y="{H+8.5}" font-size="2.4" text-anchor="middle">кружки Ø3: стартовые отверстия внутри окна</text>')
gl = []
for gx, gz in L.GLANDS:
    x = W / 2 + gx
    gl.append(f'<circle cx="{x}" cy="{z(gz)}" r="{L.GLAND_D/2}" fill="#ffe1e1" stroke="#c00" stroke-width="0.35"/>' + cross(x, z(gz)))
    gl.append(f'<circle cx="{x}" cy="{z(gz)}" r="9" fill="none" stroke="#888" stroke-width="0.2" stroke-dasharray="1 1"/>')
gl.append(f'<text x="{W/2}" y="{H+8.5}" font-size="2.4" text-anchor="middle">Ø12, центры на высоте {L.GLANDS[0][1]:g}, {W/2+L.GLANDS[0][0]:g} и {W/2+L.GLANDS[1][0]:g} от левого края</text>')
gl.append(f'<text x="{W/2}" y="{H+12}" font-size="2.4" text-anchor="middle">пунктир Ø18: гайка изнутри</text>')
o = ['<svg xmlns="http://www.w3.org/2000/svg" width="210mm" height="297mm" viewBox="0 0 210 297" font-family="Inter, Arial, sans-serif">',
     '<text x="15" y="20" font-size="6" font-weight="700">Шаблон 1:1, коробка F-100×68×50, торцы основания</text>',
     '<text x="15" y="27" font-size="3.2">Печатать в масштабе 100 % (без «подогнать по размеру»). Проверить линейкой полосу 50 мм.</text>',
     '<line x1="15" y1="33" x2="65" y2="33" stroke="#000" stroke-width="0.6"/><line x1="15" y1="31" x2="15" y2="35" stroke="#000" stroke-width="0.4"/><line x1="65" y1="31" x2="65" y2="35" stroke="#000" stroke-width="0.4"/><text x="68" y="34.2" font-size="3">50 мм</text>']
o += wall(20, 55, "Торец 1: DinMeter (вид снаружи)", din)
o += wall(20, 125, "Торец 2: вводы M12 (вид снаружи)", gl)
o.append('<text x="15" y="190" font-size="3" fill="#555">Размеры коробки и стоек сняты с картинки продавца: перед резкой приложить шаблон к коробке и сверить.</text>')
o.append('</svg>')
open("template_f100.html", "w").write('<!doctype html><meta charset="utf-8"><style>@page{size:A4;margin:0}body{margin:0}</style>' + "".join(o))
