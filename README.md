# Wrangler Onboard Air

Контроллер бортового компрессора ARB Twin (CKMTA12) для Jeep Wrangler JL на M5Stack DinMeter.
Задаёшь целевое давление энкодером, нажимаешь, система подкачивает 4 колеса сама и останавливается на цели.

## Как устроено

- **AUX1** через -F1 3 A питает коробку (разъём -X7). DinMeter читает датчик давления (G13) и напряжение AUX1
  через модуль 0-25 V (G15), Unit 2Relay на PORT.B включает реле компрессора ARB (K1) и клапан (K4), выход через -X8.
- **AUX2** идёт мимо коробки прямо на катушку реле ARB: обход, если контроллер вышел из строя.
- **Потолок** 3.0 bar в программе, защиты и проверки описаны в [firmware/README.md](firmware/README.md).

## Где что лежит

| Папка | Что внутри | Главный файл |
|---|---|---|
| [docs/](docs) | решения, закупка, распиновка, стенд | [decisions.md](docs/decisions.md), [bom.md](docs/bom.md), [pinout.md](docs/pinout.md), [bench_test.md](docs/bench_test.md) |
| [electrical/](electrical) | электросхема и распиновка разъёмов | [power_schematic_def.png](electrical/power_schematic_def.png), [connectors_pinout.png](electrical/connectors_pinout.png) |
| [enclosure/](enclosure) | печатная коробка 60 × 100 × 36: раскладка, провода, STL | [box_cust_def.png](enclosure/box_cust_def.png), [box_cust_adc_body.stl](enclosure/box_cust_adc_body.stl) + [lid](enclosure/box_cust_adc_lid.stl) |
| [mounts/apillar/](mounts/apillar) | DinMeter в крышке ручки на передней стойке | [apillar_wedge.stl](mounts/apillar/apillar_wedge.stl) |
| [firmware/](firmware) | прошивка, готовый bin, настольный тест | [README.md](firmware/README.md), [bin/wrangler_air.bin](firmware/bin/wrangler_air.bin) |
| [screen/](screen) | макеты экранов и заставки, раскадровка | [storyboard.html](screen/storyboard.html), [menu.png](screen/menu.png) |
| [tools/](tools) | приложение для стенда в Chrome, лог по USB, видео | [bench_app.html](tools/bench_app.html) |
| [archive/](archive) | старые варианты: Hammond, F100, плата Perma-Proto, прежние схемы и макеты | только для истории |

Скрипты в `archive/` не запускаются с этого места: они ссылались на соседей по старым папкам.
