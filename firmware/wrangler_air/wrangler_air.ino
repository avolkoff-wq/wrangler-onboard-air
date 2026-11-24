// Wrangler Onboard Air - M5Stack DinMeter controller for the ARB Twin compressor
// Stage 1: K1 (G2) switches the compressor relay K2. Measuring pauses = compressor off.
// Deflate build (schematic C, pio env dinmeter-deflate): K4 (G1) opens the deflate valve; one target, the controller
// measures first and then pumps or vents by itself. Within +/-0.1 bar of the target it does nothing.
// Every build keeps a log on the flash: menu UPDATE -> Wi-Fi WranglerAir -> http://192.168.4.1/log
//
// Controls
//   turn            pick a typical pressure: HIGHWAY 2.5, ROUGH ROAD 1.8, GRAVEL 1.5, TRAIL 1.2, ROCK 1.0, SAND 0.8,
//                   RECOVERY 0.5 bar, CUSTOM x.x (config.h PRESETS); LOW shown below 0.8
//   press           (released within 0.5 s) two-press start: the first press arms (only when the screen says INFLATE
//                   or DEFLATE): "PRESS AGAIN: START" with a 3 s bar (INFLATE: AUX1 >= 13.2 V on G15 = engine runs, nothing extra;
//                   below it red "ENGINE OFF", the second press is the confirmation; no 0-25 V module = "ENGINE ON?" reminder); a second press within 3 s starts. A turn, a
//                   long press or 3 s without a press disarm (log ARM_CANCEL). While running one press stops at once.
//                   On CUSTOM the first press opens the value: turn 0.1 bar steps, press OK = save and arm,
//                   long press = cancel (one look at the value before every custom run)
//   long press      held 2.0 s (from 0.3 s a "HOLD FOR MENU" bar fills along the bottom; released 0.5..2.0 s = nothing):
//                   menu: CALIBRATE, LEARN, SENSOR (live volts for the bench), OUTPUT TEST, UNITS, TYRE, OFFSET, SETTLE, SCREEN,
//                   RESET LEARNING, UPDATE, VERSION, HOURS, EXIT. Not while running.
//   OUTPUT TEST: COMPRESSOR (K1) / VALVE (K4, valve builds) / BACK, press = one 1 s pulse, then off (never latched,
//          the other output forced off, not during a run); live sensor reading; log OUTPUT_TEST with p before / after.
//   HOURS: compressor hours, hours since the last filter service, valve hours (h:mm); hold 2 s, then hold 2 s again
//          on the confirmation = FILTERS CLEANED (log FILTER_RESET). After FILTER_SERVICE_H (10 h) idle shows CLEAN FILTERS.
//   LEARN: LEARN INFLATE (compressor 15 s) / LEARN DEFLATE (valve 20 s, builds with the valve, needs >= 1.5 bar) / BACK;
//          needs > 0.15 bar; confirm with a press (long: back). Measure, one fixed pulse, measure: INFLATE learns like
//          the first-run pulse, DEFLATE sets tauD = 20 / ln(p_before_abs / p_after_abs). Press aborts. Log LEARN_*_TEST.
//   SCREEN: press to turn the picture 90 degrees (WIDE, PORTRAIT, WIDE 180, PORTRAIT 180)
//   OFFSET / SETTLE: press to edit, turn: AUTO or a fixed value, press to save
// Screen (layout: docs/screen/screen_mockup.py, screen_mockup_idle_direction.py, scenery: sim2.py)
//   idle: the preset icon (SUV on its terrain, scenery behind: city, cone, firs, peaks, sun and cactus, beach),
//   the target in big digits, the preset name and, under a coloured rule, the live pressure ("actual x.x") and
//   what a start would do: INFLATE / DEFLATE (with "press to start"),
//   = OK, ABOVE (builds without the valve); CONNECT SYSTEM below 0.05 bar or on a sensor fault, the whole screen grey
//   and start blocked (a press only beeps) until the reading stays above 0.15 bar for 1 s
//   running: a small icon, the pressure (~ = estimate), a bar from the start pressure to the target
//   (orange = compressor, sky blue = venting, cyan = measuring) and the phase line;
//   the background turns dark amber while the compressor pumps and dark blue while the valve vents
//
// Libraries: M5Unified (Arduino Library Manager). Board: M5Stack StampS3 / "M5DinMeter".
// Partition scheme must have two OTA app partitions (default 8MB scheme does).
//
// UI: drawing helpers in ui_draw.h, screens in ui_screens.h (header-only).

#include "suv_bitmap.h"
#include <esp_task_wdt.h>
#include <esp_ota_ops.h>
#include <M5Unified.h>
#include "config.h"
#include "storage.h"
#include "sensor.h"
#include "pump.h"
#include "calibration.h"
#include "ota.h"
#include "log.h"
#include "trace.h"
#include "input.h"
#include "guards.h"
#include "sound.h"
#include "ui_state.h"
#include <algorithm>

Settings settings;
Sensor sensor;
Supply supply;
Pump pump;
Calibration cal;
Ota ota;
Trace trace;   // pressure trace of the current run, written to /runs at its end
Guards guards; // idle safety checks: VALVE CHECK, PUMP CHECK, battery guard (guards.h)
M5Canvas canvas(&M5.Display);

enum IdleDir { D_UP, D_DOWN, D_OK, D_ABOVE, D_DEAD };   // what a start would do from the idle screen
enum class Screen { MAIN, MENU, EDIT, CAL, OTA, VERSION, SENSOR, CUSTOM, LEARN_PICK, LEARN_CONF, LEARN_MSG, HOURS, HOURS_CONF, OUT_TEST, WIFI_PASS, BT };
int btPos = 0; uint32_t btForgetArm = 0;   // menu BLUETOOTH
Screen screen = Screen::MAIN;
int menuPos = 0;
enum MenuItem { M_CAL, M_LEARN, M_SENSOR, M_OUTPUT, M_DEMO, M_UNITS, M_TYRE, M_OFFSET, M_OFFMAX, M_SETTLE, M_VALVE, M_VSTUCK, M_PSTUCK, M_VCHECK, M_VTRIM, M_ECONF, M_WDOG, M_SCREEN, M_RESET, M_UPDATE, M_WIFIPASS, M_BT, M_VERSION, M_HOURS, M_EXIT };
// 0.7.2, owner: names that explain themselves (labels only, NVS keys unchanged); same order as MenuItem
const char* const MENU[] = {"CALIBRATE GAUGE", "LEARN TEST", "SENSOR LIVE", "OUTPUT TEST", "DEMO MODE", "UNITS", "TYRE",
  "FLOW OFFSET", "OFFSET LIMIT", "SETTLE TIME", "VALVE FITTED", "STUCK VALVE ALARM", "STUCK PUMP ALARM", "READ AUX1 VOLTS",
  "AUX1 VOLT TRIM", "ASK IF ENGINE OFF", "HANG RESTART", "SCREEN", "FORGET LEARNING", "WIFI UPDATE + LOG", "WIFI PASSWORD",
  "BLUETOOTH", "VERSION + VALUES", "COMPRESSOR HOURS", "EXIT"};
