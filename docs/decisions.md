# Решения и текущее состояние

Журнал решений, чтобы с любого компьютера было видно, где мы. Новое сверху.

## 19.11.2026 (графики работ)

- **Каждая работа пишется в файл** `/runs/r<загрузка>_<номер>.csv`: раз в 0,5 с давление, оценка, фаза, компрессор, клапан, статические замеры отдельными строками.
  В памяти до конца работы, на флеш одной записью. Последние 20 работ, до ~8 kB на работу (дальше `truncated`).
- **Страница Wi-Fi** 192.168.4.1: список работ и график по ссылке (SVG и простой JS, без библиотек, без интернета), скачать CSV, «Clear runs».
  Живого графика нет, ADC2 при Wi-Fi не читается. Пример: `docs/screen/run_graph_example.html`.

## 18.11.2026 (этап 3: спуск)

- **Типовые давления вместо шага 0,1:** HIGHWAY 2.5, ROUGH ROAD 1.8, GRAVEL 1.5, TRAIL 1.2, ROCK 1.0, SAND 0.8, RECOVERY 0.5 bar, последним CUSTOM со своим значением, которое ставится ручкой с шагом 0,1 bar и проверяется перед каждым запуском (список владельца, `config.h` PRESETS).
- **Гибрид:** качает компрессор, как раньше, клапан только стравливает. Одна цель, контроллер сам выбирает подкачку или спуск,
  в пределах ±0,1 bar ничего не делает. Ниже 0,8 bar на экране LOW.
- **Схема C** `power_schematic_def.png` = A + Unit 2Relay: канал 1 (G2, жёлтый PORT.B) → ARB, канал 2 (G1, белый) → клапан -Y1.
  COM2 перемычкой от COM1, F1 3 A на всё (DinMeter 0,15 + катушка ARB 0,2 + клапан ≤ 1,7 A; компрессор и клапан никогда вместе).
  Диод 1N4007 на клапане, масса клапана у манифолда. Оранжевый провод клапана через ввод питания M12.
- **Коробка та же** 60 × 100 × 36 (`box_cust_adc_body/lid.stl`), раскладка `box_cust_def.png` / `box_cust_def_3d.html`:
  2Relay 56 × 24 вдоль левой стенки, клеммы к середине. Сторону клемм и Grove проверить на живом модуле.
- **Клапан** 2W040-10 латунь DC12V в свободный порт 1/8 BSPP манифолда через ниппель 1/8 × 3/8, глушитель 3/8.
  Клапана 12 В с наружной 1/8, который вкручивается прямо в порт, не нашли.
- **Прошивка 0.4.0** (`pio run -e dinmeter-deflate`, `bin/wrangler_air_deflate.bin`): спуск по модели p_абс(t) = p_абс0 · e^(−t/τ),
  τ из размера шины (≈ 179 с для 4 × 255/75R17), первый спуск 60 % пути, потом короткие; две раздельные калибровки (подкачка и спуск),
  обучение на каждом цикле. Меню TYRE: 255/75R17 или 285/70R17, выученное пересчитывается по объёму. Температуру не учитываем.
- **Лог во всех сборках:** CSV на флеше, скачивается по Wi-Fi (меню WIFI: UPDATE + LOG → 192.168.4.1/log). Калибровка, каждый цикл, поправки и их обучение.

## 18.11.2026 (своя коробка)

- **Своя печатная коробка с крышкой на защёлках, одна на обе схемы:** 60 x 100 x 36, печатные файлы `box_cust_adc_body` / `box_cust_adc_lid` (.stl для печати, .step для CAD).
  Чертежи раскладки отдельно: `box_cust.png` / `box_cust_3d.html` (схема A, без ADC, место ADC пустое), `box_cust_adc.png` / `box_cust_adc_3d.html` (схема B, с ADC).
  Узкий вариант 54 мм убран: печатаем один корпус, при переходе A → B перепечатывать не надо.
  Стенки и дно 2 мм, крышка 2 мм с бортиком 1,5 x 3,5 и 4 защёлками (крючок 1,2 мм, зуб 0,8, гнёзда в стенке 1,0), паз под отвёртку.
  DinMeter в верхнем торце (окно 46 x 22,8, просто вставляется), два ввода M12 в нижнем торце (±12 мм, высота 18). Печать PETG или ASA.
  Модель: `WA_BOX=cust` / `cust_adc` в `layout_box.py`, провода `cables_f100.py`, чертёж `draw_f100.py`, STL/STEP `WA_BOX=cust_adc python3 export_cust_stl.py`.
