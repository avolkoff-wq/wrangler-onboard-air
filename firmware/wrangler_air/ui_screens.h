// Screen drawing functions extracted from wrangler_air.ino
// Header-only: included once from the .ino after shared state is declared.
#pragma once
#include <M5Unified.h>
#include "config.h"
#include "storage.h"
#include "sensor.h"
#include "pump.h"
#include "calibration.h"
#include "ota.h"
#include "input.h"
#include "ui_draw.h"

extern M5Canvas canvas;
extern Settings settings;
extern Sensor sensor;
extern Supply supply;
extern Pump pump;
extern Calibration cal;
extern Ota ota;
extern bool iconDim;
extern int hintTop;
extern bool noAir;
extern bool armed;
extern uint8_t armStage;
extern uint32_t armT;
extern float customEdit;
extern int customStep;
extern uint8_t customWheels;
extern int menuPos;
extern int editItem, editIdx;
extern uint32_t resetArm;
extern int learnPos;
extern Mode learnSel;
extern bool learnArm;
extern bool calEngArm;
extern String learnMsg;
extern int outPos;
extern const char* const MENU[];
extern const int MENU_N;
extern const int OFF_N;
extern const int OMX_N;
extern const int SET_N;

IdleDir idleDir(float bar);
Eng engine();
const char* engText(Eng e);
uint16_t engColor(Eng e);
bool filtersDue();
int offIdx();
int omxIdx();
float omxVal(int i);
int setIdx();
Mode learnItem(int i);
void push();

// timing constants used by idle / hold UI (kept here so screens compile standalone of .ino order)
#ifndef UI_ARM_MS
#define UI_ARM_MS 3000
constexpr uint32_t ARM_MS = UI_ARM_MS;
constexpr uint32_t HOLD_SHOW_MS = 300;
#endif

#ifndef LEARN_N
#define LEARN_N (DEFLATE ? 3 : 2)
#endif
#ifndef OUT_N
#define OUT_N (DEFLATE ? 3 : 2)
#endif

void drawHoldBar() {
  const uint32_t h = input::heldMs();
  if (h < HOLD_SHOW_MS) return;
  const float f = constrain((float)(h - HOLD_SHOW_MS) / (input::BTN_LONG_MS - HOLD_SHOW_MS), 0.0f, 1.0f);
  const int top = min(hintTop, H() - 20);
  canvas.fillRect(0, top, W(), H() - top, TFT_BLACK);
  textAt("HOLD FOR MENU", W() / 2, H() - 19, &fonts::Font2, TFT_WHITE, top_center);
  canvas.fillRect(0, H() - 3, (int)(W() * f), 3, TFT_WHITE);
}

