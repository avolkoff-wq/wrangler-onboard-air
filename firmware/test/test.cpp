#include "config.h"
uint32_t g_ms=0; int g_pins[64]={0};
inline void analogReadResolution(int){} inline void analogSetPinAttenuation(int,int){} 
#define ADC_11db 0
inline uint32_t analogReadMilliVolts(int){ return 0; }   // DEMO reads the virtual tyres, the ADC is never used
#include "sensor.h"
#include "storage.h"
#include "pump.h"
#include "trace.h"
#include "guards.h"
// run with a trace like the firmware does (traceStep): one row per 0.5 s, a marker after each static measurement
static String runTraced(Pump& p, Sensor& s, Settings& st, const char* preset) {
  Trace tr; tr.begin(g_ms); PState prev = p.state(); uint32_t t0 = g_ms;
  while (p.busy() && g_ms - t0 < 20*60*1000) {
    g_ms += 20; s.update(); p.update();
    const PState ps = p.state();
    char ph = '-';
    switch (ps) { case PState::SETTLE: ph='S'; break; case PState::MEASURE: ph='E'; break; case PState::CAL_PULSE: ph='C'; break;
                  case PState::MAIN: ph='M'; break; case PState::TOPUP: ph='T'; break; case PState::VENT: ph='V'; break; default: break; }
    tr.sample(g_ms, s.bar(st.calA, st.calB), p.estimate(), ph, p.compressorOn(), p.valveOpen());
    if (prev == PState::MEASURE && ps != PState::MEASURE) tr.marker(g_ms, p.staticBar());
    prev = ps;
  }
  char h[256];
  snprintf(h, sizeof h, "preset=%s;target=%.2f;p0=%.3f;p=%.3f;result=%s;boot=1;t0=%.1f;tyre=255/75R17;starts=%d;vents=%d;dur=%.1f;test=-;",
           preset, st.target, p.p0(), p.staticBar(), p.state() == PState::DONE ? (p.inBand() ? "IN_BAND" : "DONE") : "ERR", t0 / 1000.0, p.starts(), p.vents(), (g_ms - t0) / 1000.0);
  return tr.finish(String(h));
}

// ---------- regression checks for the 0.5.2 review fixes: exit code 1 if any fails ----------
static int g_fail = 0;
static void check(bool ok, const char* what) { printf("  [%s] %s\n", ok ? "ok" : "FAIL", what); if (!ok) g_fail++; }
static void step(Pump& p, Sensor& s, uint32_t ms) { for (uint32_t t = 0; t < ms; t += 20) { g_ms += 20; s.update(); p.update(); } }
static void runToEnd(Pump& p, Sensor& s) { uint32_t t0 = g_ms; while (p.busy() && g_ms - t0 < 10*60*1000) step(p, s, 20); }
static float fakeV(float rawBar) { return rawBar / SENSOR_FS * SENSOR_SPAN; }   // pin volts for a raw (uncalibrated) bar