- **Длины проводов (мм):** cust: GND 140, +12 V 140, COM 100, VT 70.
  cust_adc: GND 145, +12 V 140, COM 75, VT 100, датчик GND/IN 85, +5 V к X4 135.

## 18.11.2026

- **Закупка:** всё на AliExpress одним заказом, список в [bom.md](bom.md). На Galaxus только 3M VHB.
- **Корпус:** коробка IP65 F-100x68x50, чёрное основание 36 + прозрачная крышка 14, без ушек.
  DinMeter в торце, окно 46 x 22,8 целиком в основании (7,6 от дна, 5,6 до кромки), бобышки не режем.
  Внутренние размеры (~63 x 95 x 45) и высота бобышек на дне сняты с картинки продавца: замерить на коробке до резки окна.
  Модель: `WA_BOX=f100` в `hardware/enclosure/hammond_1591c/layout_box.py`, чертёж `draw_f100.py` → `box_f100.png`.
  Hammond 1591CBK (Distrelec) остаётся запасным вариантом, чертежи `hammond_1591c.html`.
- **Раскладка в F100, два варианта (оба в git):**
  A1 `box_f100.png` / `box_f100_3d.html`: реле вдоль слева, Grove 5 см. Левый ввод = датчик (Grove 2 м) + фиолетовый прямо с NO, правый = питание.
  A2 `box_f100x.png` / `box_f100x_3d.html`: реле поперёк у вводов, клеммы справа, Grove 10 см к реле, две бобышки на дне под реле срезать.
  Правый ввод = датчик + фиолетовый, левый = питание. Свободное место одним куском ~40 мм в середине. Запас по ширине у реле ~4 мм с каждой стороны.
- **Ввод проводов:** вместо паза два кабельных ввода M12 IP68 (чёрные) в нижнем торце, отверстия Ø12 на высоте 18,
  в 22 мм от края и 24 мм между собой. Какой ввод под что, зависит от раскладки (см. выше).
  Под гайкой ввода на жгут кусок термоусадки 3:1 с клеем 9,5 мм, чтобы сечение было круглым (ввод обжимает 3-6,5 мм).
- **Провод:** силикон 20 AWG (0,5 мм²), механическая прочность в машине. Цвета: RD +12 V, VT компрессор, BK масса,
  OG +5 V датчика, BU масса датчика, YE сигнал.
- **Жгуты:** два жгута в термоусадке 2:1 6,4 мм без клея (вход 12 V + масса; датчик + управление ARB).
  На выходе из паза и на концах кольца 3:1 с клеем.
- **Grove:** 5 см к реле (PORT.B), 2 м к датчику (PORT.A). ADC v1.1 и Grove 10 см в запасе.
- **Две схемы хранятся обе:** вариант A `power_schematic.png` (датчик 0-3,3 V прямо на PORT.A, `power_svg_iec()`),
  вариант B `power_schematic_adc.png` (датчик 0,5-4,5 V через ADC v1.1, `power_svg_iec_adc()`), обе в `hardware/enclosure/hammond_1591c/`.
- **Датчик (основной вариант):** 5 V питание, выход **0-3,3 V**, 0-1,2 MPa, G1/8 на бондовой шайбе в порт 1/8 BSPP манифолда ARB 171503.
  Подключение без ADC и без делителя: кабель Grove 2 м прямо из PORT.A, чёрный GND, красный +5 V, жёлтый G13 (вход АЦП), белый не подключён.
  У датчика разъём JWPF 3P (-X6). Обрыв провода ловит прошивка: компрессор работает N секунд, а давление не растёт → стоп и ошибка;
  резкий провал давления до нуля → стоп. Куплен и вариант 0,5-4,5 V: он идёт через ADC v1.1, если АЦП ESP32 окажется слишком шумным.
  G13 на ESP32-S3 это ADC2: не читается, пока включён Wi-Fi (только режим обновления OTA), в работе Wi-Fi выключен.
- **Диоды:** берём оба варианта, 1N4007 в соединителе с припоем и диоды в корпусе мини-реле в колодках.

## Ранее