// idle screen and the CUSTOM value screen (edit = true): icon, big target value, unit, preset name and, below a
// coloured rule, the live pressure and what a start would do (spec: docs/screen/screen_mockup_idle_direction.py)
void drawIdle(bool edit) {
  canvas.fillSprite(TFT_BLACK);
  const int p = settings.preset;
  const float bar = edit ? customEdit : settings.target;
  const bool low = bar < TARGET_LOW_WARN;
  const float now = sensor.bar(settings.calA, settings.calB);
  const IdleDir dir = idleDir(bar);
  const bool dead = dir == D_DEAD && !edit;
  const char* word = dir == D_UP ? "INFLATE" : dir == D_DOWN ? "DEFLATE" : dir == D_OK ? "= OK" : "ABOVE";
  const uint16_t dc = dir == D_UP ? TFT_ORANGE : dir == D_DOWN ? TFT_SKYBLUE : dir == D_OK ? rgb(0, 220, 0) : C_GREY;
  const uint16_t dim = rgb(55, 55, 55);
  const uint16_t numc = edit ? TFT_CYAN : dead ? dim : TFT_GREENYELLOW;
  const uint16_t namec = edit ? TFT_CYAN : dead ? dim : low ? TFT_ORANGE : TFT_WHITE;
  const uint16_t unitc = dead ? dim : C_LGREY;
  const bool wsel = edit && customStep == 1;                              // CUSTOM, second step: wheel count
  const uint8_t nw = edit ? customWheels : settings.wheels;
  const String name = String(PRESETS[p].name) + (p == P_CUSTOM && (nw < 4 || wsel) ? (portrait() ? " x" + String(nw) : "  " + String(nw) + (nw == 1 ? " WHEEL" : " WHEELS")) : ""),
               nowT = String("actual ") + fmt(max(0.0f, now));
  const String valS = wsel ? String(nw) : String(fmt(bar)), unitS = wsel ? (nw == 1 ? "wheel" : "wheels") : unitName();
  // "now x.x", triangle, word; one line centred at y, or two lines (portrait)
  auto dirLine = [&](int y, bool twoLines) {
    canvas.setFont(F_TEXT);
    const bool tri = dir == D_UP || dir == D_DOWN;
    const int th = 11, tw = 12, gap = 6;
    const int w1 = canvas.textWidth(nowT), w2 = canvas.textWidth(word) + (tri ? tw + 4 : 0);
    auto part2 = [&](int x, int yy) {   // triangle + word from x
      if (tri) {
        const int cy = yy + canvas.fontHeight() / 2 - 1;
        if (dir == D_UP) canvas.fillTriangle(x, cy + th / 2, x + tw, cy + th / 2, x + tw / 2, cy - th / 2, dc);
        else             canvas.fillTriangle(x, cy - th / 2, x + tw, cy - th / 2, x + tw / 2, cy + th / 2, dc);
        x += tw + 4;
      }
      textAt(word, x, yy, F_TEXT, dc, top_left);
    };
    if (!twoLines) {
      const int x = (W() - (w1 + gap + w2)) / 2;
      textAt(nowT, x, y, F_TEXT, dc, top_left); part2(x + w1 + gap, y);
    } else {
      textAt(nowT, W() / 2, y, F_TEXT, dc, top_center); part2((W() - w2) / 2, y + 17);
    }
  };
  const bool hint = dir == D_UP || dir == D_DOWN;                      // "press to start" only when a start would do something
  auto idleHint = [&]() {
    if (edit) { drawHint(wsel ? "turn: wheels  press: OK  hold: back" : "turn: 0.1  press: next  hold: cancel"); return; }
    if (armed && !dead) {   // ARMED: white hint, a bar running out over ARM_MS under it
      drawHint(armStage == 2 ? "PRESS AGAIN: CONFIRM  turn: cancel" : "PRESS AGAIN: START  turn: cancel", armStage == 2 ? TFT_RED : TFT_WHITE, H() - 4);
      const float left = 1.0f - constrain((float)(millis() - armT) / ARM_MS, 0.0f, 1.0f);
      canvas.fillRect(20, H() - 3, (int)((W() - 40) * left), 3, TFT_WHITE);
      return;
    }
    // portrait has no free row: the filter note takes the hint line every other 1.5 s
    if (portrait() && filtersDue() && (millis() / 1500) % 2) { drawHint("CLEAN FILTERS", TFT_ORANGE); return; }
    drawHint(hint && !dead ? "turn: mode  press: start  hold: menu" : "turn: mode  hold: menu");
  };
  iconDim = dead;
  if (!portrait()) {
    drawPresetIcon(p, 6, 6, 104);
    iconDim = false;
    bigValue(valS, false, W() - 8, 2, numc, 1);
    textAt(unitS, W() - 8, 56, F_TEXT, unitc, top_right);
    if (!edit && filtersDue()) {   // small, not blocking: left of the unit, right of the icon
      canvas.setFont(F_TEXT); const int uw = canvas.textWidth(unitName());
      textAt("CLEAN FILTERS", W() - 8 - uw - 6, 58, &fonts::Font2, dead ? dim : TFT_ORANGE, top_right);
    }
    textAt(name + (low && !edit ? "  LOW" : ""), W() / 2, 72, F_MID, namec, top_center, F_TEXT, W() - 8);
    if (!edit) {
      canvas.fillRect(10, 94, W() - 20, 2, dead ? dim : dc);
      if (dead) textAt("CONNECT SYSTEM", W() / 2, 99, F_TEXT, C_LGREY, top_center);
      else if (armed && dir == D_UP && *engText(engine()))                  // compressor ahead: engine off / unknown
        textAt(engText(engine()), W() / 2, 99, F_TEXT, engColor(engine()), top_center);
      else dirLine(99, false);
    }
    idleHint();
  } else {
    drawPresetIcon(p, 6, !strcmp(PRESETS[p].name, "ROCK") ? 14 : 8, 123);   // tilted ROCK SUV would touch the top edge
    iconDim = false;
    textAt(name, W() / 2, 84, F_MID, namec, top_center, F_TEXT, W() - 6);
    bigValue(valS, false, W() / 2, 100, numc, 0);
    textAt(unitS + (low && !edit ? "  LOW" : ""), W() / 2, 150, F_TEXT, low && !edit && !dead ? TFT_ORANGE : unitc, top_center);
    if (!edit) {
      canvas.fillRect(10, 169, W() - 20, 2, dead ? dim : dc);
      if (dead) {
        textAt("CONNECT", W() / 2, 173, F_TEXT, C_LGREY, top_center);
        textAt("SYSTEM", W() / 2, 190, F_TEXT, C_LGREY, top_center);
      } else if (armed && dir == D_UP && *engText(engine())) {   // compressor ahead: engine off / unknown
        dirLine(173, true);
        canvas.fillRect(0, 189, W(), 17, TFT_BLACK);
        textAt(engText(engine()), W() / 2, 190, F_TEXT, engColor(engine()), top_center);
      } else dirLine(173, true);
    }
    idleHint();
  }
  if (!edit) drawHoldBar();
  push();
}

