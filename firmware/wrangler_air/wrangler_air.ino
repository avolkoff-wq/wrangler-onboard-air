// Wrangler Onboard Air - M5Stack DinMeter controller for the ARB Twin compressor
// Stage 1: K1 (G2) switches the compressor relay K2. Measuring pauses = compressor off.
//
// Controls
//   turn            set target (0.1 bar steps, max 3.0 bar)
//   press           start / stop
//   long press      menu: CALIBRATE, UNITS, OFFSET, SETTLE, RESET LEARNING, UPDATE, VERSION, EXIT
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
#include "input.h"

Settings settings;
Sensor sensor;
Pump pump;
Calibration cal;
Ota ota;
M5Canvas canvas(&M5.Display);

enum class Screen { MAIN, MENU, EDIT, CAL, OTA, VERSION };
Screen screen = Screen::MAIN;
int menuPos = 0;
enum MenuItem { M_CAL, M_UNITS, M_OFFSET, M_SETTLE, M_RESET, M_UPDATE, M_VERSION, M_EXIT };
const char* MENU[] = {"CALIBRATE", "UNITS", "OFFSET", "SETTLE", "RESET LEARNING", "UPDATE", "VERSION", "EXIT"};
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
int offIdx() { return settings.offAuto ? 0 : constrain((int)roundf(settings.offManual / 0.05f), 1, OFF_N); }
int setIdx() { return settings.settleAuto ? 0 : constrain((int)roundf((settings.settleManual - 0.5f) / 0.5f), 1, SET_N); }
String offText(int i) { return i == 0 ? "AUTO" : String(offVal(i), 2) + " bar"; }
String setText(int i) { return i == 0 ? "AUTO" : String(setVal(i), 1) + " s"; }

void beep(int n = 1) {
  for (int i = 0; i < n; i++) { M5.Speaker.tone(3500, 120); delay(180); }
}

// ---------- drawing ----------
void drawMain() {
  canvas.fillSprite(TFT_BLACK);
  const PState st = pump.state();
  const bool est = pump.estimating();
  const float shown = (st == PState::IDLE) ? sensor.bar(settings.calA, settings.calB)
                                           : (est ? pump.estimate() : pump.staticBar());
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::Font7);
  canvas.setTextSize(1);
  if (pump.paused() && !sensor.fault()) {
    // pause: big countdown to the end of the measurement
    canvas.setTextColor(TFT_CYAN);
    canvas.drawString(String(pump.pauseLeftMs() / 1000.0f, 1), 120, 52);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextColor(TFT_LIGHTGREY);
    canvas.drawString("s", 120, 92);
  } else {
    canvas.setTextColor(sensor.fault() ? TFT_RED : TFT_GREENYELLOW);
    canvas.drawString(sensor.fault() ? "--.-" : fmt(max(0.0f, shown)), 120, 52);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextColor(TFT_LIGHTGREY);
    canvas.drawString(String(est ? "~ " : "") + unitName(), 120, 92);
  }

  // top line: whole run time and compressor starts
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextDatum(top_left);
  canvas.setTextColor(TFT_LIGHTGREY);
  if (st != PState::IDLE) canvas.drawString(mmss(pump.runMs()) + "  starts " + String(pump.starts()), 4, 2);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(middle_center);

  const String tm = "  " + phaseTimer(pump.phaseMs(), pump.phaseLenMs());
  String line; uint16_t col = TFT_WHITE;
  switch (st) {
    case PState::IDLE:      line = "SET " + fmt(settings.target) + "  press: start"; break;
    case PState::SETTLE:    line = "SETTLING -> " + fmt(settings.target); col = TFT_CYAN; break;
    case PState::MEASURE:   line = "MEASURING -> " + fmt(settings.target); col = TFT_CYAN; break;
    case PState::CAL_PULSE: line = "LEARNING" + tm; col = TFT_ORANGE; break;
    case PState::MAIN:      line = "PUMP -> " + fmt(settings.target) + tm; col = TFT_ORANGE; break;
    case PState::TOPUP:     line = "TOP-UP" + tm; col = TFT_ORANGE; break;
    case PState::DONE:      line = "DONE " + fmt(pump.staticBar()) + "  " + mmss(pump.runMs()); col = TFT_GREEN; break;
    case PState::ERROR: {
      col = TFT_RED;
      switch (pump.error()) {
        case PErr::SENSOR:  line = "ERR SENSOR"; break;
        case PErr::OVER:    line = "ERR OVER 3.0 BAR"; break;
        case PErr::LEAK:    line = "ERR NO RISE: LEAK?"; break;
        case PErr::TIMEOUT: line = "ERR TIMEOUT"; break;
        default:            line = "ERR"; break;
      }
    } break;
  }
  canvas.setTextColor(col);
  canvas.drawString(line, 120, 118);
  if (pump.compressorOn()) canvas.fillCircle(228, 10, 6, TFT_ORANGE);
  canvas.pushSprite(0, 0);
}

