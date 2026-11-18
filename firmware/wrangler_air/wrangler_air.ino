// Wrangler Onboard Air - M5Stack DinMeter controller for the ARB Twin compressor
// Stage 1: K1 (G2) switches the compressor relay K2. Measuring pauses = compressor off.
// Deflate build (schematic C, pio env dinmeter-deflate): K4 (G1) opens the deflate valve; one target, the controller
// measures first and then pumps or vents by itself. Within +/-0.1 bar of the target it does nothing.
// Every build keeps a log on the flash: menu UPDATE -> Wi-Fi WranglerAir -> http://192.168.4.1/log
//
// Controls
//   turn            pick a typical pressure: HIGHWAY 2.5, ROUGH ROAD 1.8, GRAVEL 1.5, TRAIL 1.2, ROCK 1.0, SAND 0.8,
//                   RECOVERY 0.5 bar, CUSTOM x.x (config.h PRESETS); LOW shown below 0.8
//   press           start / stop; on CUSTOM it first opens the value: turn 0.1 bar steps, press OK = save and start,
//                   long press = cancel (one look at the value before every custom run)
//   long press      menu: CALIBRATE, SENSOR (live volts for the bench), UNITS, TYRE, OFFSET, SETTLE, SCREEN, RESET LEARNING, UPDATE, VERSION, EXIT
//   SCREEN: press to turn the picture 90 degrees (WIDE, PORTRAIT, WIDE 180, PORTRAIT 180)
//   OFFSET / SETTLE: press to edit, turn: AUTO or a fixed value, press to save
//
// Libraries: M5Unified (Arduino Library Manager). Board: M5Stack StampS3 / "M5DinMeter".
// Partition scheme must have two OTA app partitions (default 8MB scheme does).

#include <M5Unified.h>
#include "config.h"
#include "storage.h"
#include "sensor.h"
#include "pump.h"
#include "calibration.h"
#include "ota.h"
#include "log.h"
#include "input.h"

Settings settings;
Sensor sensor;
Pump pump;
Calibration cal;
Ota ota;
M5Canvas canvas(&M5.Display);

enum class Screen { MAIN, MENU, EDIT, CAL, OTA, VERSION, SENSOR, CUSTOM };
Screen screen = Screen::MAIN;
int menuPos = 0;
enum MenuItem { M_CAL, M_SENSOR, M_UNITS, M_TYRE, M_OFFSET, M_SETTLE, M_SCREEN, M_RESET, M_UPDATE, M_VERSION, M_EXIT };
const char* MENU[] = {"CALIBRATE", "SENSOR", "UNITS", "TYRE", "OFFSET", "SETTLE", "SCREEN", "RESET LEARNING", "WIFI: UPDATE + LOG", "VERSION", "EXIT"};
constexpr int MENU_N = sizeof(MENU) / sizeof(MENU[0]);

// ---------- helpers ----------
float toUnit(float bar) { return settings.psi ? bar * 14.5038f : bar; }
const char* unitName() { return settings.psi ? "psi" : "bar"; }
String fmt(float bar) { return String(toUnit(bar), settings.psi ? 0 : 1); }
// m:ss.t (tenths of a second)
String mmss(uint32_t ms) {
  const uint32_t t = ms / 100, s = t / 10; char b[12];
  snprintf(b, sizeof b, "%lu:%02lu.%lu", (unsigned long)(s / 60), (unsigned long)(s % 60), (unsigned long)(t % 10));
  return b;
}
// countdown when the length is known, elapsed otherwise
String phaseTimer(uint32_t elapsed, uint32_t len) {
  if (len) { const uint32_t left = elapsed >= len ? 0 : len - elapsed; return String(left / 1000.0f, 1) + "s"; }
  return mmss(elapsed);
}