// result of a menu LEARN test
void drawLearnResult() {
  canvas.fillSprite(TFT_BLACK);
  const bool def = pump.testMode() == Mode::DEF;
  if (pump.testOk())
    drawLines({ {def ? "LEARN DEFLATE OK" : "LEARN INFLATE OK", F_TEXT, TFT_GREEN},
                {def ? String(pump.testResult(), 0) : String(pump.testResult(), 3), F_DIGIT, TFT_GREENYELLOW},
                {def ? "tau, s" : "rate, bar/s", F_TEXT, C_LGREY} }, 0, H() - 18);
  else
    drawLines({ {def ? "LEARN DEFLATE" : "LEARN INFLATE", F_TEXT, TFT_ORANGE}, {"NO RESULT", F_MID, TFT_ORANGE},
                {String("before ") + fmt(pump.testBefore()) + String("  after ") + fmt(pump.staticBar()), F_SMALL, C_LGREY} }, 0, H() - 18);
  drawHint("press: back");
  push();
}

void drawMain() {
  const PState st = pump.state();
  if (st == PState::IDLE) { drawIdle(false); return; }
  const bool test = pump.testMode() != Mode::NONE;
  if (test && st == PState::DONE) { drawLearnResult(); return; }
  // background by phase: compressor pumping = dark amber, valve venting = dark blue, pauses and results = black
  canvas.fillSprite(pump.compressorOn() ? rgb(90, 38, 0) : pump.valveOpen() ? rgb(0, 40, 90) : TFT_BLACK);
  const bool est = pump.estimating();
  // after an error always the live line pressure: what the tyres hold right now (stuck valve, air from AUX2, leak)
  const float cur = est ? pump.estimate() : (pump.mode() == Mode::NONE || st == PState::ERROR ? sensor.bar(settings.calA, settings.calB) : pump.staticBar());
  const String tm = phaseTimer(pump.phaseMs(), pump.phaseLenMs());
  const String pause = String(pump.pauseLeftMs() / 1000.0f, 1) + "s";
  String line; uint16_t col = TFT_WHITE;
  switch (st) {
    case PState::SETTLE:    line = "SETTLING " + pause; col = TFT_CYAN; break;
    case PState::MEASURE:   line = "MEASURING " + pause; col = TFT_CYAN; break;
    case PState::CAL_PULSE: line = String(test ? "LEARN " : "LEARNING ") + tm; col = TFT_ORANGE; break;
    case PState::MAIN:      line = String("PUMP ") + tm; col = TFT_ORANGE; break;
    case PState::TOPUP:     line = String("TOP-UP ") + tm; col = TFT_ORANGE; break;
    case PState::VENT:      line = String(test ? "LEARN " : "VENT ") + tm; col = TFT_SKYBLUE; break;
    case PState::DONE:
      line = pump.inBand() ? "OK, WITHIN 0.1" : String("DONE ") + fmt(pump.staticBar()) + String("  ") + mmss(pump.runMs()); col = TFT_GREEN; break;
    case PState::ERROR:
      col = TFT_RED;
      switch (pump.error()) {
        case PErr::SENSOR:  line = "ERR SENSOR"; break;
        case PErr::OVER:    line = "ERR OVER 3.0 BAR"; break;
        case PErr::LEAK:    line = "ERR NO RISE: LEAK?"; break;
        case PErr::TIMEOUT: line = "ERR TIMEOUT"; break;
        case PErr::LOW_P:   line = String("PUMP UP TO ") + fmt(LEARN_DEF_MIN_BAR) + " FIRST"; break;
        case PErr::LOW_V:   line = "ERR LOW VOLTAGE"; break;
        case PErr::VALVE:   line = "ERR VALVE OPEN"; break;
        case PErr::PUMP_ON: line = "NO SIGNAL, PRESSURE RISING"; break;   // the controller keeps the compressor off, yet air comes
        default:            line = "ERR"; break;
      }
      break;
    default: break;
  }
  const String mode = pump.mode() == Mode::DEF ? "DEF " : pump.mode() == Mode::INF ? "INF " : "";
  const String counts = "starts " + String(pump.starts()) + (DEFLATE ? "  vents " + String(pump.vents()) : "");
  const String val = sensor.fault() ? String("--.-") : String(fmt(max(0.0f, cur)));
  const uint16_t valc = sensor.fault() ? TFT_RED : TFT_GREENYELLOW;
  int y, x0, x1;
  if (!portrait()) {
    textAt((DEFLATE ? mode : String("")) + mmss(pump.runMs()) + "  " + counts, 4, 2, F_SMALL, C_LGREY, top_left);
    drawPresetIcon(settings.preset, 6, 22, 64);
    bigValue(val, est, W() - 10, 16, valc, 1);
    y = 72; x0 = 10; x1 = W() - 10;
  } else {
    textAt((DEFLATE ? mode : String("")) + mmss(pump.runMs()), 4, 2, F_SMALL, C_LGREY, top_left);
    drawPresetIcon(settings.preset, 22, 22, 90);
    bigValue(val, est, W() / 2, 82, valc, 0);
    y = 146; x0 = 8; x1 = W() - 8;
  }
  // progress bar p0 -> target (LEARN test: time of the pulse), coloured by what is happening now
  const bool known = pump.mode() != Mode::NONE;
  const float p0 = pump.p0(), span = settings.target - p0;
  const bool pulse = test && (st == PState::CAL_PULSE || st == PState::VENT);
  float f = !known ? 0 : fabsf(span) < 0.01f ? 1.0f : constrain((cur - p0) / span, 0.0f, 1.0f);
  if (test) f = pulse ? constrain((float)pump.phaseMs() / max(1UL, (unsigned long)pump.phaseLenMs()), 0.0f, 1.0f)
                     : (pump.starts() + pump.vents() > 0 ? 1.0f : 0.0f);            // before the pulse 0, after it full
  const uint16_t bc = pump.compressorOn() ? TFT_ORANGE : pump.valveOpen() ? TFT_SKYBLUE : (st == PState::DONE ? TFT_GREEN : TFT_CYAN);
  canvas.fillRect(x0, y, x1 - x0 + 1, 13, TFT_BLACK);              // bar inside stays black so the fill reads
  canvas.drawRect(x0, y, x1 - x0 + 1, 13, C_GREY);
  if (known) canvas.fillRect(x0 + 2, y + 2, (int)((x1 - x0 - 4) * f) + 1, 9, bc);
  if (test) {
    textAt("0 s", x0, y + 14, F_SMALL, C_GREY, top_left);
    textAt(String((pump.testMode() == Mode::DEF ? LEARN_DEF_MS : CAL_PULSE_MS) / 1000) + " s", x1, y + 14, F_SMALL, C_GREY, top_right);
  } else {
    if (known) textAt(fmt(p0), x0, y + 14, F_SMALL, C_GREY, top_left);
    textAt(fmt(settings.target), x1, y + 14, F_SMALL, C_GREY, top_right);
  }
  if (!portrait()) textAt(line, W() / 2, 101, F_TEXT, col, top_center, F_SMALL, W() - 4);
  else {
    textAt(line, W() / 2, 178, F_TEXT, col, top_center, F_SMALL, W() - 4);
    textAt(counts, W() / 2, 198, F_SMALL, C_GREY, top_center);
  }
  if (pump.state() == PState::ERROR && pump.error() == PErr::PUMP_ON) drawHint("AUX2 ON? ELSE A RELAY IS STUCK  press: back", TFT_RED);
  else drawHint(pump.busy() ? "press: STOP" : "press: back");
  if (pump.compressorOn()) canvas.fillCircle(W() - 10, 10, 6, TFT_ORANGE);
  if (pump.valveOpen())    canvas.fillCircle(W() - 10, 10, 6, TFT_SKYBLUE);
  push();
}

