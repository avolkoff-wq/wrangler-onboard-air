#include "config.h"
uint32_t g_ms=0; int g_pins[64]={0};
inline void analogReadResolution(int){} inline void analogSetPinAttenuation(int,int){} 
#define ADC_11db 0
inline uint32_t analogReadMilliVolts(int){ return 0; }   // DEMO reads the virtual tyres, the ADC is never used
#include "sensor.h"
#include "storage.h"
#include "pump.h"
#include "trace.h"
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
    st.c *= 4; st.offset *= 4;
    Sensor::simP = 1.0f; st.target = 2.5f; s.update(); p.start(); runToEnd(p, s);
    char m[96]; snprintf(m, sizeof m, "c x4, target 2.5: tyres stay under 3.3 bar (tyres %.2f, err %s)", Sensor::simP, errName(p.error()));
    check(Sensor::simP < 3.3f && !p.compressorOn(), m);
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