const int MENU_N = sizeof(MENU) / sizeof(MENU[0]);
// 0.7.2, owner: the menu in groups. Top level: TYRE (changed often), five groups, EXIT. In a group the last entry is BACK.
// Entry codes: >= 0 a MenuItem, -1 BACK, <= -2 group (-2 - index). Hold leaves the menu from any level.
struct MenuGroup { const char* name; const int8_t* items; int n; };
const int8_t G_SETUP[]   = {M_CAL, M_LEARN, M_UNITS, M_SCREEN, M_DEMO};
const int8_t G_SERVICE[] = {M_SENSOR, M_OUTPUT, M_HOURS, M_RESET, M_VERSION};
const int8_t G_CONNECT[] = {M_BT, M_UPDATE, M_WIFIPASS};
const int8_t G_ALARMS[]  = {M_VALVE, M_VSTUCK, M_PSTUCK, M_VCHECK, M_ECONF, M_WDOG, M_VTRIM};
const int8_t G_TUNING[]  = {M_OFFSET, M_OFFMAX, M_SETTLE};
#define MG(n, a) {n, a, (int)(sizeof(a) / sizeof(a[0]))}
const MenuGroup GROUPS[] = {MG("SETUP", G_SETUP), MG("SERVICE", G_SERVICE), MG("CONNECT", G_CONNECT), MG("SAFETY", G_ALARMS), MG("PUMP TUNING", G_TUNING)};
#undef MG
const int8_t MENU_TOP[] = {M_TYRE, -2, -3, -4, -5, -6, M_EXIT};
constexpr int MENU_TOP_N = sizeof(MENU_TOP);
int menuGroup = -1;   // -1 = top level
int menuLen() { return menuGroup < 0 ? MENU_TOP_N : GROUPS[menuGroup].n + 1; }
int menuEntry(int i) { return menuGroup < 0 ? MENU_TOP[i] : i < GROUPS[menuGroup].n ? GROUPS[menuGroup].items[i] : -1; }

// ---------- OFFSET / SETTLE editor: index 0 = AUTO ----------
const int OFF_N = 30;                     // 0.05 .. 1.50 bar
const int SET_N = 19;                     // 1.0 .. 10.0 s
float offVal(int i) { return i * 0.05f; }
float setVal(int i) { return 0.5f + i * 0.5f; }
const int OMX_N = 25;                     // OFFSET MAX: index 0 = AUTO, 1..25 = 0.30 .. 1.50 bar
float omxVal(int i) { return OFFMAX_LO - 0.05f + i * 0.05f; }
int editItem = M_OFFSET, editIdx = 0;
float customEdit = 2.0f;   // value being set on the CUSTOM screen
float trimEdit = 1.0f;     // AUX1 TRIM being set
int customStep = 0;        // CUSTOM screen: 0 = pressure, 1 = number of wheels
uint8_t customWheels = 4;
uint32_t resetArm = 0;   // RESET LEARNING waits for a second press
int offIdx() { return settings.offAuto ? 0 : constrain((int)roundf(settings.offManual / 0.05f), 1, OFF_N); }
int omxIdx() { return settings.omAuto ? 0 : constrain((int)roundf((settings.omMan - OFFMAX_LO) / 0.05f) + 1, 1, OMX_N); }
int setIdx() { return settings.settleAuto ? 0 : constrain((int)roundf((settings.settleManual - 0.5f) / 0.5f), 1, SET_N); }

// ARM_MS / HOLD_SHOW_MS live in ui_screens.h (UI_ARM_MS) so idle drawing sees them
// Sounds (sound.h, non-blocking): high = good (one long: target reached, two short: already within the band),
// low = look at the screen (three short: any error or OFF TARGET; a fast series ~2 s: OVER, PUMP_ON).
void beep(int n = 1) { sound::beep(n); }

// drawing / screens (need enums and globals above)
#include "ble.h"
#include "ui_draw.h"
#include "ui_screens.h"

// every screen ends here: in DEMO a magenta tag in the top right corner, so virtual tyres are never mistaken for real ones
void push() {
  if (g_demo || g_fakeV >= 0) {   // DEMO: virtual tyres; FAKE: a pressure typed over USB
    const char* tag = g_demo ? "DEMO" : "FAKE";
    canvas.setFont(&fonts::Font2); canvas.setTextDatum(top_right);
    const int w = canvas.textWidth(tag) + 6;
    dlFillRect(canvas.width() - w - 1, 0, w + 1, 17, TFT_MAGENTA);
    canvas.setTextColor(TFT_WHITE); dlString(tag, canvas.width() - 4, 1);
  }
  if (screen == Screen::MAIN && ble::connected()) {   // a phone is connected: small blue BT mark, top left
    dlFillRRect(0, 0, 17, 11, 2, TFT_BLUE);
    canvas.setFont(&fonts::Font0); canvas.setTextDatum(top_left); canvas.setTextColor(TFT_WHITE); dlString("BT", 3, 2);
  }
  dl::finish();   // the frame for the phone (dlist.h)
  canvas.pushSprite(0, 0);
}

// "no air" state (hoses not connected): entered below NOAIR_IN or on a sensor fault, left only after the reading
// stayed above NOAIR_OUT for NOAIR_HOLD_MS, so the screen does not flicker. Starting a run is blocked meanwhile.
constexpr float NOAIR_IN = 0.05f, NOAIR_OUT = 0.15f;
constexpr uint32_t NOAIR_HOLD_MS = 1000;
bool noAir = true;
void updateNoAir() {
  static uint32_t okSince = 0;
  const float now = sensor.bar(settings.calA, settings.calB);
  // raw bar too: an open signal wire reads 0 V = raw 0, while the calibrated value is lifted by calB
  if (sensor.fault() || now < NOAIR_IN || sensor.rawBar() < NOAIR_IN) { noAir = true; okSince = 0; }
  else if (noAir) {
    if (now <= NOAIR_OUT) okSince = 0;
    else if (!okSince) okSince = millis();
    else if (millis() - okSince >= NOAIR_HOLD_MS) noAir = false;
  }
}

// 0.6.2, owner: CUSTOM starts from an empty line too (a flat tyre, anything at 0 bar), so "no air" blocks only the
// fixed presets. A sensor fault still blocks. An open signal wire is still caught 3 s after the compressor starts
// (DEAD_AFTER_MS / DEAD_BAR, ERR SENSOR): a connected tyre lifts the line above 0.1 bar by the flow alone.
bool startBlocked() { return noAir && (settings.preset != P_CUSTOM || sensor.fault()); }
// what a start would do from the idle screen (IdleDir is declared at the top)
IdleDir idleDir(float bar) {
  const float now = sensor.bar(settings.calA, settings.calB);
  if (startBlocked()) return D_DEAD;                                   // hoses not connected / no air
  if (fabsf(now - bar) <= START_BAND) return D_OK;
  if (now < bar) return D_UP;
  return DEFLATE ? D_DOWN : D_ABOVE;                                   // inflate-only builds do nothing above the target
}
// engine state before the compressor starts (menu VOLT CHECK / ENGINE CONFIRM, AUX1 on G15)
Eng engine() {
  if (!settings.vcheck) return Eng::NOCHECK;
  if (!supply.fitted()) return Eng::NOMOD;
  return supply.engineOn() ? Eng::RUN : Eng::OFF;
}
const char* engText(Eng e) { return e == Eng::RUN ? "" : e == Eng::OFF ? "ENGINE OFF" : e == Eng::NOMOD ? "NO VOLTAGE" : "ENGINE ON?"; }
uint16_t engColor(Eng e) { return e == Eng::OFF ? TFT_RED : TFT_ORANGE; }
bool engConfirm() { return settings.econfirm && engine() != Eng::RUN; }   // a third press before the compressor
// two-press start: the first press arms, a second one within ARM_MS starts; knob, long press or timeout disarm.
// Inflating with the engine off or unknown (ENGINE CONFIRM): the second press only confirms, a third one starts.
bool armed = false; uint8_t armStage = 0; uint32_t armT = 0;
void arm() {   // only when a start would inflate or deflate
  const IdleDir d = idleDir(settings.target);
  if (d == D_UP || d == D_DOWN) { armed = true; armStage = 1; armT = millis(); } else beep();
}
void disarm(const char* why) { if (!armed) return; armed = false; armStage = 0; wlog::ev("ARM_CANCEL", wlog::kv("reason", String(why))); }