void drawMenu() {
  canvas.fillSprite(TFT_BLACK);
  const int ht = drawHint("turn: scroll  press: select  hold: back");
  const Font f = portrait() ? F_SMALL : F_TEXT;
  canvas.setFont(f);
  canvas.setTextDatum(middle_left);
  const int rowH = 26, vis = max(1, (ht - 4) / rowH);
  const int first = constrain(menuPos - vis / 2, 0, max(0, MENU_N - vis));
  for (int k = 0; k < vis && first + k < MENU_N; k++) {
    const int i = first + k;
    const int y = 2 + rowH / 2 + k * rowH;
    if (i == menuPos) canvas.fillRoundRect(3, y - 11, W() - 6, 22, 4, TFT_DARKGREEN);
    canvas.setTextColor(TFT_WHITE);
    String t = MENU[i];
    if (i == M_UNITS)  t += String(": ") + unitName();
    if (i == M_TYRE)   t += String(": ") + TYRES[settings.tyre].name;
    if (i == M_OFFSET) t += String(": ") + offText(offIdx());
    if (i == M_OFFMAX) t += String(": ") + (settings.omAuto ? "AUTO " : "") + String(settings.offCap(), 2);
    if (i == M_SETTLE) t += String(": ") + setText(setIdx());
    if (i == M_SCREEN) t += String(": ") + rotName(settings.rot);
    if (i == M_DEMO || i == M_VALVE || i == M_VCHECK || i == M_ECONF || i == M_WDOG || i == M_VSTUCK || i == M_PSTUCK) {
      const bool on = i == M_DEMO ? g_demo : i == M_VALVE ? settings.valve : i == M_VCHECK ? settings.vcheck : i == M_WDOG ? settings.wdog : i == M_VSTUCK ? settings.vchk2 : i == M_PSTUCK ? settings.pchk : settings.econfirm;
      t = String(on ? "[x] " : "[ ] ") + t;
      if (i == M_ECONF && portrait()) t = String(on ? "[x] " : "[ ] ") + "ENG CONFIRM";
    }
    if (i == M_RESET && resetArm && millis() - resetArm < 3000) t = "PRESS AGAIN";
    else if (i == M_RESET && portrait()) t = "RESET LEARN";
    canvas.drawString(t, 8, y);
  }
  push();
}

