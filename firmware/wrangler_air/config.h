// Wrangler Onboard Air - stage 1 (K1 drives the compressor relay K2)
// All tunable numbers live here.
#pragma once
#include <Arduino.h>

#if SENSOR_DIRECT
#define FW_VERSION "0.3.0-stage1-direct"
#else
#define FW_VERSION "0.3.0-stage1"
#endif

// ---------- sensor variant ----------
// 0 = 0.5-4.5 V sensor on G1 (PORT.B) through a 10k/20k divider (original board)
// 1 = 0-3.3 V sensor straight on G13 (PORT.A yellow), 2 m Grove cable, no ADC, no divider
//     (schematic A, hardware/enclosure/hammond_1591c/power_schematic.png). Build: pio run -e dinmeter-direct
#ifndef SENSOR_DIRECT
#define SENSOR_DIRECT 0
#endif

// ---------- pins (M5Stack DinMeter v1.1, StampS3A) ----------
constexpr int PIN_K1       = 2;   // G2, PORT.B yellow: M5 Mini Relay (DIN) / K1 -> compressor
#if SENSOR_DIRECT
constexpr int PIN_SENSOR   = 13;  // G13, PORT.A yellow: sensor output 0-3.3 V (ADC2: not read while Wi-Fi is on, OTA only)
constexpr int PIN_K4       = -1;  // PORT.A is taken by the sensor: no deflate output in this build
#define SENSOR_PIN_NAME "G13"
#else
constexpr int PIN_SENSOR   = 1;   // G1, PORT.B: sensor through 10k/20k divider
constexpr int PIN_K4       = 13;  // G13, PORT.A: K4 (stage 3, deflate). Kept LOW in stage 1
#define SENSOR_PIN_NAME "G1"
#endif
constexpr int PIN_ENC_A    = 41;
constexpr int PIN_ENC_B    = 40;
constexpr int PIN_BTN      = 42;  // encoder push, active LOW
constexpr int PIN_HOLD     = 46;  // power hold, HIGH = stay on

// ---------- sensor ----------
#if SENSOR_DIRECT
// G1/8 ceramic sensor 0-1.2 MPa, 5 V supply, output 0-3.3 V (AliExpress 1005004559608411, "output 0-3.3V")
constexpr float DIVIDER      = 1.0f;   // no divider
constexpr float SENSOR_V0    = 0.0f;   // 0 bar = 0 V: a broken wire cannot be told from 0 bar by voltage,
constexpr float SENSOR_SPAN  = 3.3f;   //   so the pin has a pull-down and the pump checks plausibility (DEAD_*)
constexpr float SENSOR_FS    = 12.0f;  // 1.2 MPa full scale, bar
constexpr float G1_MIN_V     = -1.0f;  // no low limit
constexpr float G1_MAX_V     = 3.10f;  // ADC ceiling at 11 dB (~11.3 bar): above = fault
#else
constexpr float DIVIDER      = 1.5f;   // V_sensor = V_G1 * 1.5  (R1 10k series, R2 20k to GND)
constexpr float SENSOR_V0    = 0.5f;   // 0 bar
constexpr float SENSOR_SPAN  = 4.0f;   // 0.5..4.5 V
constexpr float SENSOR_FS    = 6.895f; // 100 psi full scale, bar
constexpr float G1_MIN_V     = 0.25f;  // below: wire broken / sensor missing
constexpr float G1_MAX_V     = 3.05f;  // above: sensor fault / over range
#endif
// plausibility while the compressor runs: the line is never near 0 bar once air flows into the tyres
constexpr uint32_t DEAD_AFTER_MS = 3000;   // compressor on this long...
constexpr float    DEAD_BAR      = 0.10f;  // ...and the sensor still reads below this: wire broken, stop

// ---------- control ----------
constexpr float TARGET_MAX       = 3.0f;   // bar, hard cap for the program
constexpr float TARGET_STEP      = 0.1f;
constexpr float STOP_MARGIN      = 0.10f;  // stop margin before anything is learned
constexpr float MARGIN_MIN       = 0.03f;  // adaptive margin limits
constexpr float MARGIN_MAX       = 0.15f;
constexpr float DONE_TOL         = 0.05f;  // static result within this is "done" (gauge accuracy)
constexpr uint32_t SETTLE_MS     = 2000;   // default manual settle time
constexpr uint32_t SETTLE_MIN_MS = 1000;   // auto settle: never shorter
constexpr uint32_t SETTLE_MAX_MS = 6000;   // auto settle: never longer
constexpr float    SETTLE_SLOPE  = 0.005f; // auto settle: steady when slower than this, bar/s
constexpr uint32_t MEASURE_MS    = 800;    // averaging window for a static measurement
constexpr uint32_t RATE_WINDOW_MS= 2500;   // window for the live rise rate while pumping
constexpr uint32_t RATE_VALID_MS = 3000;   // rate is trusted this long after the compressor starts
constexpr uint32_t CAL_PULSE_MS  = 15000;  // first-ever run: calibration pulse
constexpr uint32_t LEAK_WINDOW_MS= 60000;  // must rise 0.1 bar within this
constexpr float    LEAK_MIN_RISE = 0.10f;
constexpr uint32_t RUN_TIMEOUT_MS= 8UL*60UL*1000UL;
constexpr int      MAX_TOPUPS    = 3;
constexpr float    TOPUP_MIN_S   = 0.6f;
constexpr float    TOPUP_MAX_S   = 20.0f;
constexpr float    LEARN_ALPHA   = 0.3f;   // new = 0.7*old + 0.3*measured

// defaults until the first run teaches better values (4 tyres at once)
constexpr float DEFAULT_OFFSET = 0.40f;    // bar, dynamic minus static
constexpr float DEFAULT_RATE   = 0.05f;    // bar/s

// ---------- bench simulation ----------
// 1 = no sensor needed: a virtual 4-tyre system answers K1. For desk testing only.
#ifndef BENCH_SIM
#define BENCH_SIM 0
#endif
// physics of the virtual system (see sensor.h)
constexpr float SIM_START_BAR = 1.0f;   // tyres at power-up
constexpr float SIM_RATE      = 0.07f;  // bar/s at 0 bar with full flow, 4 tyres
constexpr float SIM_PMAX      = 7.5f;   // flow falls linearly to zero at this pressure
constexpr float SIM_C         = 7.0f;   // flow offset = SIM_C * rise rate (hose + valve cores)
constexpr float SIM_SPINUP_S  = 0.7f;   // compressor spin-up / run-down time constant
constexpr float SIM_NOISE     = 0.004f; // sensor noise, bar

// ---------- OTA access point ----------
#define OTA_SSID "WranglerAir"
#define OTA_PASS "jeepjeep"
