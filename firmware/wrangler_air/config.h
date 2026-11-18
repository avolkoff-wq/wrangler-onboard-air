// Wrangler Onboard Air - stage 1 (K1 drives the compressor relay K2)
// All tunable numbers live here.
#pragma once
#include <Arduino.h>

#ifndef BENCH_USB
#define BENCH_USB 0     // 1 = bench builds only (dinmeter-bench, *-sim): USB serial log mirror, LIVE stream, commands
#endif
#ifndef DEFLATE
#define DEFLATE 0       // 1 = schematic C: Unit 2Relay, channel 2 (PORT.B white, G1) opens the deflate valve. Only with SENSOR_DIRECT
#endif
#if SENSOR_DIRECT && DEFLATE && BENCH_USB && !BENCH_SIM
#define FW_VERSION "0.4.0-bench"
#elif SENSOR_DIRECT && DEFLATE
#define FW_VERSION "0.4.0-deflate"
#elif SENSOR_DIRECT
#define FW_VERSION "0.4.0-direct"
#else
#define FW_VERSION "0.4.0-stage1"
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
#if DEFLATE
constexpr int PIN_K4       = 1;   // G1, PORT.B white: Unit 2Relay channel 2 -> deflate valve -Y1 (schematic C)
#else
constexpr int PIN_K4       = -1;  // PORT.A is taken by the sensor: no deflate output in this build
#endif
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
constexpr float TARGET_MIN       = DEFLATE ? 0.5f : 0.0f;   // bar, lowest target (deflate build: 0.5)
constexpr float TARGET_LOW_WARN  = 0.8f;   // below this the screen shows LOW: bead may come off without beadlocks
constexpr float START_BAND       = 0.10f;  // at start: within +/-0.1 bar of the target nothing is done
constexpr float TARGET_STEP      = 0.1f;
// Typical pressures: the knob steps through these instead of 0.1 bar. Latin names only (screen font).
// 255/75R17 without beadlocks. Below 0.8 bar the screen shows LOW.
struct Preset { const char* name; float bar; };
constexpr Preset PRESETS[] = {      // owner's list, 18.11.2026, high to low; index 0 = default
  {"HIGHWAY",    2.5f}, // 36 psi: factory door placard 37 psi (LT255/75R17 and LT285/70R17), Swiss motorway
  {"ROUGH ROAD", 1.8f}, // 26 psi: broken asphalt, comfort
  {"GRAVEL",     1.5f}, // 22 psi: gravel, dirt roads, washboard
  {"TRAIL",      1.2f}, // 17 psi: trails, mud, snow
  {"ROCK",       1.0f}, // 15 psi: rocks, slow crawling
  {"SAND",       0.8f}, // 12 psi: sand, dunes
  {"RECOVERY",   0.5f}, //  7 psi: stuck in soft sand, walking pace only
  {"CUSTOM",     2.0f}, // own value, set with the knob in 0.1 bar steps (NVS "custom"); 2.0 is only the default
};
constexpr int N_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);
constexpr int P_CUSTOM  = N_PRESETS - 1;   // the last preset is CUSTOM
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
constexpr float    FILTER_SERVICE_H = 10.0f; // compressor hours between filter services: then idle shows CLEAN FILTERS
constexpr uint32_t OUTPUT_PULSE_MS = 1000;  // menu OUTPUT TEST: one pulse on K1 or K4, then off
constexpr uint32_t LEARN_DEF_MS  = 20000;  // menu LEARN DEFLATE: valve open exactly this long
constexpr float    LEARN_DEF_MIN_BAR = 1.5f; // LEARN DEFLATE needs at least this static pressure
constexpr float    LEARN_MIN_BAR = 0.15f;   // LEARN needs air in the system
constexpr uint32_t LEAK_WINDOW_MS= 60000;  // must rise 0.1 bar within this
constexpr float    LEAK_MIN_RISE = 0.10f;
constexpr uint32_t RUN_TIMEOUT_MS= 8UL*60UL*1000UL;
constexpr int      MAX_TOPUPS    = 3;
constexpr float    TOPUP_MIN_S   = 0.6f;
constexpr float    TOPUP_MAX_S   = 20.0f;
constexpr float    LEARN_ALPHA   = 0.3f;   // new = 0.7*old + 0.3*measured