// ---------- menu LEARN ----------
void drawLearnPick() {
  canvas.fillSprite(TFT_BLACK);
  drawHint("turn: scroll  press: select  hold: back");
  canvas.setFont(portrait() ? F_SMALL : F_TEXT); canvas.setTextDatum(middle_left);
  for (int i = 0; i < LEARN_N; i++) {
    const int y = 16 + i * 30;
    if (i == learnPos) canvas.fillRoundRect(3, y - 11, W() - 6, 22, 4, TFT_DARKGREEN);
    const Mode m = learnItem(i);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(m == Mode::INF ? "LEARN INFLATE" : m == Mode::DEF ? "LEARN DEFLATE" : "BACK", 8, y);
  }
  push();
}
void drawLearnConf() {
  canvas.fillSprite(TFT_BLACK);
  const bool def = learnSel == Mode::DEF;
  drawLines({ {def ? "LEARN DEFLATE:" : "LEARN INFLATE:", F_TEXT, def ? TFT_SKYBLUE : TFT_ORANGE},
              {def ? "valve " + String(LEARN_DEF_MS / 1000) + " s." : "compressor " + String(CAL_PULSE_MS / 1000) + " s.", F_TEXT, TFT_WHITE},
              {def ? "" : engText(engine()), F_TEXT, engColor(engine())},
              {"Measure, pulse, measure.", F_SMALL, C_LGREY} }, 0, H() - 18);
  drawHint(learnArm ? "PRESS AGAIN: CONFIRM  hold: back" : "press: start  hold: back", learnArm ? TFT_RED : C_LGREY);
  push();
}

