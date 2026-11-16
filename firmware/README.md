# Прошивка

## Сборка и прошивка

| Способ | Как |
|---|---|
| Готовый файл | `bin/wrangler_air_stage1.bin`: залить через меню UPDATE (Wi-Fi) или esptool по USB-C |
| Arduino IDE | открыть `wrangler_air/wrangler_air.ino`, плата M5Stack StampS3 (DinMeter), библиотека M5Unified, разметка памяти с двумя OTA-разделами |
| PlatformIO | `pio run -e dinmeter`, прошить: `pio run -e dinmeter -t upload` |

**Стенд без датчика:** `bin/wrangler_air_stage1_SIM.bin` (или `pio run -e dinmeter-sim`). Вместо датчика работают виртуальные колёса: пока K1 включён, давление растёт, на ходу датчик показывает больше на поправку. Можно проверить весь алгоритм на столе, только DinMeter по USB.

## Спецификация

Платформа: M5Stack DinMeter v1.1 (StampS3A, ESP32-S3), Arduino IDE, библиотека M5Unified.

## Пины

| Пин | Назначение |
|---|---|
| G1 (PORT.B) | датчик через делитель 10k/20k: V_датчика = V_G1 × 1.5 |
| G2 (PORT.B) | K1 (подкачка / реле компрессора), HIGH = включено |
| G13 (PORT.A) | K4 (спуск, этап 3). При старте сразу LOW |
| G41/G40, G42 | энкодер A/B, кнопка |
| G46 | удержание питания |

## Управление

Все надписи на экране только латиницей (решение владельца, кириллица не нужна).


- Поворот энкодера: целевое давление, шаг 0.1 bar, максимум 3.0.
- Нажатие: старт. Нажатие во время работы: стоп.
- Долгое нажатие: меню (калибровка, bar/psi, заготовки, обновление, версия).

## Алгоритм подкачки (4 колеса разом, мотор заведён)

1. Замер без потока.
2. Качает без пауз, колёса ≈ датчик − поправка.
3. Стоп за 0.1 bar до цели, 2 с замер без потока.
4. При недоборе одна добивка, длина = (цель − p) / скорость.
5. Поправка уточняется: новая = 0.7 × старая + 0.3 × измеренная, хранится в памяти (Preferences).
6. Первая подкачка: калибровочный импульс 15 с.

Спуск (этап 3): та же логика, колёса ≈ датчик + поправка спуска. K1 и K4 никогда не включены одновременно.

## Защиты

- Ошибка датчика: G1 ниже 0.33V (обрыв) или выше 3.0V.
- Давление выше 3.0 bar: стоп.
- Нет роста 0.1 bar за 60 с: утечка или шланг не подключён, стоп.
- Таймаут 8 минут.

## Калибровка

Меню, 4 точки: 0, ~1, ~2, ~3 bar. Программа качает до точки, водитель вводит показание стрелочного манометра энкодером, программа идёт дальше. Итог: линейная аппроксимация методом наименьших квадратов.

## Обновление

- Основное: пункт меню «Обновление», DinMeter поднимает свою точку Wi-Fi, в браузере 192.168.4.1 загружается .bin.
- Запасное: USB-C на DinMeter.
- Точка Wi-Fi: `WranglerAir`, пароль `jeepjeep` (меняется в `config.h`).
- Две копии программы (OTA-разделы) и откат при неудачном старте.
- На время обновления G2 и G13 LOW.
- Калибровка и поправки в отдельной области памяти, переживают обновление.

## 0.2.0: fewer compressor starts

- Flow offset while pumping: **AUTO** learns `offset = c × rise rate`; **MANUAL** uses a fixed value. Menu → OFFSET (AUTO or 0.05-1.50 bar).
- Settle before a measurement: **AUTO** waits until the reading is steady (1-6 s); **MANUAL** uses a fixed time. Menu → SETTLE (AUTO or 1.0-10.0 s).
- The stop margin adapts to how well earlier stops landed. Top-up time is learned separately.
- Timers on screen (tenths of a second): the whole run time and the number of starts at the top, a countdown on pauses (settle, measure, learning pulse, top-up), a stopwatch while pumping. Calibration shows a countdown on the pause and a stopwatch while pumping.
- Calibration result reads "Sensor reads 1.2% low / Zero: 0.02 bar high / Accuracy +/-0.02 bar" instead of a= b=.
- VERSION shows the learned values: off, c, rate, top, bias, dev, settle, starts.

Simulation (4 tyres, target 2.3 bar, 3 rounds of 0.2 / 1.0 / 1.8 bar starts), compressor starts:

| start | v0.1 | v0.2 round 1 | round 2 | round 3 |
|---|---|---|---|---|
| 0.2 bar | 3 → 2 → 2 | 3 | 2 | 1 |
| 1.0 bar | 2 | 2 | 2 | 1 |
| 1.8 bar | 2 | 2 | 1 | 1 |
