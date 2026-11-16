// Wrangler Onboard Air - M5Stack DinMeter controller for the ARB Twin compressor
// Stage 1: K1 (G2) switches the compressor relay K2. Measuring pauses = compressor off.
//
// Controls
//   turn            set target (0.1 bar steps, max 3.0 bar)
//   press           start / stop
//   long press      menu: CALIBRATE, UNITS, RESET LEARNING, UPDATE, VERSION, EXIT
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

enum class Screen { MAIN, MENU, CAL, OTA, VERSION };
Screen screen = Screen::MAIN;
int menuPos = 0;
const char* MENU[] = {"CALIBRATE", "UNITS", "RESET LEARNING", "UPDATE", "VERSION", "EXIT"};
constexpr int MENU_N = sizeof(MENU) / sizeof(MENU[0]);

// ---------- helpers ----------
float toUnit(float bar) { return settings.psi ? bar * 14.5038f : bar; }
const char* unitName() { return settings.psi ? "psi" : "bar"; }
String fmt(float bar) { return String(toUnit(bar), settings.psi ? 0 : 1); }

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
  canvas.setTextColor(sensor.fault() ? TFT_RED : TFT_GREENYELLOW);
  canvas.setTextDatum(middle_center);
  canvas.setFont(&fonts::Font7);
  canvas.setTextSize(1);
  canvas.drawString(sensor.fault() ? "--.-" : fmt(max(0.0f, shown)), 120, 52);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(TFT_LIGHTGREY);
  canvas.drawString(String(est ? "~ " : "") + unitName(), 120, 92);

  String line; uint16_t col = TFT_WHITE;
  switch (st) {
    case PState::IDLE:      line = "SET " + fmt(settings.target) + "  press: start"; break;
    case PState::SETTLE:
    case PState::MEASURE:   line = "MEASURING -> " + fmt(settings.target); col = TFT_CYAN; break;
    case PState::CAL_PULSE: line = "LEARNING PULSE -> " + fmt(settings.target); col = TFT_ORANGE; break;
    case PState::MAIN:      line = "PUMPING -> " + fmt(settings.target); col = TFT_ORANGE; break;
    case PState::TOPUP:     line = "TOP-UP -> " + fmt(settings.target); col = TFT_ORANGE; break;
    case PState::DONE:      line = "DONE " + fmt(pump.staticBar()) + "  press: ok"; col = TFT_GREEN; break;
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
  const int first = constrain(menuPos - 2, 0, MENU_N - 5);
  for (int k = 0; k < 5 && first + k < MENU_N; k++) {
    const int i = first + k;
    const int y = 14 + k * 26;
    if (i == menuPos) canvas.fillRoundRect(4, y - 11, 232, 22, 4, TFT_DARKGREEN);
    canvas.setTextColor(TFT_WHITE);
    String t = MENU[i];
    if (i == 1) t += String(": ") + unitName();
    canvas.drawString(t, 12, y);
  }
  canvas.pushSprite(0, 0);
}

void drawCal() {
  canvas.fillSprite(TFT_BLACK);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(TFT_WHITE);
  const int i = cal.index();
  switch (cal.state()) {
    case CState::ZERO_WAIT:
      canvas.drawString("CAL 1/4: hose open to air", 120, 40);
      canvas.drawString("press when ready", 120, 80); break;
    case CState::PUMPING:
      canvas.drawString("CAL " + String(i + 1) + "/4: pumping to ~" + String(cal.points[i], 0), 120, 50);
      canvas.drawString("press: abort", 120, 90); break;
    case CState::SETTLING:
      canvas.drawString("CAL " + String(i + 1) + "/4: settling...", 120, 60); break;
    case CState::ENTER:
      canvas.drawString("Gauge shows? turn + press", 120, 25);
      canvas.setFont(&fonts::Font7);
      canvas.setTextColor(TFT_GREENYELLOW);
      canvas.drawString(String(cal.entry(), 2), 120, 80); break;
    case CState::DONE:
      canvas.setTextColor(TFT_GREEN);
      canvas.drawString("CALIBRATION SAVED", 120, 55);
      canvas.drawString("a=" + String(settings.calA, 3) + " b=" + String(settings.calB, 3), 120, 90); break;
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
  canvas.drawString("Wrangler Air " FW_VERSION, 120, 30);
  canvas.drawString("offset " + String(settings.offset, 2) + " bar", 120, 60);
  canvas.drawString("rate " + String(settings.rate, 3) + " bar/s", 120, 84);
  canvas.drawString(settings.learned ? "learned" : "not learned yet", 120, 108);
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
          case 0: cal.start(); screen = Screen::CAL; break;
          case 1: settings.psi = !settings.psi; settings.save(); break;
          case 2: settings.resetLearning(); beep(); break;
          case 3: ota.start(); screen = Screen::OTA; break;
          case 4: screen = Screen::VERSION; break;
          default: screen = Screen::MAIN; break;
        }
      }
      if (screen == Screen::MENU) drawMenu();
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