void drawEdit() {
  canvas.fillSprite(TFT_BLACK);
  const bool off = editItem == M_OFFSET, omx = editItem == M_OFFMAX;
  String hint, title, val;
  if (off) {
    title = "OFFSET WHILE PUMPING"; val = offText(editIdx);
    hint = "learned now: " + (settings.c > 0 ? "c " + String(settings.c, 1) + " s" : String(settings.offset, 2) + " bar");
  } else if (omx) {   // AUTO shows what it would use now: 1.5 x learned offset + 0.1 (0.60 until learned)
    Settings a = settings; a.omAuto = true;
    title = "MAX OFFSET (SAFETY)"; val = editIdx ? String(omxVal(editIdx), 2) + " bar" : "AUTO " + String(a.offCap(), 2);
    hint = "learned offset " + String(settings.offset, 2) + (settings.learned ? " bar" : " bar (default)");
  } else {
    title = "SETTLE BEFORE MEASURE"; val = setText(editIdx);
    hint = "last auto: " + String(settings.settleLearn, 1) + " s";
  }
  drawLines({ {title, F_TEXT, TFT_CYAN},
              {val, F_LARGE, TFT_GREENYELLOW, F_MID},
              {hint, F_SMALL, TFT_LIGHTGREY} }, 0, H() - 18);
  drawHint("turn: change  press: save  hold: back");
  push();
}

void drawCustom() { drawIdle(true); }   // CUSTOM value: idle layout, value and name in cyan

void drawCal() {
  canvas.fillSprite(TFT_BLACK);
  const int i = cal.index();
  int top = 0;
  if (cal.state() != CState::DONE && cal.state() != CState::FAILED) top = drawTopBar(mmss(cal.totalMs()));
  const String n = "CAL " + String(i + 1) + "/4";
  switch (cal.state()) {
    case CState::ZERO_WAIT:
      // the next press starts the compressor: same engine line and confirmation as LEARN INFLATE (ENGINE CONFIRM)
      drawLines({ {"CAL 1/4", F_TEXT, TFT_WHITE}, {"hose open to air", F_TEXT, TFT_WHITE},
                  {engText(engine()), F_TEXT, engColor(engine())} }, top, H() - 18);
      drawHint(calEngArm ? "PRESS AGAIN: CONFIRM  hold: abort" : "press when ready: next  hold: abort", calEngArm ? TFT_RED : C_LGREY); break;
    case CState::PUMPING:
      drawLines({ {n + ": pumping to ~" + String(cal.points[i], 0), F_TEXT, TFT_WHITE},
                  {mmss(cal.phaseMs()), F_LARGE, TFT_ORANGE, F_MID} }, top, H() - 18);
      drawHint("press: abort"); break;
    case CState::SETTLING: {
      const uint32_t e = cal.phaseMs(), L = Calibration::SETTLE_TOTAL_MS;
      drawLines({ {n + ": settling", F_TEXT, TFT_WHITE}, {String((e >= L ? 0 : L - e) / 1000.0f, 1), F_DIGIT, TFT_CYAN} }, top, H() - 18);
      drawHint("press: abort");
    } break;
    case CState::ENTER:
      drawLines({ {"Gauge shows?", F_TEXT, TFT_WHITE}, {String(cal.entry(), 2), F_DIGIT, TFT_GREENYELLOW} }, top, H() - 18);
      drawHint("turn + press: next  hold: abort"); break;
    case CState::DONE: {   // plain words: how wrong the sensor was, and how good the result is
      const float pct = (settings.calA - 1.0f) * 100.0f;   // a > 1: sensor read low
      const float z = -settings.calB;                       // b < 0: sensor read high at zero
      drawLines({ {"CALIBRATION SAVED", F_TEXT, TFT_GREEN},
                  {fabsf(pct) < 0.2f ? "Sensor slope: exact" : "Sensor reads " + String(fabsf(pct), 1) + "% " + (pct > 0 ? "low" : "high"), F_TEXT, TFT_WHITE},
                  {fabsf(z) < 0.01f ? "Zero: exact" : "Zero: " + String(fabsf(z), 2) + " bar " + (z > 0 ? "high" : "low"), F_TEXT, TFT_WHITE},
                  {"Accuracy +/-" + String(cal.fitErr(), 2) + " bar", F_TEXT, TFT_WHITE} }, 0, H() - 18);
      drawHint("press: back");
    } break;
    case CState::FAILED:
      drawLines({ {"CALIBRATION FAILED", F_TEXT, TFT_RED}, {"old calibration kept", F_SMALL, TFT_LIGHTGREY} }, 0, H() - 18);
      drawHint("press: back"); break;
    default: break;
  }
  push();
}