// ---------- OFFSET / SETTLE editor: index 0 = AUTO ----------
constexpr int OFF_N = 30;                     // 0.05 .. 1.50 bar
constexpr int SET_N = 19;                     // 1.0 .. 10.0 s
float offVal(int i) { return i * 0.05f; }
float setVal(int i) { return 0.5f + i * 0.5f; }
int editItem = M_OFFSET, editIdx = 0;
float customEdit = 2.0f;   // value being set on the CUSTOM screen
uint32_t resetArm = 0;   // RESET LEARNING waits for a second press
int offIdx() { return settings.offAuto ? 0 : constrain((int)roundf(settings.offManual / 0.05f), 1, OFF_N); }
int setIdx() { return settings.settleAuto ? 0 : constrain((int)roundf((settings.settleManual - 0.5f) / 0.5f), 1, SET_N); }
String offText(int i) { return i == 0 ? "AUTO" : String(offVal(i), 2) + " bar"; }
String setText(int i) { return i == 0 ? "AUTO" : String(setVal(i), 1) + " s"; }

void beep(int n = 1) {
  for (int i = 0; i < n; i++) { M5.Speaker.tone(3500, 120); delay(180); }
}

// ---------- drawing ----------
// Every screen is a column of centred lines laid out for the current canvas size,
// so the same code works in landscape (240x135) and portrait (135x240).
using Font = const lgfx::IFont*;
const Font F_SMALL = &fonts::FreeSans9pt7b;
const Font F_TEXT  = &fonts::FreeSansBold9pt7b;
const Font F_MID   = &fonts::FreeSansBold12pt7b;
const Font F_LARGE = &fonts::FreeSansBold18pt7b;
const Font F_DIGIT = &fonts::Font7;           // 7-segment digits, 48 px high

struct Ln { String t; Font f; uint16_t c; Font alt = nullptr; };   // alt: smaller font if t does not fit
int W() { return canvas.width(); }
int H() { return canvas.height(); }
bool portrait() { return H() > W(); }

// split into lines no wider than maxW (words are kept whole)
int wrapText(const String& t, int maxW, String* out, int maxOut) {
  int n = 0; String cur;
  int i = 0;
  while (i <= (int)t.length() && n < maxOut) {
    int j = t.indexOf(' ', i); if (j < 0) j = t.length();
    String w = t.substring(i, j);
    String cand = cur.length() ? cur + " " + w : w;
    if (cur.length() && canvas.textWidth(cand) > maxW) { out[n++] = cur; cur = w; }
    else cur = cand;
    i = j + 1;
  }
  if (cur.length() && n < maxOut) out[n++] = cur;
  return n;
}

// draws the lines spread evenly between top and bottom
void drawLines(std::initializer_list<Ln> ls, int top = 0, int bottom = -1) {
  if (bottom < 0) bottom = H();
  const int maxW = W() - 8;
  struct Row { String t; Font f; uint16_t c; int h; } rows[16]; int n = 0;
  for (const Ln& l : ls) {
    Font f = l.f; canvas.setFont(f);
    if (l.alt && canvas.textWidth(l.t) > maxW) { f = l.alt; canvas.setFont(f); }
    String parts[4]; const int k = (f == F_DIGIT) ? (parts[0] = l.t, 1) : wrapText(l.t, maxW, parts, 4);
    for (int q = 0; q < k && n < 16; q++) rows[n++] = { parts[q], f, l.c, canvas.fontHeight() };
  }
  int total = 0; for (int q = 0; q < n; q++) total += rows[q].h;
  const float gap = max(2.0f, (float)(bottom - top - total) / (n + 1));
  float y = top + gap;
  canvas.setTextDatum(top_center);
  for (int q = 0; q < n; q++) {
    canvas.setFont(rows[q].f); canvas.setTextColor(rows[q].c);
    canvas.drawString(rows[q].t, W() / 2, (int)y);
    y += rows[q].h + gap;
  }
}

// small grey line at the very top: total time (and starts)
int drawTopBar(const String& t) {
  canvas.setFont(F_SMALL); canvas.setTextColor(TFT_LIGHTGREY); canvas.setTextDatum(top_left);
  String parts[2]; const int k = wrapText(t, W() - 22, parts, 2);
  for (int q = 0; q < k; q++) canvas.drawString(parts[q], 4, 2 + q * canvas.fontHeight());
  return 4 + k * canvas.fontHeight();
}