static int regressions() {
  wlog::verbose = false;
  printf("==== regressions (0.5.2)\n");
  {   // 1: target near the cap: the live line right after a stop carries the flow offset -> used to give ERR OVER
    g_demo = true; g_fakeV = -1; g_valve = true;
    Settings st; Sensor s; Pump p; p.begin(&s, &st);
    Sensor::simP = 1.2f; st.target = 2.4f; s.update(); p.start(); runToEnd(p, s); p.clearResult(); g_ms += 5000;   // learn first
    const float tg[] = {3.0f, 3.0f, 2.9f}, st0[] = {2.0f, 2.5f, 2.6f};
    for (int i = 0; i < 3; i++) {
      Sensor::simP = st0[i]; st.target = tg[i]; s.update(); p.start(); runToEnd(p, s);
      char m[96]; snprintf(m, sizeof m, "%.1f -> %.1f bar ends DONE, no ERR OVER (err %s, tyres %.2f)", st0[i], tg[i], errName(p.error()), Sensor::simP);
      check(p.state() == PState::DONE && Sensor::simP < 3.06f, m);
      p.clearResult(); g_ms += 5000;
    }
  }
  {   // 2: a learned c far too large hid the real pressure: the offset is capped now
    g_demo = true; g_fakeV = -1;
    Settings st; Sensor s; Pump p; p.begin(&s, &st);
    Sensor::simP = 1.2f; st.target = 2.4f; s.update(); p.start(); runToEnd(p, s); p.clearResult(); g_ms += 5000;
    st.c *= 4;   // only c goes wrong: the scalar offset (and so the AUTO cap) is measured at every stop, not from c
    Sensor::simP = 1.0f; st.target = 2.5f; s.update(); p.start(); runToEnd(p, s);
    char m[96]; snprintf(m, sizeof m, "c x4, target 2.5: tyres stay under 3.0 bar (tyres %.2f, cap %.2f, err %s)", Sensor::simP, st.offCap(), errName(p.error()));
    check(Sensor::simP < 3.0f && !p.compressorOn(), m);
    p.clearResult(); g_ms += 5000;
  }
  {   // 3: VALVE switched off while the valve is open: K4 must still close
    g_demo = true; g_fakeV = -1; g_valve = true;
    Settings st; Sensor s; Pump p; p.begin(&s, &st); Sensor::simP = 2.0f; s.update();
    p.pulse(true, IO_HOLD_MS); step(p, s, 200);
    const bool wasOpen = g_pins[PIN_K4] == HIGH;
    g_valve = false; p.stop();
    check(wasOpen && g_pins[PIN_K4] == LOW && !p.valveOpen(), "valve open, VALVE off + stop: K4 LOW");
    g_valve = true;
  }
  {   // 4: a run start cancels an IO TEST / USB output
    g_demo = true; g_fakeV = -1; g_valve = true;
    Settings st; Sensor s; Pump p; p.begin(&s, &st); Sensor::simP = 1.5f; st.target = 2.0f; s.update();
    p.pulse(false, IO_HOLD_MS); step(p, s, 200);
    const bool k1Was = g_pins[PIN_K1] == HIGH;
    p.start(); step(p, s, 40);
    check(k1Was && !p.pulsing() && p.state() == PState::SETTLE && g_pins[PIN_K1] == LOW, "k1 on, then start: pulse ended, K1 off for the start measurement");
    p.stop();
  }
  {   // 5: a valve pulse is not cut above the cap (that is when venting is wanted); a K1 pulse is
    g_demo = true; g_fakeV = -1; g_valve = true;
    Settings st; Sensor s; Pump p; p.begin(&s, &st); Sensor::simP = 3.4f; for (int i = 0; i < 20; i++) s.update();
    p.pulse(true, IO_HOLD_MS); step(p, s, 500);
    check(p.pulsing() && p.valveOpen(), "valve pulse at 3.4 bar keeps running");
    p.stop();
    p.pulse(false, IO_HOLD_MS); step(p, s, 200);
    check(!p.pulsing() && !p.compressorOn(), "K1 pulse at 3.4 bar is cut");
  }
  {   // 6: open signal wire (0 V) with a calibration zero of +0.2 bar: still ERR SENSOR
    g_demo = false; g_valve = true;
    Settings st; st.calB = 0.2f; st.learned = true; Sensor s; Pump p; p.begin(&s, &st);
    g_fakeV = fakeV(1.0f); for (int i = 0; i < 20; i++) s.update();
    st.target = 2.5f; p.start(); step(p, s, 4000);                 // settle + measure, then MAIN
    const bool pumping = p.compressorOn();
    g_fakeV = 0; step(p, s, 4000);
    check(pumping && p.state() == PState::ERROR && p.error() == PErr::SENSOR, "0 V while pumping with calB 0.2: ERR SENSOR");
    p.clearResult(); g_fakeV = -1;
  }
  {   // 7: every error has a name (the trace table used to stop at LOW_P)
    check(String(errName(PErr::LOW_V)) == "LOW_V" && String(errName(PErr::PUMP_ON)) == "PUMP_ON" && String(errName((PErr)42)) == "UNKNOWN", "errName covers all PErr");
  }
  {   // 8: NaN / absurd values from NVS fall back to defaults
    g_valve = true;
    Settings st; st.calA = NAN; st.calB = 0.1f; st.rate = INFINITY; st.tauD = -3; st.custom = 0.2f; st.sanitize();
    check(st.calA == 1.0f && st.calB == 0.0f && st.rate == DEFAULT_RATE && st.tauD == TAU_D_DEFAULT && st.custom >= 0.5f, "sanitize: NaN cal, inf rate, bad tau, custom < 0.5 with VALVE");
  }
  {   // 9: LOW_V during a pulse switches the output off; during a run it is ERR LOW_V
    g_demo = true; g_fakeV = -1; g_valve = true;
    Settings st; Sensor s; Pump p; p.begin(&s, &st); Sensor::simP = 1.5f; s.update();
    p.pulse(false, IO_HOLD_MS); step(p, s, 200); p.lowVoltage();
    check(!p.pulsing() && !p.compressorOn() && p.state() == PState::IDLE, "low voltage during k1 on: output off, no error screen");
  }
  {   // 10: OFFSET MAX: AUTO follows the learned offset, manual value is used as is
    Settings st;
    const float c0 = st.offCap();
    st.learned = true; st.offset = 0.40f; const float c1 = st.offCap();
    st.offset = 0.90f; const float c2 = st.offCap();
    st.offset = 5.0f;  const float c3 = st.offCap();
    st.omAuto = false; st.omMan = 0.8f; const float c4 = st.offCap();
    char m[128]; snprintf(m, sizeof m, "offCap: default %.2f, 0.40 -> %.2f, 0.90 -> %.2f, 5.0 -> %.2f, manual 0.80 -> %.2f", c0, c1, c2, c3, c4);
    check(fabsf(c0 - 0.6f) < 1e-4f && fabsf(c1 - 0.7f) < 1e-4f && fabsf(c2 - 1.45f) < 1e-4f && fabsf(c3 - OFFMAX_HI) < 1e-4f && fabsf(c4 - 0.8f) < 1e-4f, m);
  }
  {   // 11: a real offset larger than the cap only stops early: still DONE at the target, no error
    g_demo = true; g_fakeV = -1; g_valve = true;
    Settings st; Sensor s; Pump p; p.begin(&s, &st);
    Sensor::simP = 1.2f; st.target = 2.4f; s.update(); p.start(); runToEnd(p, s); p.clearResult(); g_ms += 5000;
    st.omAuto = false; st.omMan = 0.30f;                         // cap far below the ~0.35-0.5 sim offset
    Sensor::simP = 1.0f; st.target = 2.4f; s.update(); p.start(); runToEnd(p, s);
    char m[112]; snprintf(m, sizeof m, "cap 0.30 under the real offset: DONE %.2f bar, starts %d, err %s", Sensor::simP, p.starts(), errName(p.error()));
    check(p.state() == PState::DONE && fabsf(Sensor::simP - 2.4f) < 0.1f, m);
    p.clearResult(); g_ms += 5000;
  }
  {   // 15-17 (0.5.4): a failed ADC2 sample is dropped, not averaged in as 0 V; too few confirmed -> fault
    g_demo = false; g_fakeV = -1; adc2::fails = 0;
    adc2::stubMv[PIN_SENSOR] = (int)lroundf(fakeV(2.5f) * 1000.0f);
    Sensor s; s.begin();
    adc2::stubFailOf8 = 1;
    for (int i = 0; i < 50; i++) { g_ms += 20; s.update(); }
    char m[120]; snprintf(m, sizeof m, "1 of 8 ADC samples fail at 2.50 bar: reads %.2f (was 2.19), no fault", s.rawBar());
    check(fabsf(s.rawBar() - 2.5f) < 0.02f && !s.fault(), m);
    adc2::stubFailOf8 = 3;
    for (int i = 0; i < 20; i++) { g_ms += 20; s.update(); }
    const bool early = s.fault();
    for (int i = 0; i < 10; i++) { g_ms += 20; s.update(); }
    snprintf(m, sizeof m, "3 of 8 fail: no fault at 0.4 s, ERR SENSOR fault at 0.6 s, reading kept %.2f", s.rawBar());
    check(!early && s.fault() && fabsf(s.rawBar() - 2.5f) < 0.02f, m);
    check(adc2::fails == 50 * 1 + 30 * 3, "failed samples counted for END / io");
    adc2::stubFailOf8 = 0; adc2::fails = 0;
    for (int i = 0; i < 2; i++) { g_ms += 20; s.update(); }
    check(!s.fault(), "fault clears once the ADC confirms again");
  }
  printf("==== regressions (0.6.0)\n");
  {   // 18-19 (item 2): fewer tyres on the line than learned: the top-up stops on the estimate, never a false DONE
    for (int valve = 0; valve <= 1; valve++) {
      g_demo = true; g_fakeV = -1; g_valve = valve;
      Settings st; Sensor s; Pump p; p.begin(&s, &st);
      Sensor::simK = 1; Sensor::simP = 1.2f; st.target = 2.4f; s.update(); p.start(); runToEnd(p, s); p.clearResult(); g_ms += 5000;   // learn on 4
      Sensor::simK = 4; Sensor::simP = 1.0f; st.target = 1.2f; s.update(); p.start(); runToEnd(p, s);
      const bool ok = p.state() == PState::DONE && (p.offTarget() ? fabsf(p.staticBar() - st.target) > DONE_TOL : fabsf(p.staticBar() - st.target) <= DONE_TOL);
      char m[160]; snprintf(m, sizeof m, "1 wheel, learned on 4, 1.0 -> 1.2, VALVE %d: %s at %.2f bar (was DONE at 1.85), honest result",
                            valve, p.offTarget() ? "OFF TARGET" : "DONE", p.staticBar());
      check(ok && (valve ? !p.offTarget() || p.vents() >= MAX_VENTS : true) && Sensor::simP < 1.85f, m);
      Sensor::simK = 1; p.clearResult(); g_ms += 5000;
    }
  }
  {   // 20 (item 10): target 3.0 with a strongly negative learned bias: no false ERR OVER
    g_demo = true; g_fakeV = -1; g_valve = true;
    Settings st; Sensor s; Pump p; p.begin(&s, &st);
    Sensor::simP = 1.2f; st.target = 2.4f; s.update(); p.start(); runToEnd(p, s); p.clearResult(); g_ms += 5000;
    st.bias = -0.15f;
    Sensor::simP = 2.0f; st.target = 3.0f; s.update(); p.start(); runToEnd(p, s);
    char m[120]; snprintf(m, sizeof m, "bias -0.15, 2.0 -> 3.0: %s, tyres %.2f (was ERR OVER at 2.97)", p.state() == PState::DONE ? "DONE" : errName(p.error()), Sensor::simP);
    check(p.state() == PState::DONE && Sensor::simP < 3.06f, m);
  }
  {   // 21-24 (item 3): idle guards replayed (bench U8 / U9), one alarm per episode
    auto in = [](float p) { GuardIn g = {}; g.now = g_ms; g.idle = true; g.p = p; g.vchkOn = true; g.pchkOn = true; return g; };
    Guards gd; int fired = 0; float p = 1.5f;
    for (int t = 0; t < 12; t++) { g_ms += 1000; gd.update(in(p)); }                       // quiet time
    for (int t = 0; t < 25; t++) { g_ms += 1000; p -= 0.02f; if (gd.update(in(p)) == GuardEv::VALVE_STUCK) fired++; }
    for (int t = 0; t < 25; t++) { g_ms += 1000; p -= 0.02f; if (gd.update(in(p)) == GuardEv::VALVE_STUCK) fired++; }
    check(fired == 1, "VALVE CHECK: 0.02 bar/s fall fires once, no repeat while it keeps falling");
    Guards g2; fired = 0; p = 1.0f; int firedAt = -1;
    { GuardIn busy = in(p); busy.idle = false; g2.update(busy); }                          // a run just ended: quiet time starts
    for (int t = 0; t < 12; t++) { g_ms += 1000; g2.update(in(p)); }
    for (int t = 0; t < 25; t++) { g_ms += 1000; p += 0.04f; if (g2.update(in(p)) == GuardEv::PUMP_STUCK) { fired++; if (firedAt < 0) firedAt = t; } }
    char m[96]; snprintf(m, sizeof m, "PUMP CHECK: 0.04 bar/s rise fires once at ~20 s (at %d s)", firedAt);
    check(fired == 1 && firedAt >= 18 && firedAt <= 22, m);
    for (int t = 0; t < 31; t++) { g_ms += 1000; g2.update(in(p)); }                        // steady 30 s: re-armed
    int highAt = -1;
    for (int t = 0; t < 8; t++) { g_ms += 1000; if (g2.update(in(5.5f)) == GuardEv::PUMP_STUCK && highAt < 0) highAt = t; }
    snprintf(m, sizeof m, "PUMP CHECK: re-armed after 30 s steady, 5.5 bar fires in ~4 s (at %d s, high %d)", highAt, g2.high());
    check(highAt >= 2 && highAt <= 4 && g2.high(), m);
    Guards g3; GuardIn lv = {}; lv.compressorOn = true; lv.voltOk = true; lv.volts = 11.0f; int lowAt = -1;
    for (int t = 0; t < 150; t++) { g_ms += 20; lv.now = g_ms; if (g3.update(lv) == GuardEv::LOW_V && lowAt < 0) lowAt = t * 20; }
    snprintf(m, sizeof m, "battery guard: 11.0 V with the compressor on stops after 2 s (%d ms)", lowAt);
    check(lowAt >= 1980 && lowAt <= 2100, m);
  }
  {   // 25 (item 9): AUX1 hysteresis and trim
    Supply sp; sp.begin();
    adc2::stubFailOf8 = 0;
    auto at = [&](float v) { adc2::stubMv[PIN_VBAT] = (int)lroundf(v / VBAT_DIV * 1000.0f); for (int i = 0; i < 200; i++) { g_ms += 20; sp.update(); } };
    at(13.4f); const bool on1 = sp.engineOn();
    at(13.1f); const bool on2 = sp.engineOn();     // inside the 0.2 V band: still on
    at(12.9f); const bool on3 = sp.engineOn();     // below 13.0: off
    at(13.1f); const bool on4 = sp.engineOn();     // coming up inside the band: still off
    sp.setTrim(1.02f); at(13.0f); const float tv = sp.volts();
    char m[120]; snprintf(m, sizeof m, "AUX1: on 13.4, stays on 13.1, off 12.9, stays off 13.1; trim 1.02 x 13.0 = %.2f V", tv);
    check(on1 && on2 && !on3 && !on4 && fabsf(tv - 13.26f) < 0.03f, m);
  }
  {   // 26: HIGHWAY follows the tyre size
    Settings st; st.preset = 0; st.target = st.presetBar(0);
    const float h255 = st.target; st.setTyre(1); const float h285 = st.target; st.setTyre(0);
    char m[96]; snprintf(m, sizeof m, "HIGHWAY: 255/75 %.1f bar, 285/70 %.1f bar, back %.1f", h255, h285, st.target);
    check(fabsf(h255 - 2.5f) < 1e-4f && fabsf(h285 - 2.3f) < 1e-4f && fabsf(st.target - 2.5f) < 1e-4f, m);
  }
  {   // 27: AUTO settle on a steady line ends in ~1.3 s at any loop pace (bench 24.11: 46-48 ms passes gave 6.0 s)
    g_demo = false; g_valve = true; uint32_t worst = 0; int worstDt = 0;
    for (int dt = 5; dt <= 70; dt++) {
      Settings st; st.learned = true; Sensor s; Pump p; p.begin(&s, &st);
      g_fakeV = fakeV(1.0f); for (int i = 0; i < 40; i++) { g_ms += 20; s.update(); }
      st.target = 2.5f; p.start(); const uint32_t t0 = g_ms;
      while (p.state() == PState::SETTLE && g_ms - t0 < 10000) { g_ms += dt; s.update(); p.update(); }
      if (g_ms - t0 > worst) { worst = g_ms - t0; worstDt = dt; }
      p.stop(); p.clearResult(); g_ms += 5000;
    }
    g_fakeV = -1;
    char m[120]; snprintf(m, sizeof m, "AUTO settle, steady line, loop pass 5..70 ms: longest %.2f s (at %d ms)", worst / 1000.0, worstDt);
    check(worst <= 2000, m);
  }
  {   // 28-32: preset pressures from the menu, kept in NVS per tyre size (0.8.0)
    g_valve = true; Preferences::bytes().clear();
    auto near = [](float a, float b) { return fabsf(a - b) < 1e-4f; };
    Settings a; a.tyre = 0;
    const bool ok0 = a.setPresetBar(2, 1.3f, false);          // GRAVEL on 255/75
    a.tyre = 1; const bool ok1 = a.setPresetBar(2, 1.7f, false); // GRAVEL on 285/70
    a.tyre = 0;
    Settings b; b.load();
    char m[140]; snprintf(m, sizeof m, "presets saved and read back: GRAVEL 255/75 %.2f, 285/70 %.2f, others factory (HIGHWAY %.2f / %.2f)",
                          b.ptab[0][2], b.ptab[1][2], b.ptab[0][0], b.ptab[1][0]);
    check(ok0 && ok1 && near(b.ptab[0][2], 1.3f) && near(b.ptab[1][2], 1.7f) && near(b.ptab[0][0], 2.5f) && near(b.ptab[1][0], 2.3f) && near(b.ptab[0][4], 1.0f), m);

    a.preset = 2; a.target = a.presetBar(2); const float t255 = a.target;
    a.setTyre(1); const float t285 = a.target; a.setTyre(0);
    a.setPresetBar(2, 1.4f, false); const float tNow = a.target;   // the active preset: the target follows at once
    snprintf(m, sizeof m, "tyre size switches the targets: GRAVEL %.2f -> %.2f -> %.2f; editing the active preset moves the target to %.2f",
             t255, t285, a.target, tNow);
    check(near(t255, 1.3f) && near(t285, 1.7f) && near(tNow, 1.4f), m);

    // garbage in NVS: NaN / inf give the factory value, out of range is clamped like custom, a row of another length is ignored
    float bad[Settings::N_PT]; for (int i = 0; i < Settings::N_PT; i++) bad[i] = PRESETS[i].bar;
    bad[0] = NAN; bad[1] = INFINITY; bad[3] = 9.0f; bad[4] = -1.0f;
    Preferences pr; pr.putBytes("pt0", bad, sizeof bad);
    const float shortRow[3] = {0.1f, 0.2f, 0.3f}; pr.putBytes("pt1", shortRow, sizeof shortRow);
    Settings g; g.load();
    snprintf(m, sizeof m, "NVS garbage: NaN %.2f, inf %.2f, 9.0 -> %.2f, -1 -> %.2f, short row -> HIGHWAY %.2f", g.ptab[0][0], g.ptab[0][1], g.ptab[0][3], g.ptab[0][4], g.ptab[1][0]);
    check(near(g.ptab[0][0], 2.5f) && near(g.ptab[0][1], 1.8f) && near(g.ptab[0][3], TARGET_MAX) && near(g.ptab[0][4], TARGET_MIN) &&
          near(g.ptab[1][0], 2.3f) && near(g.ptab[1][2], 1.5f), m);

    Settings r; r.ptab[0][1] = 2.0f; r.ptab[1][5] = 1.1f; r.custom = 2.7f; r.preset = 1; r.target = 2.0f;
    const bool rs = r.resetPresets(false);
    snprintf(m, sizeof m, "RESET ALL: ROUGH ROAD %.2f, SAND 285/70 %.2f, CUSTOM %.2f, target %.2f", r.ptab[0][1], r.ptab[1][5], r.custom, r.target);
    check(rs && near(r.ptab[0][1], 1.8f) && near(r.ptab[1][5], 0.8f) && near(r.custom, 2.0f) && near(r.target, 1.8f), m);

    Settings busy; busy.preset = 3; busy.target = busy.presetBar(3); busy.ptab[0][1] = 2.0f;
    const bool e1 = busy.setPresetBar(3, 2.2f, true), e2 = busy.setPresetBar(P_CUSTOM, 2.9f, true), e3 = busy.resetPresets(true);
    snprintf(m, sizeof m, "while the pump works an edit is refused: TRAIL %.2f, target %.2f, CUSTOM %.2f, reset kept ROUGH ROAD %.2f",
             busy.ptab[0][3], busy.target, busy.custom, busy.ptab[0][1]);
    check(!e1 && !e2 && !e3 && near(busy.ptab[0][3], 1.2f) && near(busy.target, 1.2f) && near(busy.custom, 2.0f) && near(busy.ptab[0][1], 2.0f), m);
    Preferences::bytes().clear();
  }
  printf("==== regressions: %s (%d failed)\n", g_fail ? "FAILED" : "all ok", g_fail);
  return g_fail ? 1 : 0;
}

