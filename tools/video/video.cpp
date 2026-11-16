// Runs the real firmware logic (calibration.h, pump.h) on the virtual tyres and logs
// everything the screen shows, every 100 ms, for the video.
#include <cstdio>
#include <cmath>
#include "config.h"
#include "storage.h"
#include "sensor.h"
#include "pump.h"
#include "calibration.h"

static FILE* f;
static Sensor s; static Settings st;
static void tick(int n = 1) { for (int i = 0; i < n; i++) { g_ms += 10; s.update(); } }

int main() {
  f = fopen("timeline.csv", "w");
  fprintf(f, "scene,t,state,idx,shown,est,static_bar,raw,simp,on,starts,run_ms,phase_ms,phase_len,pause_left,entry,a,b,fit,action\n");
  std::srand(7);
  s.begin(); st.target = 2.3f; st.learned = false;

  // ---------- scene 1: calibration ----------
  Calibration cal; cal.begin(&s, &st);
  Sensor::simP = 0.0f; tick(300);               // hose open to air: sensor sees 0
  cal.start();
  uint32_t t0 = g_ms; const char* act = "";
  auto logc = [&](const char* a) {
    fprintf(f, "cal,%.2f,%d,%d,%.3f,0,0,%.3f,%.3f,%d,0,%u,%u,%u,0,%.2f,%.4f,%.4f,%.3f,%s\n",
            (g_ms - t0) / 1000.0, (int)cal.state(), cal.index(), s.rawBar(), s.rawBar(), Sensor::simP,
            digitalRead(PIN_K1), cal.totalMs(), cal.phaseMs(), (unsigned)Calibration::SETTLE_TOTAL_MS,
            cal.entry(), st.calA, st.calB, cal.fitErr(), a);
  };
  // wait 3 s on "hose open", press
  for (int i = 0; i < 30; i++) { tick(10); cal.update(); logc("read"); }
  cal.press(); logc("press");
  Sensor::simP = 0.8f;                          // hose back on the wheels: tyres at 0.8 bar
  int guard = 0;
  while (cal.state() != CState::DONE && cal.state() != CState::FAILED && guard++ < 20000) {
    tick(10); cal.update();
    if (cal.state() == CState::ENTER) {
      // the driver reads the gauge (sensor reads 1.2 % low and 0.02 high at zero), dials it in, presses
      const float gauge = std::round((s.rawBar() * 1.012f - 0.02f) * 20.0f) / 20.0f;
      for (int k = 0; k < 12; k++) { tick(10); logc("read"); }
      int steps = (int)std::lround((gauge - cal.entry()) / 0.05f);
      while (steps != 0) { int d = steps > 0 ? 1 : -1; cal.turn(d); steps -= d; for (int k = 0; k < 4; k++) { tick(10); logc("turn"); } }
      for (int k = 0; k < 6; k++) { tick(10); logc("read"); }
      cal.press(); logc("press");
    } else logc("");
  }
  for (int i = 0; i < 40; i++) { tick(10); logc("read"); }

  // ---------- scene 2: inflation 0.2 -> 2.3, first run after calibration ----------
  Sensor::simP = 0.2f; Sensor::simFlow = 0; tick(300);
  Pump pm; pm.begin(&s, &st);
  t0 = g_ms;
  auto logp = [&](const char* a) {
    const float sb = s.bar(st.calA, st.calB);
    const PState ps = pm.state();
    const float shown = ps == PState::IDLE ? sb : (pm.estimating() ? pm.estimate() : pm.staticBar());
    fprintf(f, "pump,%.2f,%d,0,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%u,%u,%u,%u,0,%.4f,%.4f,0,%s\n",
            (g_ms - t0) / 1000.0, (int)ps, shown, pm.estimate(), pm.staticBar(), s.rawBar(), Sensor::simP,
            pm.compressorOn(), pm.starts(), pm.runMs(), pm.phaseMs(), pm.phaseLenMs(), pm.pauseLeftMs(), st.calA, st.calB, a);
  };
  for (int i = 0; i < 25; i++) { tick(10); pm.update(); logp("read"); }
  pm.start(); logp("press");
  guard = 0;
  while (pm.busy() && guard++ < 60000) { tick(10); pm.update(); logp(""); }
  for (int i = 0; i < 40; i++) { tick(10); pm.update(); logp("read"); }
  fclose(f);
  printf("cal a=%.4f b=%.4f fit=%.3f  pump final %.2f bar, starts %d, %.1f s, state %d\n",
         st.calA, st.calB, cal.fitErr(), Sensor::simP, pm.starts(), pm.runMs() / 1000.0, (int)pm.state());
}