void drawMain() {
  canvas.fillSprite(TFT_BLACK);
  const PState st = pump.state();
  const bool est = pump.estimating();
  const float shown = (st == PState::IDLE) ? sensor.bar(settings.calA, settings.calB)
                                           : (est ? pump.estimate() : pump.staticBar());
  String bar = mmss(pump.runMs()) + "  starts " + String(pump.starts());
  if (DEFLATE) bar = (pump.mode() == Mode::DEF ? "DEF " : pump.mode() == Mode::INF ? "INF " : "") + bar + "  vents " + String(pump.vents());
  const int top = (st != PState::IDLE) ? drawTopBar(bar) : 0;

  const String tm = phaseTimer(pump.phaseMs(), pump.phaseLenMs());
  String line; uint16_t col = TFT_WHITE;
  switch (st) {
    case PState::IDLE:
      line = String(PRESETS[settings.preset].name) + " " + fmt(settings.target) + (settings.target < TARGET_LOW_WARN ? "  LOW" : "");
      if (settings.target < TARGET_LOW_WARN) col = TFT_ORANGE;
      break;
    case PState::SETTLE:    line = "SETTLING -> " + fmt(settings.target); col = TFT_CYAN; break;
    case PState::MEASURE:   line = "MEASURING -> " + fmt(settings.target); col = TFT_CYAN; break;
    case PState::CAL_PULSE: line = "LEARNING " + tm; col = TFT_ORANGE; break;
    case PState::MAIN:      line = "PUMP -> " + fmt(settings.target) + "  " + tm; col = TFT_ORANGE; break;
    case PState::TOPUP:     line = "TOP-UP " + tm; col = TFT_ORANGE; break;
    case PState::VENT:      line = "VENT -> " + fmt(settings.target) + "  " + tm; col = TFT_SKYBLUE; break;
    case PState::DONE:
      line = pump.inBand() ? "OK, WITHIN 0.1" : "DONE " + fmt(pump.staticBar()) + "  " + mmss(pump.runMs()); col = TFT_GREEN; break;
    case PState::ERROR:
      col = TFT_RED;
      switch (pump.error()) {
        case PErr::SENSOR:  line = "ERR SENSOR"; break;
        case PErr::OVER:    line = "ERR OVER 3.0 BAR"; break;
        case PErr::LEAK:    line = "ERR NO RISE: LEAK?"; break;
        case PErr::TIMEOUT: line = "ERR TIMEOUT"; break;
        default:            line = "ERR"; break;
      }
      break;
  }
  if (pump.paused() && !sensor.fault())      // pause: big countdown to the end of the measurement
    drawLines({ {String(pump.pauseLeftMs() / 1000.0f, 1), F_DIGIT, TFT_CYAN}, {"s", F_TEXT, TFT_LIGHTGREY}, {line, F_TEXT, col} }, top);
  else
    drawLines({ {sensor.fault() ? "--.-" : fmt(max(0.0f, shown)), F_DIGIT, sensor.fault() ? TFT_RED : TFT_GREENYELLOW},
                {String(est ? "~ " : "") + unitName(), F_TEXT, TFT_LIGHTGREY}, {line, F_TEXT, col} }, top);
  if (pump.compressorOn()) canvas.fillCircle(W() - 10, 10, 6, TFT_ORANGE);
  if (pump.valveOpen())    canvas.fillCircle(W() - 10, 10, 6, TFT_SKYBLUE);
  canvas.pushSprite(0, 0);
}

String rotName() {
  switch (settings.rot) { case 0: return "PORTRAIT"; case 2: return "PORTRAIT 180"; case 3: return "WIDE 180"; default: return "WIDE"; }
}