// ---------- air model: start values from the tyre size, then the learning takes over ----------
// Two tyre sizes, picked in the menu (TYRE). Air volume ~ annulus x width x fill factor: 255/75R17 ~52 L, 285/70R17 ~62 L each.
struct TyreSize { const char* name; float w, ar, rim; };
constexpr TyreSize TYRES[] = { {"255/75R17", 255, 75, 17}, {"285/70R17", 285, 70, 17} };
constexpr int   N_TYRE_SIZES = sizeof(TYRES) / sizeof(TYRES[0]);
constexpr int   N_TYRES     = 4;
constexpr float TYRE_FILL   = 0.55f;                       // torus shape vs the full annulus box
constexpr float airVL(int i) {                             // all 4 tyres, litres
  return N_TYRES * TYRE_FILL * 3.14159f / 4 *
         (((TYRES[i].rim * 25.4f + 2 * TYRES[i].w * TYRES[i].ar / 100) / 1000) * ((TYRES[i].rim * 25.4f + 2 * TYRES[i].w * TYRES[i].ar / 100) / 1000)
          - (TYRES[i].rim * 25.4f / 1000) * (TYRES[i].rim * 25.4f / 1000)) * (TYRES[i].w / 1000) * 1000;
}
constexpr float AIR_V_L     = airVL(0);                    // default size, ~210 L
// ARB CKMTA12: 174.3 L/min free air at 0 bar, 131.7 L/min at 2 bar (ARB data), linear in between
constexpr float ARB_Q0_LPM  = 174.3f, ARB_Q2_LPM = 131.7f;
constexpr float P_ATM       = 1.013f;                      // bar
constexpr float inflRate(float p, float vL) { return (ARB_Q0_LPM + (ARB_Q2_LPM - ARB_Q0_LPM) * p / 2) / 60 * P_ATM / vL; }  // bar/s
// Deflate: 4 valve cores in parallel and the 4 mm valve in series, choked flow -> p_abs(t) = p_abs0 * exp(-t / tau)
constexpr float DEFL_AREA_MM2 = 5.9f;                      // effective flow area (cores ~8 mm2 with the valve ~8.8 mm2 in series)
constexpr float tauModel(float vL) { return (vL / 1000) / (0.0404f * DEFL_AREA_MM2 * 1e-6f * 287 * 17.12f); }   // s
constexpr float TAU_D_DEFAULT = tauModel(AIR_V_L);         // ~170 s for 4 x 255/75R17
constexpr float VENT_FIRST_FRAC = 0.6f;    // first vent before anything is learned: only 60 % of the drop
constexpr float VENT_MIN_S    = 0.3f;
constexpr float VENT_MAX_S    = 90.0f;     // one valve opening never longer than this
constexpr int   MAX_VENTS     = 4;

// defaults until the first run teaches better values (4 tyres at once)
constexpr float DEFAULT_OFFSET = 0.40f;    // bar, dynamic minus static
constexpr float DEFAULT_RATE   = inflRate(1.5f, AIR_V_L);   // bar/s, ~0.011 from the air model

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
constexpr float SIM_TAU_D     = 140.0f; // virtual deflate time constant (differs from the model on purpose: learning test)
constexpr float SIM_C_D       = 25.0f;  // sensor reads low while venting: tyres - SIM_C_D * fall rate
constexpr float SIM_SPINUP_S  = 0.7f;   // compressor spin-up / run-down time constant
constexpr float SIM_NOISE     = 0.004f; // sensor noise, bar

// ---------- OTA access point ----------
#define OTA_SSID "WranglerAir"
#define OTA_PASS "jeepjeep"
constexpr size_t LOG_MAX_BYTES = 200000;  // log.csv is moved to log_old.csv at this size (flash partition, survives power-off)