void drawOta() {
  canvas.fillSprite(TFT_BLACK);
  drawLines({ {"UPDATE MODE", F_TEXT, TFT_CYAN}, {"WiFi " OTA_SSID, F_TEXT, TFT_WHITE},
              {String("pass ") + settings.wifiPass, F_TEXT, TFT_WHITE},
              {"192.168.4.1  (log: /log)", F_TEXT, TFT_WHITE, F_SMALL},
              {ota.progress() ? String(ota.progress() / 1024) + " kB received" : "", F_SMALL, TFT_LIGHTGREY} }, 0, H() - 18);
  drawHint("hold: exit");
  push();
}

void drawVersion() {
  canvas.fillSprite(TFT_BLACK);
  drawLines({ {"Wrangler Air " FW_VERSION, F_TEXT, TFT_WHITE},
              {"off " + String(settings.offset, 2) + "  c " + String(settings.c, 1) + (settings.offAuto ? " AUTO" : " MAN") + "  max " + String(settings.offCap(), 2), F_SMALL, TFT_LIGHTGREY},
              {"rate " + String(settings.rate, 3) + "  top " + String(settings.kTop, 3), F_SMALL, TFT_LIGHTGREY},
              {"bias " + String(settings.bias, 2) + "  dev " + String(settings.dev, 2), F_SMALL, TFT_LIGHTGREY},
              {"settle " + String(settings.settleLearn, 1) + "s  starts " + String(pump.starts()), F_SMALL, TFT_LIGHTGREY},
              {String(TYRES[settings.tyre].name) + "  air " + String(settings.airL(), 0) + " L", F_SMALL, TFT_LIGHTGREY},
              {DEFLATE ? "tauD " + String(settings.tauD, 0) + "s " + (settings.learnedD ? "learned" : "model") : "", F_SMALL, TFT_LIGHTGREY},
              {settings.learned ? "learned" : "not learned yet", F_SMALL, TFT_LIGHTGREY} }, 0, H() - 16);
  drawHint("press: back");
  push();
}
void drawOutTest() {
  canvas.fillSprite(TFT_BLACK);
  drawHint("press: on / off (30 s max)  turn: choose  hold: back");
  canvas.setFont(portrait() ? F_SMALL : F_TEXT); canvas.setTextDatum(middle_left);
  for (int i = 0; i < OUT_N; i++) {
    const int y = 12 + i * 24;
    if (i == outPos) canvas.fillRoundRect(3, y - 11, W() - 6, 22, 4, TFT_DARKGREEN);
    canvas.setTextColor(TFT_WHITE);
    const bool on = i < OUT_N - 1 && pump.pulsing() && pump.pulseIsValve() == (i == 1);
    canvas.drawString(i == OUT_N - 1 ? "BACK" : i == 0 ? "COMPRESSOR (K1)" : "VALVE (K4)", 8, y);
    if (on) { canvas.setTextDatum(middle_right); canvas.setTextColor(i == 1 ? TFT_SKYBLUE : TFT_ORANGE);
              canvas.drawString(String("ON ") + String((pump.pulseLeft() + 999) / 1000) + "s", W() - 8, y); canvas.setTextDatum(middle_left); }
  }
  const int y0 = 8 + OUT_N * 24;
  const float vg = sensor.vG1();
  textAt(sensor.fault() ? String("SENSOR FAULT") : String(fmt(sensor.bar(settings.calA, settings.calB))) + " " + unitName() + "  " + String(vg * DIVIDER, 2) + " V",
         W() / 2, y0, F_TEXT, sensor.fault() ? TFT_RED : TFT_GREENYELLOW, top_center, F_SMALL, W() - 4);
  textAt(supply.fitted() ? String("AUX1 ") + String(supply.volts(), 1) + " V" : String("AUX1 --"), W() / 2, y0 + 18, F_TEXT, C_LGREY, top_center, F_SMALL, W() - 4);
  push();
}
void drawHours(bool confirm) {
  canvas.fillSprite(TFT_BLACK);
  const uint32_t svc = settings.cmpSec - settings.svcSec;
  if (!confirm) {
    drawLines({ {"HOURS", F_TEXT, TFT_CYAN},
                {String("compressor total ") + hmm(settings.cmpSec), F_TEXT, TFT_WHITE, F_SMALL},
                {String("since cleaning ") + hmm(svc), F_TEXT, filtersDue() ? TFT_ORANGE : TFT_WHITE, F_SMALL},
                {DEFLATE ? String("valve ") + hmm(settings.ventSec) : "", F_TEXT, TFT_WHITE} }, 0, H() - 18);
    drawHint("press: back  hold: filters cleaned");
  } else {
    drawLines({ {"FILTERS CLEANED?", F_TEXT, TFT_ORANGE},
                {String("since service ") + hmm(svc), F_TEXT, TFT_WHITE, F_SMALL} }, 0, H() - 18);
    drawHint("hold 2 s: reset service  press: back");
  }
  push();
}

