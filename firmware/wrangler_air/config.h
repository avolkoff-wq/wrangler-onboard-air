// Wrangler Onboard Air - stage 1 (K1 drives the compressor relay K2)
// All tunable numbers live here.
#pragma once
#include <Arduino.h>

#define FW_VERSION "0.2.0-stage1"

// ---------- pins (M5Stack DinMeter v1.1, StampS3A) ----------
constexpr int PIN_SENSOR   = 1;   // G1, PORT.B: sensor through 10k/20k divider
constexpr int PIN_K1       = 2;   // G2, PORT.B: K1 PhotoMOS -> K2 compressor relay
constexpr int PIN_K4       = 13;  // G13, PORT.A: K4 (stage 3, deflate). Kept LOW in stage 1
constexpr int PIN_ENC_A    = 41;
constexpr int PIN_ENC_B    = 40;
constexpr int PIN_BTN      = 42;  // encoder push, active LOW
constexpr int PIN_HOLD     = 46;  // power hold, HIGH = stay on

// ---------- sensor ----------
constexpr float DIVIDER      = 1.5f;   // V_sensor = V_G1 * 1.5  (R1 10k series, R2 20k to GND)
constexpr float SENSOR_V0    = 0.5f;   // 0 bar
constexpr float SENSOR_SPAN  = 4.0f;   // 0.5..4.5 V
constexpr float SENSOR_FS    = 6.895f; // 100 psi full scale, bar
constexpr float G1_MIN_V     = 0.25f;  // below: wire broken / sensor missing
constexpr float G1_MAX_V     = 3.05f;  // above: sensor fault / over range

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