// ---------- menu LEARN ----------
#define LEARN_N (DEFLATE ? 3 : 2)
bool learnArm = false;   // LEARN INFLATE waiting for the engine confirmation
bool calEngArm = false;  // CALIBRATE: point 1 starts the compressor, same engine confirmation
int learnPos = 0; Mode learnSel = Mode::INF; String learnMsg;
Mode learnItem(int i) { if (i == 0) return Mode::INF; if (DEFLATE && i == 1) return Mode::DEF; return Mode::NONE; }

// ---------- run traces (trace.h) ----------
char phaseChar(PState s) {
  switch (s) {
    case PState::SETTLE: return 'S'; case PState::MEASURE: return 'E'; case PState::CAL_PULSE: return 'C';
    case PState::MAIN: return 'M'; case PState::TOPUP: return 'T'; case PState::VENT: return 'V'; default: return '-';
  }
}
// newest TRACE_KEEP files stay; names sort by boot and run number
void pruneRuns() {
  String names[64]; int n = 0;
  File d = LittleFS.open("/runs");
  if (!d) return;
  for (File f = d.openNextFile(); f && n < 64; f = d.openNextFile()) { names[n++] = String(f.name()); f.close(); }
  d.close();
  std::sort(names, names + n, [](const String& a, const String& b) { return a < b; });
  for (int i = 0; i + TRACE_KEEP < n; i++) LittleFS.remove("/runs/" + names[i].substring(names[i].lastIndexOf('/') + 1));
}
void traceStep() {
  static uint32_t runNo = 0; static PState prev = PState::IDLE; static float t0s = 0;
  const uint32_t now = millis();
  const PState st = pump.state();
  if (pump.busy() && !trace.active()) { trace.begin(now); t0s = now / 1000.0f; }
  if (trace.active()) {
    trace.sample(now, sensor.bar(settings.calA, settings.calB), pump.estimate(), phaseChar(st), pump.compressorOn(), pump.valveOpen());
    if (prev == PState::MEASURE && st != PState::MEASURE) trace.marker(now, pump.staticBar());
    if (!pump.busy()) {
      // errName() (pump.h) knows every PErr; the old local table stopped at LOW_P and read past its end on ERR LOW VOLTAGE
      const String res = st == PState::DONE ? (pump.inBand() ? "IN_BAND" : "DONE") : st == PState::ERROR ? String("ERR_") + errName(pump.error()) : "STOPPED";
      const Mode tm = pump.testMode();
      const String hdr = wlog::kv("preset", String(PRESETS[settings.preset].name)) + wlog::kv("target", settings.target, 2) +
                         wlog::kv("p0", pump.p0()) + wlog::kv("p", pump.staticBar()) + wlog::kv("result", res) +
                         wlog::kv("boot", String((int)settings.bootN)) + wlog::kv("t0", t0s, 1) + wlog::kv("tyre", String(TYRES[settings.tyre].name)) +
                         wlog::kv("starts", String(pump.starts())) + wlog::kv("vents", String(pump.vents())) +
                         wlog::kv("dur", trace.seconds(now), 1) + wlog::kv("test", String(tm == Mode::INF ? "INF" : tm == Mode::DEF ? "DEF" : "-"));
      const String body = trace.finish(hdr);
      if (wlog::ok) {
        LittleFS.mkdir("/runs");
        char name[32]; snprintf(name, sizeof name, "/runs/r%05lu_%03lu.csv", (unsigned long)settings.bootN, (unsigned long)++runNo);
        File f = LittleFS.open(name, "w");
        if (f) { f.print(body); f.close(); }
        pruneRuns();
      }
    }
  }
  prev = st;
}