int main(int argc,char**argv){
  g_demo = true; g_valve = true;   // virtual tyres, valve fitted (menu DEMO / VALVE in the firmware)
  String example; int exampleScore = -1;
  Settings st; Sensor s; Pump p; p.begin(&s,&st);
  float starts[]={2.4,2.4,2.4,1.2,0.9,2.3}; float targets[]={1.2,0.8,1.2,2.4,2.4,2.25};
  for(int r=0;r<6;r++){
    Sensor::simP=starts[r]; st.target=targets[r]; s.update();
    printf("==== run %d: tyres %.2f -> target %.2f  (tauD %.1f learned %d)\n",r,starts[r],targets[r],st.tauD,st.learnedD);
    p.start(); uint32_t t0=g_ms;
    const String tr = runTraced(p, s, st, targets[r] < starts[r] ? "SAND" : "HIGHWAY");
    const int score = (p.starts() > 0) + (p.vents() > 0) * 2;   // example: prefer a run that used the valve and the compressor
    if (score > exampleScore) { exampleScore = score; example = tr; }
    printf("  trace %d bytes\n", (int)tr.length());
    printf("  -> state %d err %d final static %.3f real %.3f  time %.0fs starts %d vents %d\n",(int)p.state(),(int)p.error(),p.staticBar(),Sensor::simP,(g_ms-t0)/1000.0,p.starts(),p.vents());
    p.clearResult(); g_ms+=5000;
  }
  // example graph page for the docs (argv[1] = output path)
  if (argc > 1) {
    FILE* f = fopen(argv[1], "w");
    if (f) {
      fputs(TRACE_GRAPH_HTML_HEAD, f);
      fputs("<p style=color:#666>Example from the desk simulation (firmware/test/run.sh), same page as on the DinMeter Wi-Fi page /run?id=...</p>\n"
            "<script type=text/plain id=data>\n", f);
      fputs(example.c_str(), f);
      fputs("</script>\n<script>draw(document.getElementById('data').textContent,false);</script>\n", f);
      fclose(f); printf("example graph written: %s\n", argv[1]);
    }
  }
  // menu LEARN: one fixed valve opening / compressor pulse between two static measurements
  struct { float start; Mode m; const char* name; } lt[] = { {2.4f, Mode::DEF, "LEARN DEFLATE"}, {1.2f, Mode::DEF, "LEARN DEFLATE below 1.5 (expect err 5 LOW_P)"}, {1.0f, Mode::INF, "LEARN INFLATE"} };
  for (auto& t : lt) {
    Sensor::simP=t.start; s.update();
    printf("==== %s: tyres %.2f  (tauD %.1f rate %.4f)\n", t.name, t.start, st.tauD, st.rate);
    p.learnTest(t.m); uint32_t t0=g_ms;
    while(p.busy() && g_ms-t0<5*60*1000){ g_ms+=20; s.update(); p.update(); }
    printf("  -> state %d err %d ok %d result %.4f  before %.3f after %.3f  tauD %.1f rate %.4f  time %.0fs\n",(int)p.state(),(int)p.error(),p.testOk(),p.testResult(),p.testBefore(),p.staticBar(),st.tauD,st.rate,(g_ms-t0)/1000.0);
    p.clearResult(); g_ms+=5000;
  }
  return regressions();
}