void drawSensor() {
  canvas.fillSprite(TFT_BLACK);
  const float vg = sensor.vG1(), vs = vg * DIVIDER;
  drawLines({ {"SENSOR CHECK", F_TEXT, TFT_CYAN},
              {"sensor " + String(vs, 2) + " V", F_MID, TFT_WHITE},
              {SENSOR_PIN_NAME " " + String(vg, 2) + " V", F_MID, TFT_GREENYELLOW},
              {supply.fitted() ? "AUX1 " + String(supply.volts(), 1) + " V" + (supply.engineOn() ? "  ENGINE ON" : "  ENGINE OFF") : String(""), F_TEXT, C_LGREY},
              {sensor.fault() ? "OUT OF RANGE" : String(fmt(sensor.bar(settings.calA, settings.calB))) + " " + unitName() + "  (raw " + String(sensor.rawBar(), 2) + ")", F_TEXT, sensor.fault() ? TFT_RED : TFT_WHITE},
              }, 0, H() - 18);
  drawHint("press: back");
  push();
}
// ---------- boot splash: VOLKOV AIR, a generic SUV drives over bumps (1.6 s, a press skips it) ----------
// bumpy track: two humps and a dip at fixed fractions of the width w, heights and widths scale with the car (q)
float splashGround(float x, float g, float w, float q) {
  const float u = x / w * 240, f = w / 240;
  return g - q * (12 * expf(-sq((u - 95) * f / (10 * q))) + 9 * expf(-sq((u - 165) * f / (8 * q))) - 3 * expf(-sq((u - 200) * f / (6 * q))));
}
void splash() {
  const int w = W(), h = H();
  const bool port = h > w;
  const float SS = port ? 2.6f : 1.8f, q = SS / 1.4f;           // px per unit: the car fills the space under the title
  const float g = h - (port ? 46 : 21);
  const uint16_t grey = canvas.color565(110, 110, 110);
  constexpr int N = 40, FRAME_MS = 40;
  for (int i = 0; i < N; i++) {
    const uint32_t t0 = millis();
    if (digitalRead(PIN_BTN) == LOW) break;
    const float t = i / float(N - 1), fade = min(1.0f, t * 2.2f);
    canvas.fillSprite(TFT_BLACK);
    for (int x = 0; x < w - 1; x++) canvas.drawLine(x, splashGround(x, g, w, q), x + 1, splashGround(x + 1, g, w, q), grey);
    for (int x = 4; x < w; x += 11) canvas.drawPixel(x, splashGround(x, g, w, q) + 4, grey);
    // SUV: both wheels on the track, the body pitched along the line through them
    const float wb = SUV_WB * SS, xw = -40 * q + t * (w + 20 + 40 * q);
    const float yr = splashGround(xw, g, w, q), yf = splashGround(xw + wb, g, w, q), a = atan2f(yf - yr, wb);
    drawSuv(xw, yr, SS, a * RAD_TO_DEG);
    // title fades in
    const uint8_t v = 255 * fade;
    const uint16_t white = canvas.color565(v, v, v), amb = canvas.color565(255 * fade, 165 * fade, 0);
    canvas.setFont(&fonts::FreeSansBold18pt7b); canvas.setTextDatum(top_left);
    if (port) {
      textAt("VOLKOV", w / 2, 40, &fonts::FreeSansBold18pt7b, white, top_center);
      textAt("AIR", w / 2, 80, &fonts::FreeSansBold18pt7b, amb, top_center);
    } else {
      const int tw = canvas.textWidth("VOLKOV "), aw = canvas.textWidth("AIR"), x0 = (w - tw - aw) / 2;
      textAt("VOLKOV ", x0, 30, &fonts::FreeSansBold18pt7b, white, top_left);
      textAt("AIR", x0 + tw, 30, &fonts::FreeSansBold18pt7b, amb, top_left);
    }
    push();
    while (millis() - t0 < FRAME_MS) delay(1);
  }
  while (digitalRead(PIN_BTN) == LOW) delay(5);   // a press that skipped the splash does not reach the main screen
}