// ---------- bench builds only (BENCH_USB): USB serial live stream and commands ----------
#if BENCH_USB
// LIVE,t,bar,est,phase,k1,valve twice per second while something runs (run, LEARN, OUTPUT TEST, calibration pumping)
void benchLive() {
  static uint32_t last = 0;
  const uint32_t now = millis();
  const bool on = pump.busy() || pump.pulsing() || cal.state() == CState::PUMPING;
  if (!on || now - last < 500) return;
  last = now;
  Serial.printf("LIVE,%.1f,%.3f,%.3f,%c,%d,%d\n", now / 1000.0f, sensor.bar(settings.calA, settings.calB), pump.estimate(),
                phaseChar(pump.state()), pump.compressorOn() ? 1 : 0, pump.valveOpen() ? 1 : 0);
}
void benchDump(const char* path) {
  File f = LittleFS.open(path, "r");
  if (!f) { Serial.printf("# %s: none\n", path); return; }
  Serial.printf("# ---- %s (%u bytes)\n", path, (unsigned)f.size());
  uint8_t buf[256];
  while (f.available()) { const size_t n = f.read(buf, sizeof buf); Serial.write(buf, n); }
  f.close();
  Serial.println("# ---- end");
}
void benchCommand(String c) {
  c.trim(); c.toLowerCase();
  if (!c.length()) return;
  if (c == "help") {
    Serial.println("# commands: status | log | runs | run <id> | clear | pulse k1 | pulse valve | k1 on|off | valve on|off | fake <bar>|off | io | ui | press | hold | left | right | help");
    Serial.println("# k1 / valve on: held up to 30 s, never both; fake: the sensor reads the typed pressure (FAKE tag), off at power-up");
  } else if (c == "status") {
    Serial.printf("# fw %s  preset %s  target %.2f bar  sensor %.3f bar (%.3f V)%s  state %d err %d\n", FW_VERSION,
                  PRESETS[settings.preset].name, settings.target, sensor.bar(settings.calA, settings.calB), sensor.vG1() * DIVIDER,
                  sensor.fault() ? " FAULT" : "", (int)pump.state(), (int)pump.error());
    Serial.printf("# ADC2 failed samples since the last RUN: %lu%s\n", (unsigned long)adc2::fails, sensor.adcFault() ? "  (sensor ADC fault)" : "");
    Serial.printf("# AUX1 %.2f V  %s  volt check %d  engine confirm %d  valve %d  demo %d\n", supply.volts(),
                  !supply.fitted() ? "no module" : supply.engineOn() ? "engine on" : "engine off", settings.vcheck, settings.econfirm, g_valve, g_demo);
    Serial.printf("# tyre %s  rate %.4f  kTop %.4f  offset %.3f  c %.2f  offcap %.2f%s  tauD %.1f  learned %d/%d  cal a %.4f b %.3f\n",
                  TYRES[settings.tyre].name, settings.rate, settings.kTop, settings.offset, settings.c, settings.offCap(), settings.omAuto ? " AUTO" : "", settings.tauD,
                  settings.learned, settings.learnedD, settings.calA, settings.calB);
    Serial.printf("# BT %s  bonds %d  connected %d  pairing %d  phone inputs %lu\n", ble::on ? "on" : "off", ble::bonds(), ble::connected(), ble::pairing() ? 1 : 0, (unsigned long)ble::inputs);
    Serial.printf("# compressor %.2f h  valve %.2f h  since filter service %.2f h  boot %lu\n", settings.cmpSec / 3600.0f,
                  settings.ventSec / 3600.0f, settings.svcHours(), (unsigned long)settings.bootN);
  } else if (c == "log") {
    benchDump("/log_old.csv"); benchDump("/log.csv");
  } else if (c == "runs") {
    File d = LittleFS.open("/runs");
    if (!d) { Serial.println("# no runs"); return; }
    for (File f = d.openNextFile(); f; f = d.openNextFile()) {
      String n = f.name(); n = n.substring(n.lastIndexOf('/') + 1);
      Serial.printf("# %s  %s\n", n.c_str(), f.readStringUntil('\n').c_str());
      f.close();
    }
    d.close();
  } else if (c.startsWith("run ")) {
    String id = c.substring(4); id.trim();
    if (id.indexOf('/') >= 0 || id.indexOf("..") >= 0) { Serial.println("# bad id"); return; }
    if (!id.endsWith(".csv")) id += ".csv";
    benchDump(("/runs/" + id).c_str());
  } else if (c == "clear") {
    wlog::clear(); wlog::ev("LOG", "cleared=1;"); Serial.println("# log cleared (runs kept)");
  } else if ((ota.running() || cal.active()) && !c.endsWith("off") && (c.startsWith("pulse") || c.startsWith("k1") || c.startsWith("valve"))) {
    // Wi-Fi is on: the sensor (ADC2) can not be read, so the pressure cut-offs of a pulse would be blind.
    // Calibration: K1 is the calibration's, an extra pulse would spoil the point (review item 5)
    Serial.println(ota.running() ? "# blocked: Wi-Fi UPDATE is on, outputs stay off" : "# blocked: calibration is running");
  } else if (c == "pulse k1" || c == "pulse valve") {
    const bool v = c.endsWith("valve");
    if (v && !DEFLATE) Serial.println("# no valve in this build");
    else if (!pump.pulse(v)) Serial.println("# blocked: a run or a pulse is active");
    else Serial.printf("# %s pulse %lu ms\n", v ? "VALVE" : "K1", (unsigned long)OUTPUT_PULSE_MS);
  } else if (c == "io") {   // one line for the Mac bench app: IO,t,bar,pinV,aux1V,k1,valve,fake,state,err,adcfail
    const UiSnapshot u = uiRead();
    Serial.printf("IO,%.1f,%.3f,%.3f,%.2f,%d,%d,%d,%d,%d,%lu\n", millis() / 1000.0f, u.bar, sensor.vG1(),
                  u.aux1, u.k1, u.valve, u.fake, u.state, u.err, (unsigned long)u.adcFails);   // last field: failed ADC2 samples since the last RUN
  } else if (c == "ui") {   // the screen as data (ui_state.h): what the BLE notify will send
    const UiSnapshot u = uiRead();
    Serial.printf("UI,%lu,%d,%d,%d,%d,%d,%d,%.3f,%.3f,%.2f,%.3f,%.2f,%d,%d,%d,%d,%d,%lu,%lu\n", (unsigned long)u.seq, u.screen, u.menuPos,
                  u.preset, u.state, u.err, u.armStage, u.bar, u.est, u.target, u.staticBar, u.aux1, u.k1, u.valve, u.noAir,
                  u.inBand, u.offTarget, (unsigned long)u.runMs, (unsigned long)u.phaseMs);
  } else if (c == "press" || c == "hold" || c == "left" || c == "right") {   // the phone path, typed: same queue as the knob
    input::send(c == "left" ? -1 : c == "right" ? 1 : 0, c == "press" ? input::Btn::SHORT : c == "hold" ? input::Btn::LONG : input::Btn::NONE, input::Src::USB);
  } else if (c == "k1 on" || c == "valve on") {
    const bool v = c.startsWith("valve");
    if (v && !DEFLATE) Serial.println("# no valve (menu VALVE off)");
    else { pump.pulseOff(); pump.update(); if (!pump.pulse(v, IO_HOLD_MS)) Serial.println("# blocked: a run is active");
           else Serial.printf("# %s ON, auto off in %lu s\n", v ? "VALVE" : "K1", (unsigned long)(IO_HOLD_MS / 1000)); }
  } else if (c == "k1 off" || c == "valve off") {
    pump.pulseOff(); Serial.println("# outputs off");
  } else if (c.startsWith("fake")) {
    String a = c.substring(4); a.trim();
    if (a == "off" || !a.length()) { g_fakeV = -1; sensor.resetFilter(); Serial.println("# fake off: real sensor"); }
    else {   // typed bar (calibrated) -> raw bar -> pin voltage, so every screen and check sees it as the sensor
      const float bar = a.toFloat(), raw = (bar - settings.calB) / settings.calA;
      g_fakeV = max(0.0f, (raw / SENSOR_FS * SENSOR_SPAN + SENSOR_V0) / DIVIDER); sensor.resetFilter();
      Serial.printf("# fake %.2f bar (pin %.3f V)\n", bar, g_fakeV);
      wlog::ev("FAKE", wlog::kv("bar", bar));
    }
  } else {
    Serial.println("# unknown command, type help");
  }
}
void benchLoop() {
  static String line;
  while (Serial.available()) {
    const char ch = Serial.read();
    if (ch == '\n' || ch == '\r') { benchCommand(line); line = ""; }
    else if (line.length() < 80) line += ch;
  }
  benchLive();
}
#endif

// ---------- hour meter ----------
// every second the compressor (run, LEARN, calibration) or the valve is on; saved when a run or calibration ends
void countHours() {
  static uint32_t last = millis(), cmpMs = 0, ventMs = 0;
  static bool wasActive = false;
  const uint32_t now = millis(), dt = now - last; last = now;
  if (pump.compressorOn() || cal.state() == CState::PUMPING) cmpMs += dt;
  if (pump.valveOpen()) ventMs += dt;
  settings.cmpSec += cmpMs / 1000; cmpMs %= 1000;
  settings.ventSec += ventMs / 1000; ventMs %= 1000;
  const bool active = pump.busy() || cal.active();
  if (wasActive && !active) settings.save();             // not every second: once per run
  wasActive = active;
}

bool filtersDue() { return settings.svcHours() >= FILTER_SERVICE_H; }

void applyRotation() {
  M5.Display.setRotation(settings.rot);
  canvas.deleteSprite();
  canvas.createSprite(M5.Display.width(), M5.Display.height());
}

// ---------- menu OUTPUT TEST / WIFI PASS ----------
#define OUT_N (DEFLATE ? 3 : 2)   // COMPRESSOR, (VALVE), BACK
int outPos = 0;
int wifiPassPos = 0;   // digit index 0..7 while editing Wi-Fi password
char wifiPassEdit[9] = "";

// ---------- OTA rollback ----------
// The Arduino core (CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=1) marks a new Wi-Fi image valid before setup() unless this
// returns true. We keep it pending and mark it valid only after APP_OK_MS of normal loop passes. A new image that
// crashes or hangs (watchdog) before that restarts in the pending state, and the bootloader goes back to the old one.
// Images flashed over USB are not pending: the call is then a no-op.
constexpr uint32_t APP_OK_MS = 30000;
extern "C" bool verifyRollbackLater() { return true; }   // overrides the weak C function in esp32-hal-misc.c
void markAppValid() {
  static bool done = false;
  if (done || millis() < APP_OK_MS) return;
  done = true;
  esp_ota_img_states_t st;
  if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
    esp_ota_mark_app_valid_cancel_rollback();
    wlog::ev("OTA", "app_valid=1;");
  }
}

// reset reason for the BOOT line (review item 7)
const char* resetName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "POWERON";   case ESP_RST_SW: return "SW";           case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";   case ESP_RST_TASK_WDT: return "TASK_WDT"; case ESP_RST_WDT: return "WDT";
    case ESP_RST_BROWNOUT: return "BROWNOUT"; case ESP_RST_DEEPSLEEP: return "DEEPSLEEP"; case ESP_RST_EXT: return "EXT";
    case ESP_RST_USB: return "USB";           default: return "OTHER";
  }
}