void drawMenu() {
  canvas.fillSprite(TFT_BLACK);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(middle_left);
  const int first = constrain(menuPos - 2, 0, max(0, MENU_N - 5));
  for (int k = 0; k < 5 && first + k < MENU_N; k++) {
    const int i = first + k;
    const int y = 14 + k * 26;
    if (i == menuPos) canvas.fillRoundRect(4, y - 11, 232, 22, 4, TFT_DARKGREEN);
    canvas.setTextColor(TFT_WHITE);
    String t = MENU[i];
    if (i == M_UNITS)  t += String(": ") + unitName();
    if (i == M_OFFSET) t += ": " + offText(offIdx());
    if (i == M_SETTLE) t += ": " + setText(setIdx());
    canvas.drawString(t, 12, y);
  }
  canvas.pushSprite(0, 0);
}

void drawEdit() {
  canvas.fillSprite(TFT_BLACK);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(TFT_CYAN);
  const bool off = editItem == M_OFFSET;
  canvas.drawString(off ? "OFFSET WHILE PUMPING" : "SETTLE BEFORE MEASURE", 120, 14);
  canvas.setFont(&fonts::FreeSansBold18pt7b);
  canvas.setTextColor(TFT_GREENYELLOW);
  canvas.drawString(off ? offText(editIdx) : setText(editIdx), 120, 58);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(TFT_LIGHTGREY);
  String hint;
  if (off) hint = "learned now: " + (settings.c > 0 ? "c " + String(settings.c, 1) + " s" : String(settings.offset, 2) + " bar");
  else     hint = "last auto: " + String(settings.settleLearn, 1) + " s";
  canvas.drawString(hint, 120, 96);
  canvas.drawString("turn: change  press: save", 120, 120);
  canvas.pushSprite(0, 0);
}

void drawCal() {
  canvas.fillSprite(TFT_BLACK);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(TFT_WHITE);
  const int i = cal.index();
  if (cal.state() != CState::DONE && cal.state() != CState::FAILED) {
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextDatum(top_left);
    canvas.setTextColor(TFT_LIGHTGREY);
    canvas.drawString(mmss(cal.totalMs()), 4, 2);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(TFT_WHITE);
  }
  switch (cal.state()) {
    case CState::ZERO_WAIT:
      canvas.drawString("CAL 1/4: hose open to air", 120, 40);
      canvas.drawString("press when ready", 120, 80); break;
    case CState::PUMPING:
      canvas.drawString("CAL " + String(i + 1) + "/4: pumping to ~" + String(cal.points[i], 0), 120, 40);
      canvas.setTextColor(TFT_ORANGE);
      canvas.drawString(mmss(cal.phaseMs()), 120, 66);
      canvas.setTextColor(TFT_WHITE);
      canvas.drawString("press: abort", 120, 90); break;
    case CState::SETTLING:
      canvas.drawString("CAL " + String(i + 1) + "/4: settling", 120, 36);
      canvas.setFont(&fonts::Font7);
      canvas.setTextColor(TFT_CYAN);
      { const uint32_t e = cal.phaseMs(), L = Calibration::SETTLE_TOTAL_MS;
        canvas.drawString(String((e >= L ? 0 : L - e) / 1000.0f, 1), 120, 86); } break;
    case CState::ENTER:
      canvas.drawString("Gauge shows? turn + press", 120, 25);
      canvas.setFont(&fonts::Font7);
      canvas.setTextColor(TFT_GREENYELLOW);
      canvas.drawString(String(cal.entry(), 2), 120, 80); break;
    case CState::DONE:
      canvas.setTextColor(TFT_GREEN);
      canvas.drawString("CALIBRATION SAVED", 120, 20);
      canvas.setTextColor(TFT_WHITE);
      canvas.drawString("scale x" + String(settings.calA, 3), 120, 50);
      canvas.drawString("zero " + String(settings.calB, 2) + " bar", 120, 76);
      canvas.drawString("fit error " + String(cal.fitErr(), 2) + " bar", 120, 102); break;
    case CState::FAILED:
      canvas.setTextColor(TFT_RED);
      canvas.drawString("CALIBRATION FAILED", 120, 55);
      canvas.drawString("press: back", 120, 90); break;
    default: break;
  }
  canvas.pushSprite(0, 0);
}