void drawMenu() {
  canvas.fillSprite(TFT_BLACK);
  const Font f = portrait() ? F_SMALL : F_TEXT;
  canvas.setFont(f);
  canvas.setTextDatum(middle_left);
  const int rowH = 26, vis = max(1, (H() - 4) / rowH);
  const int first = constrain(menuPos - vis / 2, 0, max(0, MENU_N - vis));
  for (int k = 0; k < vis && first + k < MENU_N; k++) {
    const int i = first + k;
    const int y = 2 + rowH / 2 + k * rowH;
    if (i == menuPos) canvas.fillRoundRect(3, y - 11, W() - 6, 22, 4, TFT_DARKGREEN);
    canvas.setTextColor(TFT_WHITE);
    String t = MENU[i];
    if (i == M_UNITS)  t += String(": ") + unitName();
    if (i == M_TYRE)   t += String(": ") + TYRES[settings.tyre].name;
    if (i == M_OFFSET) t += ": " + offText(offIdx());
    if (i == M_SETTLE) t += ": " + setText(setIdx());
    if (i == M_SCREEN) t += ": " + rotName();
    if (i == M_RESET && resetArm && millis() - resetArm < 3000) t = "PRESS AGAIN";
    else if (i == M_RESET && portrait()) t = "RESET LEARN";
    canvas.drawString(t, 8, y);
  }
  canvas.pushSprite(0, 0);
}

void drawEdit() {
  canvas.fillSprite(TFT_BLACK);
  const bool off = editItem == M_OFFSET;
  String hint;
  if (off) hint = "learned now: " + (settings.c > 0 ? "c " + String(settings.c, 1) + " s" : String(settings.offset, 2) + " bar");
  else     hint = "last auto: " + String(settings.settleLearn, 1) + " s";
  drawLines({ {off ? "OFFSET WHILE PUMPING" : "SETTLE BEFORE MEASURE", F_TEXT, TFT_CYAN},
              {off ? offText(editIdx) : setText(editIdx), F_LARGE, TFT_GREENYELLOW, F_MID},
              {hint, F_SMALL, TFT_LIGHTGREY}, {"turn: change  press: save", F_SMALL, TFT_LIGHTGREY} });
  canvas.pushSprite(0, 0);
}

void drawCustom() {   // CUSTOM preset value, cyan big number (orange below TARGET_LOW_WARN)
  canvas.fillSprite(TFT_BLACK);
  const bool low = customEdit < TARGET_LOW_WARN;
  drawLines({ {fmt(customEdit), F_DIGIT, low ? TFT_ORANGE : TFT_CYAN},
              {String(unitName()) + (low ? "  LOW" : ""), F_TEXT, low ? TFT_ORANGE : TFT_LIGHTGREY},
              {"CUSTOM: turn 0.1, press: OK", F_TEXT, TFT_WHITE} });
  canvas.pushSprite(0, 0);
}