// the screen as data, once per pass (ui_state.h)
// share of the way from the start pressure to the target, 0..1, for the working sound (same as the run screen bar)
float workProgress() {
  const float cur = pump.estimating() ? pump.estimate() : sensor.bar(settings.calA, settings.calB);
  const float span = settings.target - pump.p0();
  if (fabsf(span) < 0.01f) return 1.0f;
  return constrain((cur - pump.p0()) / span, 0.0f, 1.0f);
}

void publishUi(int scr) {
  UiSnapshot u = uiRead();
  u.seq++; u.screen = (uint8_t)scr; u.menuPos = (uint8_t)menuPos; u.preset = settings.preset;
  u.state = (uint8_t)pump.state(); u.err = (uint8_t)pump.error(); u.armStage = armed ? armStage : 0;
  u.k1 = pump.compressorOn(); u.valve = pump.valveOpen(); u.noAir = noAir; u.inBand = pump.inBand(); u.offTarget = pump.offTarget();
  u.fake = g_fakeV >= 0; u.demo = g_demo;
  u.bar = sensor.bar(settings.calA, settings.calB); u.est = pump.estimate(); u.target = settings.target; u.staticBar = pump.staticBar(); u.p0 = pump.p0(); u.customEdit = customEdit; u.customStep = customStep; u.customWheels = customWheels;
  u.aux1 = supply.volts(); u.runMs = pump.runMs(); u.phaseMs = pump.phaseMs(); u.adcFails = adc2::fails;
  uiPublish(u);
  ble::update(u);   // notify the phone (ble.h), nothing when BT is off
}

// ---------- setup / loop ----------
void setup() {
  // outputs first: compressor and K4 must never twitch at boot
  pinMode(PIN_K1, OUTPUT); digitalWrite(PIN_K1, LOW);
  if (PIN_K4 >= 0) { pinMode(PIN_K4, OUTPUT); digitalWrite(PIN_K4, LOW); }
  pinMode(PIN_HOLD, OUTPUT); digitalWrite(PIN_HOLD, HIGH);

  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Speaker.setVolume(255);   // 0.6.2, owner: the buzzer at full volume
#if BENCH_USB
  Serial.begin(115200);
  Serial.println("# Wrangler Air " FW_VERSION " bench: type help");
#endif
  M5.Display.setBrightness(255);   // always full brightness (sunlight)
  settings.load();
  settings.bootN++; settings.save();
  wlog::begin(settings.bootN);
  const esp_reset_reason_t rr = esp_reset_reason();
  wlog::ev("BOOT", wlog::kv("fw", FW_VERSION) + wlog::kv("reset", String(resetName(rr))) + wlog::kv("tyre", String(TYRES[settings.tyre].name)) + wlog::kv("air_L", settings.airL(), 0) + wlog::kv("target", settings.target, 2) +
                   wlog::kv("offset", settings.offset) + wlog::kv("c", settings.c, 2) + wlog::kv("offcap", settings.offCap()) + wlog::kv("rate", settings.rate, 4) +
                   wlog::kv("kTop", settings.kTop, 4) + wlog::kv("bias", settings.bias) + wlog::kv("dev", settings.dev) +
                   wlog::kv("learned", settings.learned ? "1" : "0") + wlog::kv("tauD", settings.tauD, 1) +
                   wlog::kv("learnedD", settings.learnedD ? "1" : "0") + wlog::kv("calA", settings.calA, 4) + wlog::kv("calB", settings.calB) +
                   wlog::kv("cmp_h", settings.cmpSec / 3600.0f, 2) + wlog::kv("vent_h", settings.ventSec / 3600.0f, 2) +
                   wlog::kv("svc_h", settings.svcHours(), 2));
  applyRotation();     // WIDE / PORTRAIT, chosen in the menu
  sensor.begin();
  supply.begin();
  pump.begin(&sensor, &settings);
  cal.begin(&sensor, &settings);
  input::begin();
  input::queueBegin();
  ble::menuSource = menuBlob;      // the menu as a list for the phone
  if (settings.bt) ble::begin();   // menu BLUETOOTH, off by default
  supply.setTrim(settings.vtrim);
  splash();          // outputs are already off; sensor and pump wait

  // supply dip (engine crank, loose -X7 contact): the restart cut the outputs; logged so it is not mistaken for AUX1 off
  if (rr == ESP_RST_BROWNOUT) wlog::ev("BROWNOUT_RESET", "");
  // the last start was a watchdog reset or a crash: outputs were cut by the restart, say so
  if (rr == ESP_RST_TASK_WDT || rr == ESP_RST_INT_WDT || rr == ESP_RST_WDT || rr == ESP_RST_PANIC) {
    const bool wdt = rr != ESP_RST_PANIC;
    wlog::ev(wdt ? "WATCHDOG_RESET" : "CRASH_RESET", wlog::kv("reason", String((int)rr)));
    dlClear(TFT_BLACK);
    drawLines({ {wdt ? "WATCHDOG RESET" : "CRASH RESET", F_TEXT, TFT_ORANGE}, {"outputs were switched off", F_SMALL, TFT_WHITE},
                {"check the tyres", F_SMALL, TFT_WHITE} }, 0, H() - 18);
    push();
    for (int i = 0; i < 3; i++) { M5.Speaker.tone(1800, 120); delay(220); }
    delay(2500);
  }
  // watchdog on the main loop: no loop pass for WDT_MS = restart, and a restart drops both outputs
  esp_task_wdt_config_t wc = { .timeout_ms = WDT_MS, .idle_core_mask = 0, .trigger_panic = true };
  if (esp_task_wdt_reconfigure(&wc) != ESP_OK) esp_task_wdt_init(&wc);
  if (settings.wdog) esp_task_wdt_add(NULL);   // menu WATCHDOG
}