- **Схема питания:** IEC-схема `hardware/enclosure/hammond_1591c/power_schematic.png` (`power_svg.py`).
  AUX1 → F1 3 A → Y снаружи: DinMeter +12 V и COM реле. NO реле → VT → V1. AUX2 → V2. Катоды V1/V2 вместе
  с фиолетовым проводом ARB (X3). AUX2 в коробку не заходит никогда.
- **D3 не нужен:** у ARB CKMTA12 свои реле внутри ([manual](https://store.arbusa.com/content/CKMTA12_Installation%20Guide.pdf)).
- **Датчик 5 V + ADC v1.1:** вход ESP32 до 3,3 V, G1 занят на PORT.B. +5 V датчика от красного провода Grove PORT.A
  (единственная пайка, X4).
- **Не использовать** запчасти Crown Automotive. Текст на экране DinMeter только латиницей.

## 2026-11-19 Boot splash
- VOLKOV AIR (AIR amber), version below, a generic SUV drives over two bumps; 1.6 s, a press skips it. No Jeep grille or logo. Mock-up: docs/screen/splash.gif. Outputs stay off during the splash.
- Power off by switching AUX1 is fine: NVS and LittleFS survive a cut; mid-run the relays drop, the NC valve closes, only that run's hour-meter minutes and trace tail are lost.

## 2026-11-19 CUSTOM wheel count, hours labels, splash
- CUSTOM: after the pressure, a second step picks 1-4 wheels on the line. The run scales the learned values to that air volume (rate, kTop x4/n, tauD, c x n/4); NVS always keeps the 4-wheel values, so learning carries over and a power cut mid-run leaves nothing scaled. Main screen shows "CUSTOM  2 WHEELS" (portrait "CUSTOM x2") when below 4.
- HOURS: "compressor total" and "since cleaning".
- Splash: no firmware version.
- Silhouette (21.11): the owner's white-on-black reference image, used as a mask for all icons and the splash: docs/screen/suv_mask.png (512 px) for the mock-ups, firmware/wrangler_air/suv_bitmap.h (128x51, 1 bit) pushed rotated and zoomed with pushRotateZoomWithAA. 40 units long, pivot = ground under the rear wheel, nose to the right. Wheels about 27 % smaller relative to the body than the earlier hand-drawn one.

## 2026-11-19 Over-pressure
- The owner has a 4 bar relief valve on the wheel line, so the firmware over-pressure stop is a second layer, not the only protection.
- Supply-voltage monitor dropped: no divider resistors wanted, and the DinMeter only measures its own battery.

## 2026-11-19 Supply voltage: ready 0-25 V module
- The owner wanted no loose resistors. The ADC Unit v1.1 reads only 0-12 V (docs.m5stack.com), so 12.4-14.4 V would clip at 12.0 and could not tell engine off from engine on.
- Taken instead: a ready 0-25 V voltage module (30k / 7.5k divider on the board, 1:5), flat on the floor in the free right half of the cust_def box on 3M VHB. VCC and GND by jumpers from the DinMeter terminal, output S on G15 = the white conductor of the PORT.A Grove (cut inside the box, solder-seal splice -X6a; the white wire is unused at the sensor). 14.4 V gives 2.9 V on G15.
- G15 is ADC2 like G13: not readable while Wi-Fi is on. Firmware use (voltage on screen, ENGINE OFF? below ~12.8 V, log and run graphs) to be added.
- Change the same day (owner): every +12 V and ground splice is made outside the box at -X2 / -X2a, ready wires go into the box. Right gland now carries 6 wires (BK DinMeter, BK module, RD DinMeter, RD relay COM, RD module, OG valve), ~5.3 mm bundle; a 9.5 mm 3:1 adhesive heat-shrink jacket over the bundle at the gland makes it round so the M12 (3-6.5 mm) seals.

## 2026-11-19 Deflate flow check: is the vent valve the bottleneck?
- Path: 4 valve cores with chucks in parallel, then hoses, tee, manifold, 1/8 x 3/8 nipple, valve 2W040-10 (4 mm), muffler. Isentropic orifice model, choked and subsonic, 4 x 255/75R17 = 210 L:

| core, each | valve chain (valve + nipple + muffler) | total | valve chain slows by | 2.5 -> 1.2 | 2.5 -> 0.8 | 2.5 -> 0.5 bar |
|---|---|---|---|---|---|---|
| 1.0 mm2 | 7.1 mm2 | 3.5 mm2 | 13 % | 140 s | 201 s | 256 s |
| 2.0 mm2 | 7.1 mm2 | 5.3 mm2 | 33 % | 92 s | 131 s | 168 s |
| 2.0 mm2 | 4.0 mm2 (valve Cv 0.23) | 3.6 mm2 | 56 % | 137 s | 197 s | 251 s |

- Conclusion: the 4 cores are the main restriction; the valve chain adds 13-33 % with the expected valve, up to ~56 % if the seller's Cv 0.23 is true. Not a blocker: deflate takes 2-4 min, and tau is learned on the real system.
- The exponential choked model stays within 1 % of the full model down to 0.5 bar (subsonic only at the very end).
- Bug fixed: a CUSTOM run on fewer wheels scaled tau by n/4 (valve-limited assumption). With cores in parallel tau(1 wheel) is ~0.69 x tau(4), not 0.25 x. Now tauWheels(n) from the core/valve split, and such a run never writes its learning: the 4-wheel values are set aside and restored after the run.
- Field check (bench step 5 and 6): LEARN DEFLATE on 1 wheel and on 4 wheels. tau4 / tau1 near 1.4 = cores dominate as assumed; near 4 = the valve dominates.

## 2026-11-19 Unit 2Relay as it really is, entries grouped by gland
- From docs.m5stack.com/en/unit/Unit_2Relay (owner noticed the contacts were drawn wrong): Grove pin order GND, 5V, RELAY-2 (yellow), RELAY-1 (white). Relay 1 sits at the far end, relay 2 next to the Grove plug. Each relay has a VH3.96-4P header right behind it, pins across the module C A B C: the contact switches A-B, the two C pins are only linked to each other.
- Same firmware pins: compressor on G2 (yellow) = relay 2, valve on G1 (white) = relay 1. Wiring: +12 V to A of relay 2, jumper A2 -> A1; B2 violet to the ARB, B1 orange to the valve. C pins unused.
- The relay bodies are about as wide as the module, so the left gland at x -12 would sit right behind relay 1. Glands moved to x -2 (sensor + ARB) and x +17.5 (power + valve); 2Relay back at the gland end (y -42). Printed box exported again (box_cust_adc_body/lid .stl/.step).
- Schematic C redrawn with the entries grouped exactly as they pass the glands: -X1 right gland 1-6 (RD DinMeter, RD module, BK module, BK DinMeter, RD relay 2 A, OG valve), -X5 left gland 7 (VT) + -W2 Grove.
- Same day, owner: no splice inside the box at all. Relay 1 A gets its own red wire from -X2 (no A2-A1 jumper). -X1 order: 1-2 grounds (DinMeter, module), 3-6 +12 V (module, DinMeter, relay 2 A, relay 1 A), 7 output OG to the valve; -X5: 8 output VT to the ARB plus the Grove to the sensor. Right gland now 7 wires (hex bundle ~5.3 mm, same as 6) in the 9.5 mm adhesive heat-shrink jacket.
- -X2 splice outside: AUX1 after -F1 into 4 wires; -X2a: ground into 2 wires.
- Owner: the valve wire is an output, so it moves to the outputs. Right gland = power in only, 6 wires (1-2 grounds, 3-6 +12 V). Left gland = outputs 7 OG (valve) and 8 VT (ARB) plus the Grove to the sensor; valve and sensor both sit at the manifold, so they leave together.

## 2026-11-19 Box disconnect: Deutsch DT connectors on the pigtails
- Owner: glands stay, no connectors in the box wall. The wires leave the glands as ~20 cm pigtails ending in Deutsch DT connectors, so the box unplugs from the car harness.
- -X7 input, DT 8-way (6 used): 1 BK DinMeter, 2 BK module, 3 RD module, 4 RD DinMeter, 5 RD relay 2 A, 6 RD relay 1 A, 7-8 spare with seal plugs. Box pigtail DT04-08P (pins), car side DT06-08S (sockets): the always-live car side has the shrouded sockets.
- -X8 output, DT 6-way (5 used): 1 OG valve, 2 VT ARB, 3 BK sensor GND, 4 RD sensor +5 V, 5 YE sensor signal, 6 spare. Box pigtail DT06-6S (sockets), car side DT04-6P (pins). Different size and gender, so the two cannot be swapped.
- DT: 13 A per contact, IP68, needs the DT crimp tool. Splices -X2 (+12 V, 1 to 4) and -X2a (ground, 1 to 2) stay in the car harness before -X7.
