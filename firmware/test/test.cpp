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
}
