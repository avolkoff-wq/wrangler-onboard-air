# Review request for Claude Code — Wrangler Onboard Air UI refactor

**Repo:** https://github.com/avolkoff-wq/wrangler-onboard-air  
**Branch:** `improvements-wifi-pinout-log-sensor`  
**Date:** 2026-11-21  
**Context:** You authored the original firmware. Another agent restored the truncated `.ino` and finished the UI split. Please verify behaviour and style against your original design.

## What was broken

Commit `43ac4bf` truncated `firmware/wrangler_air/wrangler_air.ino` (~70 KB → ~7 KB) while only stub headers `ui_draw.h` / `ui_screens.h` existed (declarations, no bodies).

## What was done

### 1. Restore `.ino`
- Restored full logic from `origin/main` (`a7d0216` / same blob as SHA you named for the good tree).
- Kept branch docs + `ota.h` Wi‑Fi 10‑minute inactivity timeout.

### 2. UI refactor (header-only)
| File | Role | ~size |
|------|------|-------|
| `ui_draw.h` | Fonts, `drawLines` / hints, scenery, SUV, `drawPresetIcon`, `fmt`/`mmss`/… buffers | ~18 KB |
| `ui_screens.h` | `drawIdle`, `drawMain`, menu/cal/ota/hours/sensor/learn/out-test, `splash` | ~25 KB |
| `wrangler_air.ino` | Globals, arm/engine/noAir, traces, bench, setup/loop | ~34 KB / ~645 lines (was ~1290) |

Style: same comments and drawing logic moved, not rewritten. Shared state via `extern` + enums defined in `.ino` before the includes.

### 3. Less `String` on hot paths
- `fmt`, `mmss`, `phaseTimer`, `offText`, `setText`, `hmm`, `rotName` → rotating `char[48]` scratch buffers + `snprintf`, return `const char*`.
- Call sites that concatenated literals wrap with `String("…") + …` so pointer arithmetic is not introduced.

### 4. Extra (optional from the brief)
- **Error beeps:** `beepErr(PErr)` — SENSOR triple short, LEAK double long, OVER / TIMEOUT / LOW_V distinct.
- **Wi‑Fi password menu:** `WIFI PASSWORD` → NVS `settings.wifiPass[9]`, used by `Ota::start(..., pass)`.
- **Sensor probe at inflate start:** if `p0 < 0.08` bar, 1.5 s compressor pulse + 0.8 s settle; no rise → SENSOR beep + brief message (does **not** go through `Pump` error state).

### 5. Explicitly not done
- Hardware interlock preventing compressor + valve together (per brief).

## Build check
```text
cd firmware && pio run -e dinmeter
→ SUCCESS (ESP32-S3 image linked)
```
RAM ~16.8%, Flash ~39.3%. Narrowing warnings in `drawLines({…})` colour ternaries are pre-existing style.

## Please verify

1. **Compile** in Arduino IDE (M5DinMeter / StampS3) as well as PlatformIO.
2. **Idle / run / menu** screens look and behave like before the truncate (presets, scenery, hold-for-menu, two/three-press arm, LEARN, OUTPUT TEST, HOURS, SENSOR, CUSTOM wheels).
3. **Logs:** ARM_CANCEL, RUN, WIFI timeout=1, SENSOR_PROBE, WIFI_PASS, FILTER_RESET unchanged aside from new events.
4. **Header-only split:** OK for you long-term, or prefer `.cpp` + thinner headers?
5. **Sensor probe:** blocking `delay` in `loop` before `pump.start()` — keep, move into `Pump`, or drop?
6. **Wi‑Fi password:** charset A–Z/0–9 only, 8 chars; default still `jeepjeep`.
7. Any accidental logic drift in `drawMain` / `drawIdle` / arm path vs your last good `.ino`.

## Diff focus
```bash
git fetch origin
git log origin/main..origin/improvements-wifi-pinout-log-sensor --oneline
git diff origin/main...origin/improvements-wifi-pinout-log-sensor -- firmware/wrangler_air/
```

Thanks — flag anything that should be reverted or reshaped to match your conventions.