void drawCal() {
  canvas.fillSprite(TFT_BLACK);
  const int i = cal.index();
  int top = 0;
  if (cal.state() != CState::DONE && cal.state() != CState::FAILED) top = drawTopBar(mmss(cal.totalMs()));
  const String n = "CAL " + String(i + 1) + "/4";
  switch (cal.state()) {
    case CState::ZERO_WAIT:
      drawLines({ {"CAL 1/4", F_TEXT, TFT_WHITE}, {"hose open to air", F_TEXT, TFT_WHITE}, {"press when ready", F_SMALL, TFT_LIGHTGREY} }, top); break;
    case CState::PUMPING:
      drawLines({ {n + ": pumping to ~" + String(cal.points[i], 0), F_TEXT, TFT_WHITE},
                  {mmss(cal.phaseMs()), F_LARGE, TFT_ORANGE, F_MID}, {"press: abort", F_SMALL, TFT_LIGHTGREY} }, top); break;
    case CState::SETTLING: {
      const uint32_t e = cal.phaseMs(), L = Calibration::SETTLE_TOTAL_MS;
      drawLines({ {n + ": settling", F_TEXT, TFT_WHITE}, {String((e >= L ? 0 : L - e) / 1000.0f, 1), F_DIGIT, TFT_CYAN} }, top);
    } break;
    case CState::ENTER:
      drawLines({ {"Gauge shows?", F_TEXT, TFT_WHITE}, {String(cal.entry(), 2), F_DIGIT, TFT_GREENYELLOW},
                  {"turn + press", F_SMALL, TFT_LIGHTGREY} }, top); break;
    case CState::DONE: {   // plain words: how wrong the sensor was, and how good the result is
      const float pct = (settings.calA - 1.0f) * 100.0f;   // a > 1: sensor read low
      const float z = -settings.calB;                       // b < 0: sensor read high at zero
      drawLines({ {"CALIBRATION SAVED", F_TEXT, TFT_GREEN},
                  {fabsf(pct) < 0.2f ? "Sensor slope: exact" : "Sensor reads " + String(fabsf(pct), 1) + "% " + (pct > 0 ? "low" : "high"), F_TEXT, TFT_WHITE},
                  {fabsf(z) < 0.01f ? "Zero: exact" : "Zero: " + String(fabsf(z), 2) + " bar " + (z > 0 ? "high" : "low"), F_TEXT, TFT_WHITE},
                  {"Accuracy +/-" + String(cal.fitErr(), 2) + " bar", F_TEXT, TFT_WHITE} });
    } break;
    case CState::FAILED:
      drawLines({ {"CALIBRATION FAILED", F_TEXT, TFT_RED}, {"old calibration kept", F_SMALL, TFT_LIGHTGREY}, {"press: back", F_SMALL, TFT_LIGHTGREY} }); break;
    default: break;
  }
  canvas.pushSprite(0, 0);
}

void drawOta() {
  canvas.fillSprite(TFT_BLACK);
  drawLines({ {"UPDATE MODE", F_TEXT, TFT_CYAN}, {"WiFi " OTA_SSID, F_TEXT, TFT_WHITE}, {"pass " OTA_PASS, F_TEXT, TFT_WHITE},
              {"192.168.4.1  (log: /log)", F_TEXT, TFT_WHITE, F_SMALL},
              {ota.progress() ? String(ota.progress() / 1024) + " kB received" : "long press: exit", F_SMALL, TFT_LIGHTGREY} });
  canvas.pushSprite(0, 0);
}

void drawVersion() {
  canvas.fillSprite(TFT_BLACK);
  drawLines({ {"Wrangler Air " FW_VERSION, F_TEXT, TFT_WHITE},
              {"off " + String(settings.offset, 2) + "  c " + String(settings.c, 1) + (settings.offAuto ? " AUTO" : " MAN"), F_SMALL, TFT_LIGHTGREY},
              {"rate " + String(settings.rate, 3) + "  top " + String(settings.kTop, 3), F_SMALL, TFT_LIGHTGREY},
              {"bias " + String(settings.bias, 2) + "  dev " + String(settings.dev, 2), F_SMALL, TFT_LIGHTGREY},
              {"settle " + String(settings.settleLearn, 1) + "s  starts " + String(pump.starts()), F_SMALL, TFT_LIGHTGREY},
              {String(TYRES[settings.tyre].name) + "  air " + String(settings.airL(), 0) + " L", F_SMALL, TFT_LIGHTGREY},
              {DEFLATE ? "tauD " + String(settings.tauD, 0) + "s " + (settings.learnedD ? "learned" : "model") : "", F_SMALL, TFT_LIGHTGREY},
              {settings.learned ? "learned" : "not learned yet", F_SMALL, TFT_LIGHTGREY} });
  canvas.pushSprite(0, 0);
}

// live sensor check for the bench: what the sensor sends, what the input pin sees (after the divider if any), and the pressure
void drawSensor() {
  canvas.fillSprite(TFT_BLACK);
  const float vg = sensor.vG1(), vs = vg * DIVIDER;
  drawLines({ {"SENSOR CHECK", F_TEXT, TFT_CYAN},
              {"sensor " + String(vs, 2) + " V", F_MID, TFT_WHITE},
              {SENSOR_PIN_NAME " " + String(vg, 2) + " V", F_MID, TFT_GREENYELLOW},
              {sensor.fault() ? "OUT OF RANGE" : fmt(sensor.bar(settings.calA, settings.calB)) + " " + unitName() + "  (raw " + String(sensor.rawBar(), 2) + ")", F_TEXT, sensor.fault() ? TFT_RED : TFT_WHITE},
              {"press: back", F_SMALL, TFT_LIGHTGREY} });
  canvas.pushSprite(0, 0);
}