void loop() {
  {   // longest pass since the last RUN start (logged in END): the filters run on time, this shows how slow it got
    static uint32_t lastPass = 0; const uint32_t t = millis();
    if (lastPass && t - lastPass > g_loopMaxMs) g_loopMaxMs = t - lastPass;
    lastPass = t;
  }
  if (settings.wdog) esp_task_wdt_reset();
  sound::update();
  markAppValid();
  M5.update();
  // ADC2 (G13 sensor, G15 AUX1) can not be read while Wi-Fi is on: keep the last values instead of reading junk
  // (the slow AUX1 filter would otherwise drift to 0 and show NO VOLTAGE for a while after UPDATE)
  if (!ota.running()) { sensor.update(); supply.update(); }
  // idle for the stuck-output checks: nothing of ours drives K1 / K4. Calibration pumps K1 directly (calibration.h),
  // so it must count as busy, or PUMP CHECK sees its rise as "no signal, pressure rising" and stops it.
  {
    const bool calPump = cal.state() == CState::PUMPING;
    GuardIn gi;
    gi.now = millis();
    gi.idle = !pump.busy() && !pump.pulsing() && !cal.active() && !ota.running() && !g_demo;
    gi.fault = sensor.fault(); gi.p = sensor.bar(settings.calA, settings.calB);
    gi.vchkOn = settings.vchk2 && g_valve; gi.pchkOn = settings.pchk;
    gi.pumpBusy = pump.busy(); gi.calActive = cal.active();
    gi.compressorOn = pump.compressorOn() || calPump;
    gi.voltOk = settings.vcheck && supply.fitted(); gi.volts = supply.volts();
    switch (guards.update(gi)) {
      case GuardEv::VALVE_STUCK:
        wlog::ev("VALVE_STUCK", wlog::kv("p", gi.p) + wlog::kv("rate", guards.rate(), 4)); pump.valveStuck(gi.p); break;
      case GuardEv::PUMP_STUCK:
        wlog::ev("PUMP_STUCK", wlog::kv("p", gi.p) + wlog::kv("rise", guards.rise()) + wlog::kv("high", String(guards.high() ? 1 : 0)));
        pump.pumpStuck(gi.p); break;
      case GuardEv::LOW_V:   // run: ERR LOW VOLTAGE; IO TEST / USB "k1 on": output off; calibration: CAL FAILED
        wlog::ev("LOW_VOLTAGE", wlog::kv("aux1", gi.volts, 1) + wlog::kv("in", String(calPump ? "CAL" : pump.busy() ? "RUN" : "PULSE")));
        if (calPump) cal.fail("low_voltage"); else pump.lowVoltage();
        break;
      default: break;
    }
  }
  pump.update();
  if (!pump.busy()) settings.useWheels(4);   // a CUSTOM run on fewer wheels is over: learning back to the 4-wheel volume
  cal.update();
  countHours();
  traceStep();
#if BENCH_USB
  benchLoop();
#endif
  ota.update();

  {   // working rattle while a run pumps or vents (sound.h)
    static uint32_t lastTick = 0;
    if (pump.busy() && (pump.compressorOn() || pump.valveOpen()) && !sound::busy() && millis() - lastTick >= sound::workEveryMs(pump.valveOpen(), workProgress())) {
      lastTick = millis(); sound::work(pump.valveOpen());
    }
  }
  int steps; input::Btn b;           // b not const: MAIN swallows the press that stopped a run
  input::take(steps, b);              // knob and (later) phone through one queue (input.h)
  // the phone back key (0.7.2): in a menu group one level up, on the main screen nothing (the app handles it),
  // anywhere else the same as a hold (each screen's own way back)
  if (b == input::Btn::BACK) {
    if (screen == Screen::MENU && menuGroup >= 0) { menuPos = 1 + menuGroup; menuGroup = -1; b = input::Btn::NONE; }
    else if (screen == Screen::MAIN) b = input::Btn::NONE;
    else b = input::Btn::LONG;
  }
  // input feedback (0.6.2): tick on a turn, click on a press, a lower tone on a hold, a three-note call when the hold opens
  // the menu. Skipped while a result / error sound plays; a turn is silent during a run (it does nothing there).
  if (!sound::busy()) {
    if (b == input::Btn::SHORT) sound::click();
    else if (b == input::Btn::LONG) { if (screen == Screen::MAIN && !pump.busy()) sound::menuIn(); else sound::hold(); }
    else if (steps && !pump.busy()) sound::tick();
  }

  // a run result or an error is announced on every screen (review item 6): VALVE / PUMP CHECK or a failing run used to
  // stay silent while the menu (or SENSOR, HOURS ...) was open. Errors bring the main screen up, except in CAL and OTA.
  {
    static PState prev = PState::IDLE;
    const PState ps = pump.state();
    if (ps != prev) {
      if (ps == PState::ERROR) {
        sound::error(pump.error());
        if (screen != Screen::MAIN && screen != Screen::CAL && screen != Screen::OTA) { screen = Screen::MAIN; b = input::Btn::NONE; steps = 0; }
        disarm("ERROR");
      } else if (ps == PState::DONE) {
        if (pump.inBand()) sound::inBand(); else if (pump.offTarget()) sound::offTarget(); else sound::done();
      }
      prev = ps;
    }
  }

  switch (screen) {
    case Screen::MAIN: {
      updateNoAir();
      if (armed && steps) { disarm("KNOB"); steps = 0; }               // a turn only cancels the armed start
      if (armed && millis() - armT >= ARM_MS) disarm("TIMEOUT");
      if (steps && !pump.busy()) {
        // the knob steps through the typical pressures (config.h PRESETS), clockwise = higher; CUSTOM is last
        // 0.7.3, owner: the presets go round (CUSTOM <-> HIGHWAY), like the menu; one step at a time, skipping a preset
        // below the lowest target in the direction of travel
        const int dir = steps > 0 ? -1 : 1;
        int i = settings.preset;
        for (int k = 0; k < abs(steps); k++) {
          int tries = 0;
          do { i = ((i + dir) % N_PRESETS + N_PRESETS) % N_PRESETS; } while (settings.presetBar(i) < TARGET_MIN && ++tries < N_PRESETS);
        }
        settings.preset = i; settings.target = settings.presetBar(i);
        pump.clearResult();
      }
      static bool dirty = false; static uint32_t lastTurn = 0;
      if (steps) { dirty = true; lastTurn = millis(); }
      if (dirty && millis() - lastTurn > 2000) { settings.save(); dirty = false; }   // spare the flash

      // stop never needs two presses: while running any press stops, a short or medium one on release, a hold at 2 s
      // (the hold used to fire LONG, which is ignored during a run, and its release then gave nothing: the run went on)
      const bool stopNow = pump.busy() && (b == input::Btn::SHORT || b == input::Btn::MID || b == input::Btn::LONG);
      if (stopNow) { pump.stop(); b = input::Btn::NONE; }   // the same hold must not open the menu right after the stop
      else if (b == input::Btn::SHORT) {
        if (pump.state() == PState::DONE || pump.state() == PState::ERROR) pump.clearResult();
        else if (startBlocked()) { disarm("NO_AIR"); beep(); }                  // CONNECT SYSTEM: nothing to start
        else if (armed && armStage == 1 && idleDir(settings.target) == D_UP && engConfirm()) {   // engine off / unknown: confirm first
          armStage = 2; armT = millis(); wlog::ev("ENGINE_CONFIRM", wlog::kv("eng", String(engText(engine()))) + wlog::kv("aux1", supply.volts(), 1));
        }
        else if (armed) {   // second press within ARM_MS: go
          armed = false; armStage = 0;
          settings.useWheels(settings.preset == P_CUSTOM ? settings.wheels : 4);
          pump.start();
        }
        else if (settings.preset == P_CUSTOM) { customEdit = settings.custom; customWheels = settings.wheels; customStep = 0; screen = Screen::CUSTOM; }   // check the value first
        else arm();
      }
      if (b == input::Btn::LONG && !pump.busy()) { disarm("LONG"); screen = Screen::MENU; menuPos = 0; menuGroup = -1; }
      drawMain();
    } break;

    case Screen::MENU:
      if (steps) { const int n = menuLen(); menuPos = ((menuPos + steps) % n + n) % n; }   // round, at every level
      if (b == input::Btn::LONG) { screen = Screen::MAIN; menuGroup = -1; }          // hold leaves the menu from any level
      if (b == input::Btn::SHORT) {
        const int e = menuEntry(menuPos);
        if (e == -1) { menuPos = 1 + menuGroup; menuGroup = -1; }                     // BACK: up, on the group just left
        else if (e <= -2) { menuGroup = -2 - e; menuPos = 0; }                      // into a group
        else switch (e) {
          case M_CAL:     pump.stop(); cal.start(); calEngArm = false; screen = Screen::CAL; break;   // no IO TEST pulse left on (review item 5)
          case M_LEARN:   learnPos = 0; screen = Screen::LEARN_PICK; break;
          case M_SENSOR:  screen = Screen::SENSOR; break;
          case M_OUTPUT:  outPos = 0; screen = Screen::OUT_TEST; break;
          case M_UNITS:   settings.psi = !settings.psi; settings.save(); break;
          case M_TYRE: {  // press: next size; learned values are scaled by the air volume
            const uint8_t old = settings.tyre;
            settings.setTyre((settings.tyre + 1) % N_TYRE_SIZES);
            wlog::ev("TYRE", wlog::kv("from", String(TYRES[old].name)) + wlog::kv("to", String(TYRES[settings.tyre].name)) +
                             wlog::kv("air_L", settings.airL(), 0) + wlog::kv("rate", settings.rate, 4) + wlog::kv("tauD", settings.tauD, 1));
          } break;
          case M_OFFSET:  editItem = M_OFFSET; editIdx = offIdx(); screen = Screen::EDIT; break;
          case M_OFFMAX:  editItem = M_OFFMAX; editIdx = omxIdx(); screen = Screen::EDIT; break;
          case M_SETTLE:  editItem = M_SETTLE; editIdx = setIdx(); screen = Screen::EDIT; break;
          case M_DEMO:    // virtual tyres for the desk: never saved, off at every power-up
            g_demo = !g_demo; Sensor::simReset(); sensor.resetFilter(); wlog::ev("DEMO", wlog::kv("on", String(g_demo ? 1 : 0))); break;
          case M_VALVE:
            pump.stop();   // outputs off BEFORE g_valve changes (pump valve() closes K4 either way since 0.5.2)
            settings.valve = g_valve = !settings.valve;
            // with the valve the lowest target is 0.5 bar: the CUSTOM value too, not only the active target
            if (settings.target < TARGET_MIN) settings.target = TARGET_MIN;
            if (settings.custom < TARGET_MIN) settings.custom = TARGET_MIN;
            settings.save(); wlog::ev("VALVE", wlog::kv("on", String(g_valve ? 1 : 0))); break;
          case M_VCHECK:  settings.vcheck = !settings.vcheck; settings.save(); break;
          case M_VTRIM:   // set the shown AUX1 volts to the multimeter reading (needs the module and a voltage)
            if (supply.rawVolts() < VBAT_MIN_V) beep(3);
            else { trimEdit = settings.vtrim; editItem = M_VTRIM; screen = Screen::EDIT; }
            break;
          case M_ECONF:   settings.econfirm = !settings.econfirm; settings.save(); break;
          case M_PSTUCK:  settings.pchk = !settings.pchk; settings.save(); break;
          case M_VSTUCK:  settings.vchk2 = !settings.vchk2; settings.save(); break;
          case M_WDOG:
            settings.wdog = !settings.wdog; settings.save();
            if (settings.wdog) esp_task_wdt_add(NULL); else esp_task_wdt_delete(NULL);
            wlog::ev("WATCHDOG", wlog::kv("on", String(settings.wdog ? 1 : 0))); break;
          case M_SCREEN:  settings.rot = (settings.rot + 1) % 4; settings.save(); applyRotation(); break;
          case M_RESET:   // two presses within 3 s, so a stray click cannot wipe what was learned
            if (resetArm && millis() - resetArm < 3000) { settings.resetLearning(); wlog::ev("RESET_LEARNING", ""); beep(); resetArm = 0; }
            else resetArm = millis();
            break;
          case M_UPDATE:  pump.stop(); ble::end(); ota.start(&settings.psi, settings.wifiPass); screen = Screen::OTA; break;   // Wi-Fi and BLE never together
          case M_BT:      btPos = 0; btForgetArm = 0; screen = Screen::BT; break;
          case M_WIFIPASS:
            strncpy(wifiPassEdit, settings.wifiPass, sizeof wifiPassEdit);
            wifiPassEdit[sizeof wifiPassEdit - 1] = 0;
            if (!wifiPassEdit[0]) strncpy(wifiPassEdit, OTA_PASS, sizeof wifiPassEdit);
            wifiPassPos = 0; screen = Screen::WIFI_PASS; break;
          case M_VERSION: screen = Screen::VERSION; break;
          case M_HOURS:   screen = Screen::HOURS; break;
          default: screen = Screen::MAIN; break;
        }
      }
      if (screen == Screen::MENU) drawMenu();
      break;

    case Screen::EDIT:
      if (editItem == M_VTRIM) {   // turn: shown volts +/-0.05 V, press: save, hold: back
        const float raw = supply.rawVolts();
        if (steps && raw > 1) trimEdit = constrain((raw * trimEdit + steps * 0.05f) / raw, VTRIM_MIN, VTRIM_MAX);
        if (b == input::Btn::SHORT) {
          wlog::ev("AUX1_TRIM", wlog::mv("trim", settings.vtrim, trimEdit, 4) + wlog::kv("aux1", raw * trimEdit, 2));
          settings.vtrim = trimEdit; settings.save(); supply.setTrim(trimEdit); beep(); screen = Screen::MENU;
        }
        if (b == input::Btn::LONG) screen = Screen::MENU;
        if (screen == Screen::EDIT) drawTrim(raw * trimEdit, trimEdit);
        break;
      }
      if (steps) editIdx = constrain(editIdx + steps, 0, editItem == M_OFFSET ? OFF_N : editItem == M_OFFMAX ? OMX_N : SET_N);
      if (b == input::Btn::SHORT) {
        if (editItem == M_OFFSET) { settings.offAuto = editIdx == 0; if (editIdx) settings.offManual = offVal(editIdx); }
        else if (editItem == M_OFFMAX) {
          settings.omAuto = editIdx == 0; if (editIdx) settings.omMan = omxVal(editIdx);
          wlog::ev("OFFSET_MAX", wlog::kv("auto", String(settings.omAuto ? 1 : 0)) + wlog::kv("cap", settings.offCap()));
        }
        else                      { settings.settleAuto = editIdx == 0; if (editIdx) settings.settleManual = setVal(editIdx); }
        settings.save(); beep(); screen = Screen::MENU;
      }
      if (b == input::Btn::LONG) screen = Screen::MENU;   // leave without saving
      if (screen == Screen::EDIT) drawEdit();
      break;

    case Screen::CUSTOM:   // CUSTOM preset: turn 0.1 bar, press = save and arm, long = cancel
      updateNoAir();
      if (customStep == 0) {   // pressure, 0.1 steps
        if (steps) customEdit = roundf(constrain(customEdit + steps * TARGET_STEP, TARGET_MIN, TARGET_MAX) * 10) / 10.0f;
        if (b == input::Btn::SHORT) customStep = 1;
        if (b == input::Btn::LONG) screen = Screen::MAIN;
      } else {                 // number of wheels on the line: the air volume and the learning scale with it
        if (steps) customWheels = constrain((int)customWheels + steps, 1, 4);
        if (b == input::Btn::SHORT) {
          customEdit = constrain(customEdit, (float)TARGET_MIN, TARGET_MAX);   // pressed without turning: still within limits
          settings.custom = settings.target = customEdit; settings.wheels = customWheels; settings.save();
          wlog::ev("CUSTOM_SET", wlog::kv("custom", settings.custom, 2) + wlog::kv("wheels", String(settings.wheels)));
          screen = Screen::MAIN;
          arm();                                                       // then press again to start
        }
        if (b == input::Btn::LONG) customStep = 0;
      }
      if (screen == Screen::CUSTOM) drawCustom();
      break;

    case Screen::LEARN_PICK:   // LEARN INFLATE / LEARN DEFLATE (valve builds) / BACK
      if (steps) learnPos = constrain(learnPos + steps, 0, LEARN_N - 1);
      if (b == input::Btn::LONG) screen = Screen::MENU;
      if (b == input::Btn::SHORT) {
        const Mode m = learnItem(learnPos);
        const float now = sensor.bar(settings.calA, settings.calB);
        if (m == Mode::NONE) screen = Screen::MENU;
        else if (sensor.fault() || now <= LEARN_MIN_BAR) { learnMsg = "CONNECT SYSTEM"; screen = Screen::LEARN_MSG; }
        else if (m == Mode::DEF && now < LEARN_DEF_MIN_BAR) { learnMsg = String("PUMP UP TO ") + fmt(LEARN_DEF_MIN_BAR) + " FIRST"; screen = Screen::LEARN_MSG; }
        else { learnSel = m; learnArm = false; screen = Screen::LEARN_CONF; }
      }
      if (screen == Screen::LEARN_PICK) drawLearnPick();
      break;

    case Screen::LEARN_CONF:
      if (b == input::Btn::LONG) screen = Screen::LEARN_PICK;
      if (b == input::Btn::SHORT) {
        if (learnSel == Mode::INF && engConfirm() && !learnArm) learnArm = true;    // engine off / unknown: one more press
        else { learnArm = false; pump.learnTest(learnSel); screen = Screen::MAIN; }
      }
      if (b == input::Btn::LONG) learnArm = false;
      if (screen == Screen::LEARN_CONF) drawLearnConf();
      break;

    case Screen::LEARN_MSG:
      if (b == input::Btn::SHORT || b == input::Btn::LONG) screen = Screen::LEARN_PICK;
      else { dlClear(TFT_BLACK); drawLines({ {learnMsg, F_TEXT, TFT_ORANGE} }, 0, H() - 18); drawHint("press: back"); push(); }
      break;

    case Screen::CAL:
      if (steps) cal.turn(steps);
      // while the compressor pumps a medium press (0.5-2 s) aborts too, like a stop during a run; elsewhere only SHORT confirms
      if ((b == input::Btn::SHORT || b == input::Btn::MID) && (cal.state() == CState::PUMPING || cal.state() == CState::SETTLING)) cal.abort();
      else if (b == input::Btn::SHORT) {
        if (cal.state() == CState::ZERO_WAIT && engConfirm() && !calEngArm) {   // engine off / unknown: one more press
          calEngArm = true; wlog::ev("ENGINE_CONFIRM", wlog::kv("in", String("CAL")) + wlog::kv("aux1", supply.volts(), 1));
        } else { calEngArm = false; cal.press(); }
      }
      if (b == input::Btn::LONG) cal.abort();
      if (!cal.active()) screen = Screen::MAIN; else drawCal();
      break;

    case Screen::OTA:
      if (b == input::Btn::LONG) { ota.stop(); screen = Screen::MAIN; }
      else if (!ota.running()) screen = Screen::MAIN;                  // Wi-Fi switched itself off after 10 min without requests
      else drawOta();
      if (screen != Screen::OTA && settings.bt) ble::begin();          // BLE back after the Wi-Fi session
      break;

    case Screen::BT:   // menu BLUETOOTH: [x] BT ON / PAIR / FORGET ALL / BACK
      if (steps) btPos = ((btPos + steps) % 4 + 4) % 4;
      if (b == input::Btn::LONG) { ble::pairStop(); screen = Screen::MENU; }
      if (b == input::Btn::SHORT) {
        if (ble::pairing()) ble::pairStop();                           // any press closes the PIN window
        else switch (btPos) {
          case 0: settings.bt = !settings.bt; settings.save(); if (settings.bt) ble::begin(); else ble::end(); break;
          case 1: if (!ble::pairStart()) beep(3); break;               // BT off or 3 phones bonded already
          case 2:                                                      // two presses within 3 s, like RESET LEARNING
            if (!ble::on) beep(3);
            else if (btForgetArm && millis() - btForgetArm < 3000) { ble::forgetAll(); btForgetArm = 0; beep(); }
            else btForgetArm = millis();
            break;
          default: screen = Screen::MENU; break;
        }
      }
      if (screen == Screen::BT) drawBt();
      break;

    case Screen::VERSION:
      if (b == input::Btn::SHORT || b == input::Btn::LONG) screen = Screen::MENU;
      else drawVersion();
      break;

    case Screen::OUT_TEST:   // IO TEST: COMPRESSOR (K1) / VALVE (K4) / BACK: press = on (up to 30 s) / off, never both
      if (steps) outPos = constrain(outPos + steps, 0, OUT_N - 1);
      if (b == input::Btn::LONG) { pump.stop(); screen = Screen::MENU; }
      if (b == input::Btn::MID && pump.pulsing()) pump.stop();          // a medium press switches an output off, never on
      if (b == input::Btn::SHORT) {
        if (outPos == OUT_N - 1) { pump.stop(); screen = Screen::MENU; }
        else {
          const bool v = outPos == 1, wasThis = pump.pulsing() && pump.pulseIsValve() == v;
          pump.pulseOff(); pump.update();                              // whatever was on goes off first
          if (!wasThis && !pump.pulse(v, IO_HOLD_MS)) beep();          // a run is active
        }
      }
      if (screen == Screen::OUT_TEST) drawOutTest();
      break;

    case Screen::HOURS:
      if (b == input::Btn::SHORT) screen = Screen::MENU;
      if (b == input::Btn::LONG) screen = Screen::HOURS_CONF;
      if (screen == Screen::HOURS) drawHours(false);
      break;

    case Screen::HOURS_CONF:   // hold 2 s again: the filter service counter starts from zero
      if (b == input::Btn::SHORT) screen = Screen::HOURS;
      if (b == input::Btn::LONG) {
        wlog::ev("FILTER_RESET", wlog::kv("svc_h", settings.svcHours(), 2) + wlog::kv("cmp_h", settings.cmpSec / 3600.0f, 2));
        settings.svcSec = settings.cmpSec; settings.save(); beep(); screen = Screen::HOURS;
      }
      if (screen == Screen::HOURS_CONF) drawHours(true);
      break;


    case Screen::WIFI_PASS: {   // edit AP password (8 chars, A-Z 0-9), stored in NVS, used by ota.h
      const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
      const int AL = sizeof(alphabet) - 1;
      if (steps) {
        char& ch = wifiPassEdit[wifiPassPos];
        int idx = 0; while (idx < AL && alphabet[idx] != ch) idx++;
        if (idx >= AL) idx = 0;
        idx = (idx + steps) % AL; if (idx < 0) idx += AL;
        ch = alphabet[idx];
      }
      if (b == input::Btn::SHORT) {
        if (wifiPassPos < 7) wifiPassPos++;
        else {
          wifiPassEdit[8] = 0;
          strncpy(settings.wifiPass, wifiPassEdit, sizeof settings.wifiPass);
          settings.wifiPass[sizeof settings.wifiPass - 1] = 0;
          settings.save();
          wlog::ev("WIFI_PASS", "changed=1;");
          beep(); screen = Screen::MENU;
        }
      }
      if (b == input::Btn::LONG) screen = Screen::MENU;
      if (screen == Screen::WIFI_PASS) {
        dlClear(TFT_BLACK);
        char posBuf[16];
        snprintf(posBuf, sizeof posBuf, "char %d/8", wifiPassPos + 1);
        drawLines({ {"WIFI PASSWORD", F_TEXT, TFT_CYAN},
                    {String(wifiPassEdit), F_MID, TFT_GREENYELLOW},
                    {posBuf, F_SMALL, TFT_LIGHTGREY} }, 0, H() - 18);
        canvas.setFont(F_MID);
        const int tw = canvas.textWidth(wifiPassEdit);
        const int cw = max(1, (int)canvas.textWidth("A"));
        const int x0 = (W() - tw) / 2;
        dlFillRect(x0 + wifiPassPos * cw, 72, cw, 3, TFT_CYAN);
        drawHint("press: next  hold: back  turn: char");
        push();
      }
    } break;

    case Screen::SENSOR:
      if (b == input::Btn::SHORT || b == input::Btn::LONG) screen = Screen::MENU;
      else drawSensor();
      break;
  }
  publishUi((int)screen);
  delay(20);
}