void drawOta() {
  canvas.fillSprite(TFT_BLACK);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(TFT_CYAN);
  canvas.drawString("UPDATE MODE", 120, 14);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("WiFi " OTA_SSID, 120, 44);
  canvas.drawString("pass " OTA_PASS, 120, 68);
  canvas.drawString("http://192.168.4.1", 120, 92);
  canvas.setTextColor(TFT_LIGHTGREY);
  canvas.drawString(ota.progress() ? String(ota.progress() / 1024) + " kB received" : "long press: exit", 120, 120);
  canvas.pushSprite(0, 0);
}

void drawVersion() {
  canvas.fillSprite(TFT_BLACK);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("Wrangler Air " FW_VERSION, 120, 12);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(TFT_LIGHTGREY);
  canvas.drawString("off " + String(settings.offset, 2) + "  c " + String(settings.c, 1) + (settings.offAuto ? "  AUTO" : "  MAN"), 120, 38);
  canvas.drawString("rate " + String(settings.rate, 3) + "  top " + String(settings.kTop, 3), 120, 60);
  canvas.drawString("bias " + String(settings.bias, 2) + "  dev " + String(settings.dev, 2), 120, 82);
  canvas.drawString("settle " + String(settings.settleLearn, 1) + "s  starts " + String(pump.starts()), 120, 104);
  canvas.drawString(settings.learned ? "learned" : "not learned yet", 120, 124);
  canvas.pushSprite(0, 0);
}

// ---------- setup / loop ----------
void setup() {
  // outputs first: compressor and K4 must never twitch at boot
  pinMode(PIN_K1, OUTPUT); digitalWrite(PIN_K1, LOW);
  pinMode(PIN_K4, OUTPUT); digitalWrite(PIN_K4, LOW);
  pinMode(PIN_HOLD, OUTPUT); digitalWrite(PIN_HOLD, HIGH);

  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(160);
  canvas.createSprite(M5.Display.width(), M5.Display.height());

  settings.load();
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
        settings.target = constrain(settings.target + steps * TARGET_STEP, 0.0f, TARGET_MAX);
        settings.target = roundf(settings.target * 10) / 10.0f;
        pump.clearResult();
      }
      static bool dirty = false; static uint32_t lastTurn = 0;
      if (steps) { dirty = true; lastTurn = millis(); }
      if (dirty && millis() - lastTurn > 2000) { settings.save(); dirty = false; }   // spare the flash

      if (b == input::Btn::SHORT) {
        if (pump.busy()) pump.stop();
        else if (pump.state() == PState::DONE || pump.state() == PState::ERROR) pump.clearResult();
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
          case M_UNITS:   settings.psi = !settings.psi; settings.save(); break;
          case M_OFFSET:  editItem = M_OFFSET; editIdx = offIdx(); screen = Screen::EDIT; break;
          case M_SETTLE:  editItem = M_SETTLE; editIdx = setIdx(); screen = Screen::EDIT; break;
          case M_RESET:   settings.resetLearning(); beep(); break;
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
  }
  delay(20);
}