void applyRotation() {
  M5.Display.setRotation(settings.rot);
  canvas.deleteSprite();
  canvas.createSprite(M5.Display.width(), M5.Display.height());
}

// ---------- setup / loop ----------
void setup() {
  // outputs first: compressor and K4 must never twitch at boot
  pinMode(PIN_K1, OUTPUT); digitalWrite(PIN_K1, LOW);
  if (PIN_K4 >= 0) { pinMode(PIN_K4, OUTPUT); digitalWrite(PIN_K4, LOW); }
  pinMode(PIN_HOLD, OUTPUT); digitalWrite(PIN_HOLD, HIGH);

  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setBrightness(160);
  settings.load();
  settings.bootN++; settings.save();
  wlog::begin(settings.bootN);
  wlog::ev("BOOT", wlog::kv("fw", FW_VERSION) + wlog::kv("tyre", String(TYRES[settings.tyre].name)) + wlog::kv("air_L", settings.airL(), 0) + wlog::kv("target", settings.target, 2) +
                   wlog::kv("offset", settings.offset) + wlog::kv("c", settings.c, 2) + wlog::kv("rate", settings.rate, 4) +
                   wlog::kv("kTop", settings.kTop, 4) + wlog::kv("bias", settings.bias) + wlog::kv("dev", settings.dev) +
                   wlog::kv("learned", settings.learned ? "1" : "0") + wlog::kv("tauD", settings.tauD, 1) +
                   wlog::kv("learnedD", settings.learnedD ? "1" : "0") + wlog::kv("calA", settings.calA, 4) + wlog::kv("calB", settings.calB));
  applyRotation();     // WIDE / PORTRAIT, chosen in the menu
  sensor.begin();
  pump.begin(&sensor, &settings);
  cal.begin(&sensor, &settings);
  input::begin();
}

