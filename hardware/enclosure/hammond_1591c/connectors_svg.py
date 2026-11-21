"""Connector pinout sheet: every harness connector, both halves, mating-face view (male pins / female sockets), wire colours.
Gender rule: the half that stays live when unplugged gets the sockets (female), so no live pin is ever exposed."""
CC = {"RD": "#d42020", "BK": "#111111", "YE": "#e0a800", "VT": "#7b2cbf", "OG": "#ff7a00"}
CONN = [  # name, where, pins [(colour, function)], female side, male side
    ("-X7", "вход питания, хвост правого ввода", [("RD", "+12 В после -F1"), ("BK", "масса 31")],
     "жгут машины (AUX1 / масса)", "хвост коробки, за ним сплайсы -X2 / -X2a"),
    ("-X8", "манифолд, хвост левого ввода", [("OG", "клапан +12 В (реле 1)"), ("RD", "+5 В датчика"), ("BK", "GND датчика"), ("YE", "сигнал давления G13")],
     "хвост коробки", "хвосты датчика -B1 и клапана -Y1, заведены прямо сюда"),
    ("-X10", "компрессор ARB", [("VT", "включить компрессор (реле 2)")],
     "хвост коробки", "жгут ARB, за ним -V1 / -V2"),
]
o = ['<svg viewBox="0 0 1100 620" style="background:#fff" xmlns="http://www.w3.org/2000/svg">',
     '<style>.d{font:700 14px Inter,system-ui,sans-serif;fill:#111}.n{font:12px Inter,system-ui,sans-serif;fill:#333}'
     '.c{font:11px Inter,system-ui,sans-serif;fill:#555}.p{font:700 11px Inter,system-ui,sans-serif;fill:#111}</style>']
lab = lambda x, y, t, c="c", a="start": o.append(f'<text x="{x}" y="{y}" class="{c}" text-anchor="{a}">{t}</text>')
lab(30, 36, "Wrangler Air · распиновка разъёмов", "d")
lab(30, 56, "Вид на рабочую сторону (откуда вставляется ответная часть). Замок сверху. Папа = штыри ●, мама = гнёзда ○. "
    "Номера у мамы зеркальны папе: 1 мамы стыкуется с 1 папы.", "c")
lab(30, 72, "Правило: мама на той стороне, где остаётся напряжение при расстыковке, голых штырей под питанием нет.", "c")


def face(x, y, pins, male):
    n = len(pins); pw = 30; w = n * pw + 20
    o.append(f'<rect x="{x}" y="{y}" width="{w}" height="50" rx="8" fill="#f3f3f1" stroke="#111" stroke-width="1.5"/>')
    o.append(f'<rect x="{x + w/2 - 14}" y="{y - 9}" width="28" height="9" rx="2" fill="#ddd" stroke="#111" stroke-width="1.2"/>')   # latch
    for i in range(n):
        k = i if male else n - 1 - i                      # female mirrored
        col, _ = pins[k]
        cx = x + 10 + pw * i + pw / 2
        if male:
            o.append(f'<circle cx="{cx}" cy="{y+25}" r="8" fill="{CC[col]}" stroke="#111" stroke-width="1"/>')
        else:
            o.append(f'<circle cx="{cx}" cy="{y+25}" r="8" fill="#fff" stroke="{CC[col]}" stroke-width="4"/>')
        lab(cx, y + 64, str(k + 1), "p", "middle")
    return w


Y = 100
for name, where, pins, fem, mal in CONN:
    lab(30, Y + 18, f"{name} · {len(pins)}P", "d"); lab(30, Y + 36, where, "c")
    wf = face(230, Y + 14, pins, False); lab(230, Y + 2, "мама: " + fem, "n")
    face(560, Y + 14, pins, True); lab(560, Y + 2, "папа: " + mal, "n")
    ty = Y + 18
    for i, (col, fn) in enumerate(pins):
        o.append(f'<rect x="870" y="{ty + i*17 - 10}" width="14" height="12" fill="{CC[col]}"/>')
        lab(890, ty + i * 17, f"{i+1}  {col}  {fn}", "c")
    o.append(f'<line x1="30" y1="{Y + 112}" x2="1070" y2="{Y + 112}" stroke="#ccc"/>')
    Y += 130
notes = ["Как стыкуется: 1 к 1, 2 к 2. На маме номер выбит на корпусе со стороны провода, на рабочей стороне он зеркальный, как на рисунке.",
         "-X8: сигнал YE в крайней ячейке 4, рядом только масса. +12 В клапана в ячейке 1, дальше всего от сигнала.",
         "Масса клапана не идёт через -X8: свой провод от катушки на шпильку у манифолда. -V3 1N4007 прямо на катушке.",
         "Набор герморазъёмов 1-6 pin, провод 0,5 мм² (20 AWG). Неиспользованных ячеек нет, заглушки не нужны.",
         "Мама и папа здесь по контактам: гнёзда или штыри. В наборе корпуса могут быть подписаны наоборот, смотреть на сами контакты."]
for i, t in enumerate(notes):
    lab(30, Y + 10 + i * 18, t, "c")
o.append("</svg>")
open("connectors_pinout.html", "w").write('<meta charset="utf-8"><body style="margin:0">' + "".join(o))
print("ok")
