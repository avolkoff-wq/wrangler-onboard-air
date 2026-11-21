# Wrangler Onboard Air — единый отчёт о работе

**Для:** Claude Code (автор исходной прошивки) — ревью и проверка  
**Репозиторий:** https://github.com/avolkoff-wq/wrangler-onboard-air  
**Ветка:** `improvements-wifi-pinout-log-sensor`  
**Tip:** HEAD ветки `improvements-wifi-pinout-log-sensor` (этот отчёт — последний docs-коммит)  
**UI-коммит:** `d9a3f98`  
**PR:** https://github.com/avolkoff-wq/wrangler-onboard-air/pull/1  
**Дата:** 2026-11-21, ~23:05 Europe/Zurich  

---

## 1. Исходная задача (бриф)

1. Восстановить `firmware/wrangler_air/wrangler_air.ino` из `main` (файл был случайно урезан).
2. Рефакторинг UI: `ui_draw` (иконки/scenery/примитивы) + `ui_screens` (экраны); `.ino` оставить тонким (`setup` / `loop` / энкодер).
3. Уменьшить `String` в горячих местах (`fmt`, `mmss`, `phaseTimer`, `offText`, `setText` и т.п.).
4. По возможности: разные бипы на ошибки; пароль Wi‑Fi из меню (NVS); короткий тест датчика при старте, если `p0 ≈ 0`.
5. **Не делать:** аппаратную блокировку одновременного включения компрессора и клапана.

---

## 2. Что было сломано

На ветке после stub-коммитов (`96f3c87`, `6d4cef5`) коммит `43ac4bf` **урезал** `wrangler_air.ino` (~70 KB / ~1290 строк → ~7 KB), оставив только объявления в `ui_draw.h` / `ui_screens.h` без тел. Проект не собирался и не работал.

---

## 3. Что сделано по коду

### 3.1. Восстановление и UI-split

| Файл | Назначение | Размер |
|------|------------|--------|
| `ui_draw.h` | Шрифты, `drawLines`/hints, scenery, SUV, `drawPresetIcon`, formatters | ~365 строк / ~18 KB |
| `ui_screens.h` | `drawIdle`, `drawMain`, menu/cal/ota/hours/sensor/learn/out-test, `splash` | ~489 строк / ~25 KB |
| `wrangler_air.ino` | Globals, arm/engine/noAir, traces, bench, `setup`/`loop` | **~645 строк / ~34 KB** (было ~1290 / ~70 KB) |

Подход: **header-only** (удобно для Arduino IDE). Логика и комментарии перенесены, не переписаны с нуля. Общее состояние — через `extern`; enum’ы объявлены в `.ino` до `#include` UI-заголовков.

Сохранены уже бывшие на ветке изменения: pinout, bench checklist, заметка про температуру, **Wi‑Fi inactivity timeout 10 мин** в `ota.h` (`WIFI timeout=1`).

### 3.2. Меньше `String` на горячих путях

`fmt`, `mmss`, `phaseTimer`, `offText`, `setText`, `hmm`, `rotName` → вращающиеся `char[48]` + `snprintf`, возвращают `const char*`.  
Там, где нужна склейка с литералами — `String("…") + …`, чтобы не получить арифметику указателей.

### 3.3. Дополнительные фичи

| Фича | Как |
|------|-----|
| Бипы ошибок | `beepErr(PErr)`: SENSOR — тройной короткий; LEAK — двойной длинный; OVER / TIMEOUT / LOW_V — свои паттерны |
| Wi‑Fi пароль | Пункт меню **WIFI PASSWORD**; `settings.wifiPass[9]` в NVS; `Ota::start(psi, pass)` |
| Sensor probe | Перед `pump.start()` при inflate, если `p0 < 0.08`: импульс компрессора 1.5 s + 0.8 s settle; нет роста → бип SENSOR + короткое сообщение (**не** через `Pump` error state; есть блокирующие `delay`) |

### 3.4. Явно не делалось

Аппаратная блокировка «компрессор + клапан вместе».

---

## 4. Порядок в git

### До

- На remote торчали stub-коммиты и truncate `.ino` (`43ac4bf`).
- Локальный фикс сначала не пушился (не было write-токена), потом remote был `ahead` с битой историей.

### Сделано

1. **Soft-reset** на `f1deb98` (последний «хороший» docs-коммит до stub’ов).
2. Один чистый коммит UI-рефакторинга: `d9a3f98`.
3. Docs — этот единый отчёт (HEAD).
4. **`git push --force-with-lease`** на `origin/improvements-wifi-pinout-log-sensor`.
5. Working tree clean; local == remote.

### История ветки сейчас (`origin/main` → tip)

```
865e63e  docs: final pinout schematic C
4cbd739  ota: 10-minute inactivity timeout
39e7227  docs: bench checklist
f1deb98  docs: temperature not compensated
d9a3f98  Refactor UI into ui_draw/ui_screens; restore full sketch logic
<HEAD>   docs: single consolidated work + git-cleanup report (this file)
```

Мусорные stub/truncate-коммиты **удалены из истории ветки** (force-with-lease).  
Открытый PR #1 подтянул новый tip; title/body PR агент **не смог** обновить (нет прав у integration-токена) — при желании обновить вручную.

Дифф vs `main` (кратко): `+1112 / -905` по `firmware/` + `docs/`.

---

## 5. Сборка

```bash
cd firmware && pio run -e dinmeter
# → SUCCESS (ESP32-S3 image)
# RAM ~16.8%, Flash ~39.3%
```

Артефакт: `firmware/.pio/build/dinmeter/firmware.bin` (в `.gitignore`, локально собирался).  
Предупреждения narrowing в тернарниках цветов `drawLines({…})` — прежний стиль, не регрессия рефакторинга.

---

## 6. Что проверить Claude Code

1. Сборка в **Arduino IDE** (M5DinMeter / StampS3), не только PlatformIO.
2. Экраны idle / run / menu / LEARN / OUTPUT TEST / HOURS / SENSOR / CUSTOM — поведение как до truncate.
3. Логи: `ARM_CANCEL`, `RUN`, `WIFI timeout=1`, плюс новые `SENSOR_PROBE`, `WIFI_PASS`.
4. Header-only split — ок надолго или лучше `.cpp` + тонкие `.h`?
5. Sensor probe с `delay` в `loop` — оставить, перенести в `Pump`, или убрать?
6. Wi‑Fi пароль: A–Z/0–9, 8 символов; дефолт `jeepjeep`.
7. Нет ли скрытого дрейфа логики в `drawMain` / `drawIdle` / arm-path vs твой последний хороший `.ino` на `main`.

### Команды

```bash
git fetch origin
git checkout improvements-wifi-pinout-log-sensor
git log origin/main..HEAD --oneline
git diff origin/main...HEAD -- firmware/wrangler_air/
cd firmware && pio run -e dinmeter
```

---

## 7. Итог одной строкой

Урезанный sketch восстановлен, UI вынесен в `ui_draw.h` / `ui_screens.h`, горячие `String` ужаты, добавлены бипы / Wi‑Fi password / sensor probe; битая git-история схлопнута и запушена на `improvements-wifi-pinout-log-sensor` (UI: `d9a3f98`), сборка PlatformIO зелёная.