void loop() {
  M5.update();
  sensor.update();
  pump.update();
  cal.update();
  ota.update();

  const int steps = input::takeSteps();
  const input::Btn b = input::button();

  switch (screen) {
    case Screen::MAIN: {
      if (steps && !pump.busy()) {
        // the knob steps through the typical pressures (config.h PRESETS), clockwise = higher; CUSTOM is last
        int i = constrain((int)settings.preset - steps, 0, N_PRESETS - 1);
        while (settings.presetBar(i) < TARGET_MIN && i > 0) i--;
        settings.preset = i; settings.target = settings.presetBar(i);
        pump.clearResult();
      }
      static bool dirty = false; static uint32_t lastTurn = 0;
      if (steps) { dirty = true; lastTurn = millis(); }
      if (dirty && millis() - lastTurn > 2000) { settings.save(); dirty = false; }   // spare the flash

      if (b == input::Btn::SHORT) {
        if (pump.busy()) pump.stop();
        else if (pump.state() == PState::DONE || pump.state() == PState::ERROR) pump.clearResult();
        else if (settings.preset == P_CUSTOM) { customEdit = settings.custom; screen = Screen::CUSTOM; }   // check the value first
        else pump.start();
      }
      if (b == input::Btn::LONG && !pump.busy()) { screen = Screen::MENU; menuPos = 0; }
      static PState prev = PState::IDLE;
      if (pump.state() != prev) {
        if (pump.state() == PState::DONE)  beep(2);
        if (pump.state() == PState::ERROR) beep(4);
        prev = pump.state();
      }
      drawMain();
    } break;

    case Screen::MENU:
      if (steps) menuPos = constrain(menuPos + steps, 0, MENU_N - 1);
      if (b == input::Btn::LONG) screen = Screen::MAIN;
      if (b == input::Btn::SHORT) {
        switch (menuPos) {
          case M_CAL:     cal.start(); screen = Screen::CAL; break;
          case M_SENSOR:  screen = Screen::SENSOR; break;
          case M_UNITS:   settings.psi = !settings.psi; settings.save(); break;
          case M_TYRE: {  // press: next size; learned values are scaled by the air volume
            const uint8_t old = settings.tyre;
            settings.setTyre((settings.tyre + 1) % N_TYRE_SIZES);
            wlog::ev("TYRE", wlog::kv("from", String(TYRES[old].name)) + wlog::kv("to", String(TYRES[settings.tyre].name)) +
                             wlog::kv("air_L", settings.airL(), 0) + wlog::kv("rate", settings.rate, 4) + wlog::kv("tauD", settings.tauD, 1));
          } break;
          case M_OFFSET:  editItem = M_OFFSET; editIdx = offIdx(); screen = Screen::EDIT; break;
          case M_SETTLE:  editItem = M_SETTLE; editIdx = setIdx(); screen = Screen::EDIT; break;
          case M_SCREEN:  settings.rot = (settings.rot + 1) % 4; settings.save(); applyRotation(); break;
          case M_RESET:   // two presses within 3 s, so a stray click cannot wipe what was learned
            if (resetArm && millis() - resetArm < 3000) { settings.resetLearning(); wlog::ev("RESET_LEARNING", ""); beep(); resetArm = 0; }
            else resetArm = millis();
            break;
          case M_UPDATE:  ota.start(); screen = Screen::OTA; break;
          case M_VERSION: screen = Screen::VERSION; break;
          default: screen = Screen::MAIN; break;
        }
      }
      if (screen == Screen::MENU) drawMenu();
      break;

    case Screen::EDIT:
      if (steps) editIdx = constrain(editIdx + steps, 0, editItem == M_OFFSET ? OFF_N : SET_N);
      if (b == input::Btn::SHORT) {
        if (editItem == M_OFFSET) { settings.offAuto = editIdx == 0; if (editIdx) settings.offManual = offVal(editIdx); }
        else                      { settings.settleAuto = editIdx == 0; if (editIdx) settings.settleManual = setVal(editIdx); }
        settings.save(); beep(); screen = Screen::MENU;
      }
      if (b == input::Btn::LONG) screen = Screen::MENU;   // leave without saving
      if (screen == Screen::EDIT) drawEdit();
      break;

    case Screen::CUSTOM:   // CUSTOM preset: turn 0.1 bar, press = save and start, long = cancel
      if (steps) customEdit = roundf(constrain(customEdit + steps * TARGET_STEP, TARGET_MIN, TARGET_MAX) * 10) / 10.0f;
      if (b == input::Btn::SHORT) {
        settings.custom = settings.target = customEdit; settings.save();
        wlog::ev("CUSTOM_SET", wlog::kv("custom", settings.custom, 2));
        screen = Screen::MAIN; pump.start();
      }
      if (b == input::Btn::LONG) screen = Screen::MAIN;
      if (screen == Screen::CUSTOM) drawCustom();
      break;

    case Screen::CAL:
      if (steps) cal.turn(steps);
      if (b == input::Btn::SHORT) {
        if (cal.state() == CState::PUMPING || cal.state() == CState::SETTLING) cal.abort();
        else cal.press();
      }
      if (b == input::Btn::LONG) cal.abort();
      if (!cal.active()) screen = Screen::MAIN; else drawCal();
      break;

    case Screen::OTA:
      if (b == input::Btn::LONG) { ota.stop(); screen = Screen::MAIN; }
      else drawOta();
      break;

    case Screen::VERSION:
      if (b != input::Btn::NONE) screen = Screen::MENU;
      else drawVersion();
      break;

    case Screen::SENSOR:
      if (b != input::Btn::NONE) screen = Screen::MENU;
      else drawSensor();
      break;
  }
  delay(20);
}
