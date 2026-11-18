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
//                   or DEFLATE): "PRESS AGAIN: START" with a 3 s bar (INFLATE also "ENGINE ON?", a reminder only); a second press within 3 s starts. A turn, a
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
#include <algorithm>

Settings settings;
Sensor sensor;
Pump pump;
Calibration cal;
Ota ota;
Trace trace;   // pressure trace of the current run, written to /runs at its end
M5Canvas canvas(&M5.Display);

enum IdleDir { D_UP, D_DOWN, D_OK, D_ABOVE, D_DEAD };   // what a start would do from the idle screen
enum class Screen { MAIN, MENU, EDIT, CAL, OTA, VERSION, SENSOR, CUSTOM, LEARN_PICK, LEARN_CONF, LEARN_MSG, HOURS, HOURS_CONF, OUT_TEST };
Screen screen = Screen::MAIN;
int menuPos = 0;
enum MenuItem { M_CAL, M_LEARN, M_SENSOR, M_OUTPUT, M_UNITS, M_TYRE, M_OFFSET, M_SETTLE, M_SCREEN, M_RESET, M_UPDATE, M_VERSION, M_HOURS, M_EXIT };
const char* MENU[] = {"CALIBRATE", "LEARN", "SENSOR", "OUTPUT TEST", "UNITS", "TYRE", "OFFSET", "SETTLE", "SCREEN", "RESET LEARNING", "WIFI: UPDATE + LOG", "VERSION", "HOURS", "EXIT"};
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

constexpr uint32_t DONE_BEEP_MS = 1500;   // long beep when a run reaches its target

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

// ---------- screen graphics (spec: docs/screen/screen_mockup.py, M5GFX primitives only) ----------
uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return canvas.color565(r, g, b); }
// icon colours: with iconDim (idle, no air) everything is drawn in greys, half the brightness
bool iconDim = false;
uint16_t ic(uint8_t r, uint8_t g, uint8_t b) {
  if (!iconDim) return rgb(r, g, b);
  const uint8_t v = (uint8_t)((0.3f * r + 0.59f * g + 0.11f * b) / 2); return rgb(v, v, v);
}
#define C_GREY  ic(150, 150, 150)
#define C_LGREY ic(200, 200, 200)
#define C_DGREY ic(90, 90, 90)
#define C_SAND  ic(222, 184, 120)
#define C_ROCK  ic(160, 160, 170)

// line w px thick
void wline(float x0, float y0, float x1, float y1, int w, uint16_t c) {
  if (w <= 1) canvas.drawLine(lroundf(x0), lroundf(y0), lroundf(x1), lroundf(y1), c);
  else canvas.drawWideLine(lroundf(x0), lroundf(y0), lroundf(x1), lroundf(y1), w / 2.0f, c);
}
// convex polygon, fan of triangles
void fillConvex(const float (*p)[2], int n, uint16_t c) {
  for (int i = 1; i + 1 < n; i++)
    canvas.fillTriangle(lroundf(p[0][0]), lroundf(p[0][1]), lroundf(p[i][0]), lroundf(p[i][1]), lroundf(p[i + 1][0]), lroundf(p[i + 1][1]), c);
}

// filled boxy SUV side view, 40 x 22 units, (x, y) = rear wheel ground contact, s = px per unit, ang in degrees (negative = nose up)
void drawSuv(float x, float y, float s, float ang = 0) {
  const float a = ang * DEG_TO_RAD, ca = cosf(a), sa = sinf(a);
  auto R = [&](float px, float py, float* o) { o[0] = x + s * (px * ca + py * sa); o[1] = y - s * (-px * sa + py * ca); };
  auto poly = [&](const float (*u)[2], int n, uint16_t c) {
    float p[8][2]; for (int i = 0; i < n; i++) R(u[i][0], u[i][1], p[i]); fillConvex(p, n, c);
  };
  static const float lower[][2] = { {-4,3}, {36,3}, {36,11}, {31,12}, {2,12}, {-4,12} };   // body, split into convex parts
  static const float cabin[][2] = { {2,12}, {31,12}, {27,20}, {4,20} };
  static const float win1[][2]  = { {5,13}, {14,13}, {14,18}, {6,18} };
  static const float win2[][2]  = { {16,13}, {26,13}, {24.5f,18}, {16,18} };
  static const float spare[][2] = { {-6,6}, {-4,6}, {-4,11}, {-6,11} };
  poly(lower, 6, ic(255, 255, 255)); poly(cabin, 4, ic(255, 255, 255));
  poly(win1, 4, TFT_BLACK); poly(win2, 4, TFT_BLACK); poly(spare, 4, ic(255, 255, 255));
  const float r = 5.2f * s; const int rim = max(1, (int)(s * 1.6f));
  for (float wx : {4.0f, 28.0f}) {
    float c[2]; R(wx, 4.5f, c);
    const int cx = lroundf(c[0]), cy = lroundf(c[1]);
    canvas.fillCircle(cx, cy, lroundf(r), ic(255, 255, 255));
    canvas.fillCircle(cx, cy, max(1L, lroundf(r) - rim), TFT_BLACK);
    canvas.fillCircle(cx, cy, max(1L, lroundf(r * 0.35f)), ic(255, 255, 255));
  }
}

// gravel pebbles: u, depth below ground, radius (units); fixed list = the mock-up's random.seed(3)
static const float PEBBLES[][3] = {
  {12.4f,3.7f,1.1f}, {47.6f,3.4f,1.4f}, {3.4f,1.6f,1.1f}, {13.5f,2.4f,1.4f}, {24.5f,4.8f,1.1f}, {20.7f,4.9f,0.8f}, {33.0f,5.0f,1.4f},
  {20.3f,1.6f,0.8f}, {8.3f,5.3f,0.8f}, {15.7f,1.6f,1.1f}, {24.6f,4.4f,1.1f}, {37.1f,5.2f,1.1f}, {37.9f,3.8f,0.8f}, {45.7f,1.9f,0.8f},
  {25.7f,2.5f,1.4f}, {22.7f,4.0f,1.1f}, {21.9f,4.8f,1.4f}, {18.2f,3.8f,1.4f}, {12.1f,2.8f,0.8f}, {44.5f,5.5f,1.4f}, {36.2f,4.3f,1.1f},
  {50.2f,5.1f,1.4f}, {5.4f,4.1f,1.4f}, {43.2f,3.8f,1.1f}, {6.5f,3.4f,1.4f}, {51.5f,1.9f,0.8f} };

// scenery behind the icon (spec: docs/screen/sim2.py scenery(), fir(), cactus(), sun(), peak()), drawn before terrain and SUV
#define C_TREE  ic(40, 125, 60)
#define C_CACT  ic(70, 160, 80)
#define C_SUN   ic(255, 200, 40)
#define C_MOUNT ic(105, 118, 140)
#define C_CONE  ic(255, 120, 0)
#define C_TOWER ic(85, 95, 115)
#define C_LIT   ic(255, 220, 120)
#define C_SEA   ic(40, 110, 190)
#define C_FOAM  ic(170, 210, 240)
// PIL-style rectangle with inclusive corners
void rectI(float x0, float y0, float x1, float y1, uint16_t c) {
  const int a = lroundf(x0), b = lroundf(y0); canvas.fillRect(a, b, lroundf(x1) - a + 1, lroundf(y1) - b + 1, c);
}
void rrectI(float x0, float y0, float x1, float y1, float r, uint16_t c) {
  const int a = lroundf(x0), b = lroundf(y0); canvas.fillRoundRect(a, b, lroundf(x1) - a + 1, lroundf(y1) - b + 1, lroundf(r), c);
}
void drawScenery(const char* n, float ox, float g, float s) {
  auto X = [&](float u) { return ox + u * s; };
  auto fir = [&](float u, float hgt) {
    rectI(X(u) - 0.8f * s, g - 3 * s, X(u) + 0.8f * s, g, ic(110, 80, 50));
    for (int k = 0; k < 3; k++) {
      const float y0 = g - 3 * s - k * hgt * 0.25f * s, wd = (4.5f - k * 1.2f) * s;
      const float t[][2] = { {X(u) - wd, y0}, {X(u) + wd, y0}, {X(u), y0 - hgt * 0.42f * s} };
      fillConvex(t, 3, C_TREE);
    }
  };
  auto cactus = [&](float u, float hgt) {
    const float c = 1.3f * s;
    rrectI(X(u) - c, g - hgt * s, X(u) + c, g, c, C_CACT);
    rrectI(X(u) - 4 * s, g - hgt * 0.75f * s, X(u) - 4 * s + 2 * c, g - hgt * 0.4f * s, c, C_CACT);
    rectI(X(u) - 4 * s, g - hgt * 0.45f * s, X(u), g - hgt * 0.45f * s + 2 * c, C_CACT);
    rrectI(X(u) + 4 * s - 2 * c, g - hgt * 0.85f * s, X(u) + 4 * s, g - hgt * 0.5f * s, c, C_CACT);
    rectI(X(u), g - hgt * 0.55f * s, X(u) + 4 * s, g - hgt * 0.55f * s + 2 * c, C_CACT);
  };
  auto sun = [&](float u, float v, float r = 3.5f) {
    const float cx = X(u), cy = g - v * s;
    for (int k = 0; k < 8; k++) {
      const float a = k * PI / 4;
      wline(cx + cosf(a) * r * 1.4f * s, cy + sinf(a) * r * 1.4f * s, cx + cosf(a) * r * 2 * s, cy + sinf(a) * r * 2 * s, max(1, (int)s), C_SUN);
    }
    canvas.fillCircle(lroundf(cx), lroundf(cy), lroundf(r * s), C_SUN);
  };
  auto peak = [&](float u0, float u1, float top) {
    const float um = (u0 + u1) / 2, f = 0.28f;
    const float m[][2] = { {X(u0), g}, {X(um), g - top * s}, {X(u1), g} };
    fillConvex(m, 3, C_MOUNT);
    const float sn[][2] = { {X(um - (um - u0) * f), g - top * s * (1 - f)}, {X(um), g - top * s}, {X(um + (u1 - um) * f), g - top * s * (1 - f)} };
    fillConvex(sn, 3, ic(255, 255, 255));
  };
  auto skyline = [&]() {                                     // city towers behind the road, lit windows, antenna
    static const int8_t towers[][3] = { {14, 7, 30}, {22, 6, 22}, {29, 8, 34}, {38, 6, 26}, {45, 8, 31} };   // u0, width, height
    for (auto& t : towers) {
      const int u0 = t[0], wd = t[1], ht = t[2];
      rectI(X(u0), g - ht * s, X(u0 + wd), g, C_TOWER);
      for (int wy = 4; wy < ht - 2; wy += 4)
        for (float wx : {1.5f, wd - 2.5f})
          if ((wy * 7 + u0) % 3) rectI(X(u0 + wx), g - (ht - wy) * s, X(u0 + wx + 1), g - (ht - wy) * s + s, C_LIT);
    }
    rectI(X(32.5f), g - 38 * s, X(33.2f), g - 34 * s, C_TOWER);
  };
  auto sea = [&]() {                                         // sea band behind the beach with wave crests
    const float top = g - 15 * s;
    rectI(X(0), top, X(52), g - 1 * s, C_SEA);
    for (int u = 2; u < 52; u += 9) {
      const float cx = (X(u) + X(u + 5)) / 2, cy = top + 0.25f * s, rx = 2.5f * s, ry = 1.75f * s;
      for (int a = 200; a < 340; a += 10)
        wline(cx + rx * cosf(a * DEG_TO_RAD), cy + ry * sinf(a * DEG_TO_RAD), cx + rx * cosf((a + 10) * DEG_TO_RAD), cy + ry * sinf((a + 10) * DEG_TO_RAD), max(1, (int)s), C_FOAM);
    }
  };
  if (!strcmp(n, "HIGHWAY"))         skyline();
  else if (!strcmp(n, "ROUGH ROAD")) {                       // traffic cone with a white band
    const float x = X(46);
    const float cone[][2] = { {x - 3.5f * s, g}, {x + 3.5f * s, g}, {x + 1 * s, g - 11 * s}, {x - 1 * s, g - 11 * s} };
    fillConvex(cone, 4, C_CONE);
    rectI(x - 2.2f * s, g - 6 * s, x + 2.2f * s, g - 4.5f * s, ic(255, 255, 255));
    rectI(x - 4.5f * s, g - 1 * s, x + 4.5f * s, g, C_CONE);
  }
  else if (!strcmp(n, "GRAVEL"))     { fir(48, 26); fir(3, 18); }
  else if (!strcmp(n, "TRAIL"))      { fir(3, 22); fir(47, 28); fir(41, 20); }
  else if (!strcmp(n, "ROCK"))       { peak(-6, 22, 30); peak(38, 60, 24); }
  else if (!strcmp(n, "SAND"))       { sun(40, 25); cactus(49, 19); }
  else if (!strcmp(n, "RECOVERY"))   { sea(); sun(30, 26, 3); }
}

// preset icon, box w x 0.62 w, (ox, oy) = top left
void drawPresetIcon(int preset, float ox, float oy, float w) {
  const float s = w / 52.0f, h = w * 0.62f, g = oy + h - 6 * s;
  const int lw = max(2, (int)(s * 1.4f));
  auto X = [&](float u) { return ox + u * s; };
  auto ground = [&]() { wline(ox, g, ox + w, g, lw, C_GREY); };
  auto fillUnder = [&](float (*top)(float), uint16_t c) {        // area between a curve (units) and g + 6 units
    for (int px = 0; px <= (int)w; px++) { const float y = g + top(px / s) * s; canvas.drawFastVLine(lroundf(ox + px), lroundf(y), lroundf(g + 6 * s - y) + 1, c); }
  };
  const char* n = PRESETS[preset].name;
  drawScenery(n, ox, g, s);
  if (!strcmp(n, "HIGHWAY")) {
    ground();
    for (int i = 0; i < 52; i += 10) wline(X(i), g + 4 * s, X(i + 5), g + 4 * s, lw, ic(255, 255, 255));
    const int yy[] = {10, 14, 18};
    for (int k = 0; k < 3; k++) wline(X(0), g - yy[k] * s, X(5 - k), g - yy[k] * s, lw, C_LGREY);
    drawSuv(X(12), g, s);
  } else if (!strcmp(n, "ROUGH ROAD")) {
    const float p[][2] = { {0,0}, {20,0}, {23,5}, {31,5}, {34,0}, {52,0} };
    for (int i = 0; i < 5; i++) wline(X(p[i][0]), g + p[i][1] * s, X(p[i + 1][0]), g + p[i + 1][1] * s, lw, C_GREY);
    const int tw = max(1, lw - 1);
    const float c1[][2] = { {40,1}, {42,3}, {45,2}, {47,4} }, c2[][2] = { {6,1}, {8,3}, {11,2} };   // cracks
    for (int i = 0; i < 3; i++) wline(X(c1[i][0]), g + c1[i][1] * s, X(c1[i + 1][0]), g + c1[i + 1][1] * s, tw, C_GREY);
    for (int i = 0; i < 2; i++) wline(X(c2[i][0]), g + c2[i][1] * s, X(c2[i + 1][0]), g + c2[i + 1][1] * s, tw, C_GREY);
    drawSuv(X(9), g, s, -3);
  } else if (!strcmp(n, "GRAVEL")) {
    ground();
    for (auto& q : PEBBLES) canvas.fillCircle(lroundf(X(q[0])), lroundf(g + q[1] * s), max(1L, lroundf(q[2] * s)), C_LGREY);
    drawSuv(X(9), g, s);
  } else if (!strcmp(n, "TRAIL")) {
    for (int u = 0; u < 52; u++) wline(X(u), g + 2.2f * s * sinf(u / 4.5f), X(u + 1), g + 2.2f * s * sinf((u + 1) / 4.5f), lw, C_GREY);
    for (int u : {6, 18, 30, 42}) wline(X(u), g + 4.5f * s, X(u + 3), g + 4.5f * s, lw, C_DGREY);   // tyre tracks
    drawSuv(X(9), g - 1.5f * s, s, -5);
  } else if (!strcmp(n, "ROCK")) {
    ground();
    const float rock[][2] = { {X(24), g}, {X(28), g - 8 * s}, {X(34), g - 13 * s}, {X(41), g - 12 * s}, {X(47), g - 6 * s}, {X(50), g} };
    fillConvex(rock, 6, C_ROCK);
    const float small[][2] = { {X(3), g}, {X(6), g - 3 * s}, {X(10), g} };
    fillConvex(small, 3, C_ROCK);
    drawSuv(X(8), g - 0.5f * s, s, -22);
  } else if (!strcmp(n, "SAND")) {
    fillUnder([](float u) { return 2 - 11 * expf(-powf((u - 40) / 14.0f, 2)); }, C_SAND);
    drawSuv(X(8), g + 1.5f * s, s, -12);
  } else if (!strcmp(n, "RECOVERY")) {
    drawSuv(X(8), g + 3 * s, s);
    fillUnder([](float u) { return -1 + 1.8f * sinf(u / 6.0f); }, C_SAND);        // wheels half in the sand
    const float ax = X(47);
    wline(ax, g - 20 * s, ax, g - 9 * s, lw + 1, ic(255, 165, 0));
    canvas.fillTriangle(lroundf(ax - 4 * s), lroundf(g - 10 * s), lroundf(ax + 4 * s), lroundf(g - 10 * s), lroundf(ax), lroundf(g - 4 * s), ic(255, 165, 0));
  } else {                                                      // CUSTOM
    ground();
    drawSuv(X(6), g, s);
    const float cx = X(46), cy = g - 17 * s, r = 5.5f * s;
    for (int k = 0; k < lw; k++) canvas.drawCircle(lroundf(cx), lroundf(cy), lroundf(r) - k, ic(0, 255, 255));
    wline(cx - r * 0.55f, cy, cx + r * 0.55f, cy, lw, ic(0, 255, 255));
    wline(cx, cy - r * 0.55f, cx, cy + r * 0.55f, lw, ic(0, 255, 255));
  }
}

void textAt(const String& t, int x, int y, Font f, uint16_t c, textdatum_t d, Font alt = nullptr, int maxW = 0) {
  canvas.setFont(f);
  if (alt && maxW && canvas.textWidth(t) > maxW) canvas.setFont(alt);
  canvas.setTextColor(c); canvas.setTextDatum(d); canvas.drawString(t, x, y);
}
// bottom hint: what the knob and the button do right now ("hold" = the 2 s long press). Font2, wraps to 2 lines
// (portrait); if it still does not fit, the 8 px Font0. Returns the top of the hint; hintTop keeps it for the hold bar.
int hintTop = 0;
int drawHint(const String& t, uint16_t c = 0, int bottom = -1) {
  if (!c) c = C_GREY;
  if (bottom < 0) bottom = H();
  // wrap at the double spaces between the "control: action" groups, so a group is never split
  auto wrapGroups = [&](String* out) {
    int n = 0; String cur; int i = 0;
    while (i <= (int)t.length() && n < 3) {
      int j = t.indexOf("  ", i); if (j < 0) j = t.length();
      const String g = t.substring(i, j), cand = cur.length() ? cur + "  " + g : g;
      if (cur.length() && canvas.textWidth(cand) > W() - 4) { out[n++] = cur; cur = g; } else cur = cand;
      i = j + 2;
    }
    if (cur.length() && n < 3) out[n++] = cur;
    return n;
  };
  String parts[3]; Font f = &fonts::Font2;
  canvas.setFont(f);
  int n = wrapGroups(parts);
  if (n > 2) { f = &fonts::Font0; canvas.setFont(f); n = wrapGroups(parts); }
  const int lh = f == &fonts::Font0 ? 9 : 16;
  const int top = bottom - n * lh;
  for (int q = 0; q < n; q++) textAt(parts[q], W() / 2, top + q * lh, f, c, top_center);
  hintTop = top;
  return top;
}

// big value in Font7 digits; "~" (estimate) in a text font in front of it. align: 0 = centre at x, 1 = right edge at x
void bigValue(const String& v, bool tilde, int x, int y, uint16_t c, int align) {
  canvas.setFont(F_DIGIT); const int wv = canvas.textWidth(v);
  canvas.setFont(F_LARGE); const int wt = tilde ? canvas.textWidth("~") + 2 : 0;
  const int left = align ? x - wv - wt : x - (wv + wt) / 2;
  if (tilde) textAt("~", left, y + 24, F_LARGE, c, middle_left);
  textAt(v, left + wt, y, F_DIGIT, c, top_left);
}

// "no air" state (hoses not connected): entered below NOAIR_IN or on a sensor fault, left only after the reading
// stayed above NOAIR_OUT for NOAIR_HOLD_MS, so the screen does not flicker. Starting a run is blocked meanwhile.
constexpr float NOAIR_IN = 0.05f, NOAIR_OUT = 0.15f;
constexpr uint32_t NOAIR_HOLD_MS = 1000;
bool noAir = true;
void updateNoAir() {
  static uint32_t okSince = 0;
  const float now = sensor.bar(settings.calA, settings.calB);
  if (sensor.fault() || now < NOAIR_IN) { noAir = true; okSince = 0; }
  else if (noAir) {
    if (now <= NOAIR_OUT) okSince = 0;
    else if (!okSince) okSince = millis();
    else if (millis() - okSince >= NOAIR_HOLD_MS) noAir = false;
  }
}

// what a start would do from the idle screen (IdleDir is declared at the top)
IdleDir idleDir(float bar) {
  const float now = sensor.bar(settings.calA, settings.calB);
  if (noAir) return D_DEAD;                                            // hoses not connected / no air
  if (fabsf(now - bar) <= START_BAND) return D_OK;
  if (now < bar) return D_UP;
  return DEFLATE ? D_DOWN : D_ABOVE;                                   // inflate-only builds do nothing above the target
}
// two-press start: the first press arms, a second one within ARM_MS starts; knob, long press or timeout disarm
constexpr uint32_t ARM_MS = 3000;
bool armed = false; uint32_t armT = 0;
void arm() {   // only when a start would inflate or deflate
  const IdleDir d = idleDir(settings.target);
  if (d == D_UP || d == D_DOWN) { armed = true; armT = millis(); } else beep();
}
void disarm(const char* why) { if (!armed) return; armed = false; wlog::ev("ARM_CANCEL", wlog::kv("reason", String(why))); }
// hold-for-menu bar along the bottom edge: from 0.3 s of holding, full at the LONG press (2.0 s)
constexpr uint32_t HOLD_SHOW_MS = 300;
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
  const String name = PRESETS[p].name, nowT = "actual " + fmt(max(0.0f, now));
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
    if (edit) { drawHint("turn: 0.1  press: OK  hold: cancel"); return; }
    if (armed && !dead) {   // ARMED: white hint, a bar running out over ARM_MS under it
      drawHint("PRESS AGAIN: START  turn: cancel", TFT_WHITE, H() - 4);
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
    bigValue(fmt(bar), false, W() - 8, 2, numc, 1);
    textAt(unitName(), W() - 8, 56, F_TEXT, unitc, top_right);
    if (!edit && filtersDue()) {   // small, not blocking: left of the unit, right of the icon
      canvas.setFont(F_TEXT); const int uw = canvas.textWidth(unitName());
      textAt("CLEAN FILTERS", W() - 8 - uw - 6, 58, &fonts::Font2, dead ? dim : TFT_ORANGE, top_right);
    }
    textAt(name + (low && !edit ? "  LOW" : ""), W() / 2, 72, F_MID, namec, top_center, F_TEXT, W() - 8);
    if (!edit) {
      canvas.fillRect(10, 94, W() - 20, 2, dead ? dim : dc);
      if (dead) textAt("CONNECT SYSTEM", W() / 2, 99, F_TEXT, C_LGREY, top_center);
      else if (armed && dir == D_UP) textAt("ENGINE ON?", W() / 2, 99, F_TEXT, TFT_ORANGE, top_center);   // compressor ahead: reminder only
      else dirLine(99, false);
    }
    idleHint();
  } else {
    drawPresetIcon(p, 6, !strcmp(PRESETS[p].name, "ROCK") ? 14 : 8, 123);   // tilted ROCK SUV would touch the top edge
    iconDim = false;
    textAt(name, W() / 2, 84, F_MID, namec, top_center, F_TEXT, W() - 6);
    bigValue(fmt(bar), false, W() / 2, 100, numc, 0);
    textAt(String(unitName()) + (low && !edit ? "  LOW" : ""), W() / 2, 150, F_TEXT, low && !edit && !dead ? TFT_ORANGE : unitc, top_center);
    if (!edit) {
      canvas.fillRect(10, 169, W() - 20, 2, dead ? dim : dc);
      if (dead) {
        textAt("CONNECT", W() / 2, 173, F_TEXT, C_LGREY, top_center);
        textAt("SYSTEM", W() / 2, 190, F_TEXT, C_LGREY, top_center);
      } else if (armed && dir == D_UP) {   // compressor ahead: reminder only, no hardware check
        dirLine(173, true);
        canvas.fillRect(0, 189, W(), 17, TFT_BLACK);
        textAt("ENGINE ON?", W() / 2, 190, F_TEXT, TFT_ORANGE, top_center);
      } else dirLine(173, true);
    }
    idleHint();
  }
  if (!edit) drawHoldBar();
  canvas.pushSprite(0, 0);
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
                {"before " + fmt(pump.testBefore()) + "  after " + fmt(pump.staticBar()), F_SMALL, C_LGREY} }, 0, H() - 18);
  drawHint("press: back");
  canvas.pushSprite(0, 0);
}

void drawMain() {
  const PState st = pump.state();
  if (st == PState::IDLE) { drawIdle(false); return; }
  const bool test = pump.testMode() != Mode::NONE;
  if (test && st == PState::DONE) { drawLearnResult(); return; }
  // background by phase: compressor pumping = dark amber, valve venting = dark blue, pauses and results = black
  canvas.fillSprite(pump.compressorOn() ? rgb(90, 38, 0) : pump.valveOpen() ? rgb(0, 40, 90) : TFT_BLACK);
  const bool est = pump.estimating();
  const float cur = est ? pump.estimate() : (pump.mode() == Mode::NONE ? sensor.bar(settings.calA, settings.calB) : pump.staticBar());
  const String tm = phaseTimer(pump.phaseMs(), pump.phaseLenMs());
  const String pause = String(pump.pauseLeftMs() / 1000.0f, 1) + "s";
  String line; uint16_t col = TFT_WHITE;
  switch (st) {
    case PState::SETTLE:    line = "SETTLING " + pause; col = TFT_CYAN; break;
    case PState::MEASURE:   line = "MEASURING " + pause; col = TFT_CYAN; break;
    case PState::CAL_PULSE: line = (test ? "LEARN " : "LEARNING ") + tm; col = TFT_ORANGE; break;
    case PState::MAIN:      line = "PUMP " + tm; col = TFT_ORANGE; break;
    case PState::TOPUP:     line = "TOP-UP " + tm; col = TFT_ORANGE; break;
    case PState::VENT:      line = (test ? "LEARN " : "VENT ") + tm; col = TFT_SKYBLUE; break;
    case PState::DONE:
      line = pump.inBand() ? "OK, WITHIN 0.1" : "DONE " + fmt(pump.staticBar()) + "  " + mmss(pump.runMs()); col = TFT_GREEN; break;
    case PState::ERROR:
      col = TFT_RED;
      switch (pump.error()) {
        case PErr::SENSOR:  line = "ERR SENSOR"; break;
        case PErr::OVER:    line = "ERR OVER 3.0 BAR"; break;
        case PErr::LEAK:    line = "ERR NO RISE: LEAK?"; break;
        case PErr::TIMEOUT: line = "ERR TIMEOUT"; break;
        case PErr::LOW_P:   line = "PUMP UP TO " + fmt(LEARN_DEF_MIN_BAR) + " FIRST"; break;
        default:            line = "ERR"; break;
      }
      break;
    default: break;
  }
  const String mode = pump.mode() == Mode::DEF ? "DEF " : pump.mode() == Mode::INF ? "INF " : "";
  const String counts = "starts " + String(pump.starts()) + (DEFLATE ? "  vents " + String(pump.vents()) : "");
  const String val = sensor.fault() ? "--.-" : fmt(max(0.0f, cur));
  const uint16_t valc = sensor.fault() ? TFT_RED : TFT_GREENYELLOW;
  int y, x0, x1;
  if (!portrait()) {
    textAt((DEFLATE ? mode : "") + mmss(pump.runMs()) + "  " + counts, 4, 2, F_SMALL, C_LGREY, top_left);
    drawPresetIcon(settings.preset, 6, 22, 64);
    bigValue(val, est, W() - 10, 16, valc, 1);
    y = 72; x0 = 10; x1 = W() - 10;
  } else {
    textAt((DEFLATE ? mode : "") + mmss(pump.runMs()), 4, 2, F_SMALL, C_LGREY, top_left);
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
  drawHint(pump.busy() ? "press: STOP" : "press: back");
  if (pump.compressorOn()) canvas.fillCircle(W() - 10, 10, 6, TFT_ORANGE);
  if (pump.valveOpen())    canvas.fillCircle(W() - 10, 10, 6, TFT_SKYBLUE);
  canvas.pushSprite(0, 0);
}

String rotName() {
  switch (settings.rot) { case 0: return "PORTRAIT"; case 2: return "PORTRAIT 180"; case 3: return "WIDE 180"; default: return "WIDE"; }
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
    if (i == M_OFFSET) t += ": " + offText(offIdx());
    if (i == M_SETTLE) t += ": " + setText(setIdx());
    if (i == M_SCREEN) t += ": " + rotName();
    if (i == M_RESET && resetArm && millis() - resetArm < 3000) t = "PRESS AGAIN";
    else if (i == M_RESET && portrait()) t = "RESET LEARN";
    canvas.drawString(t, 8, y);
  }
  canvas.pushSprite(0, 0);
}

// ---------- menu LEARN ----------
constexpr int LEARN_N = DEFLATE ? 3 : 2;
int learnPos = 0; Mode learnSel = Mode::INF; String learnMsg;
Mode learnItem(int i) { if (i == 0) return Mode::INF; if (DEFLATE && i == 1) return Mode::DEF; return Mode::NONE; }
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
  canvas.pushSprite(0, 0);
}
void drawLearnConf() {
  canvas.fillSprite(TFT_BLACK);
  const bool def = learnSel == Mode::DEF;
  drawLines({ {def ? "LEARN DEFLATE:" : "LEARN INFLATE:", F_TEXT, def ? TFT_SKYBLUE : TFT_ORANGE},
              {def ? "valve " + String(LEARN_DEF_MS / 1000) + " s." : "compressor " + String(CAL_PULSE_MS / 1000) + " s.", F_TEXT, TFT_WHITE},
              {def ? "" : "ENGINE ON?", F_TEXT, TFT_ORANGE},
              {"Measure, pulse, measure.", F_SMALL, C_LGREY} }, 0, H() - 18);
  drawHint("press: start  hold: back");
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
              {hint, F_SMALL, TFT_LIGHTGREY} }, 0, H() - 18);
  drawHint("turn: change  press: save  hold: back");
  canvas.pushSprite(0, 0);
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
      drawLines({ {"CAL 1/4", F_TEXT, TFT_WHITE}, {"hose open to air", F_TEXT, TFT_WHITE} }, top, H() - 18);
      drawHint("press when ready: next  hold: abort"); break;
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
  canvas.pushSprite(0, 0);
}

void drawOta() {
  canvas.fillSprite(TFT_BLACK);
  drawLines({ {"UPDATE MODE", F_TEXT, TFT_CYAN}, {"WiFi " OTA_SSID, F_TEXT, TFT_WHITE}, {"pass " OTA_PASS, F_TEXT, TFT_WHITE},
              {"192.168.4.1  (log: /log)", F_TEXT, TFT_WHITE, F_SMALL},
              {ota.progress() ? String(ota.progress() / 1024) + " kB received" : "", F_SMALL, TFT_LIGHTGREY} }, 0, H() - 18);
  drawHint("hold: exit");
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
              {settings.learned ? "learned" : "not learned yet", F_SMALL, TFT_LIGHTGREY} }, 0, H() - 16);
  drawHint("press: back");
  canvas.pushSprite(0, 0);
}

// live sensor check for the bench: what the sensor sends, what the input pin sees (after the divider if any), and the pressure
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
      static const char* E[] = {"NONE", "SENSOR", "OVER", "LEAK", "TIMEOUT", "LOW_P"};
      const String res = st == PState::DONE ? (pump.inBand() ? "IN_BAND" : "DONE") : st == PState::ERROR ? String("ERR_") + E[(int)pump.error()] : "STOPPED";
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

// ---------- menu OUTPUT TEST ----------
constexpr int OUT_N = DEFLATE ? 3 : 2;   // COMPRESSOR, (VALVE), BACK
int outPos = 0;
void drawOutTest() {
  canvas.fillSprite(TFT_BLACK);
  drawHint("press: 1 s pulse  turn: choose  hold: back");
  canvas.setFont(portrait() ? F_SMALL : F_TEXT); canvas.setTextDatum(middle_left);
  for (int i = 0; i < OUT_N; i++) {
    const int y = 12 + i * 24;
    if (i == outPos) canvas.fillRoundRect(3, y - 11, W() - 6, 22, 4, TFT_DARKGREEN);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(i == OUT_N - 1 ? "BACK" : i == 0 ? "COMPRESSOR (K1)" : "VALVE (K4)", 8, y);
  }
  const int y0 = 8 + OUT_N * 24;
  const float vg = sensor.vG1();
  textAt(sensor.fault() ? String("SENSOR FAULT") : fmt(sensor.bar(settings.calA, settings.calB)) + " " + unitName() + "  " + String(vg * DIVIDER, 2) + " V",
         W() / 2, y0, F_TEXT, sensor.fault() ? TFT_RED : TFT_GREENYELLOW, top_center, F_SMALL, W() - 4);
  if (pump.pulsing()) textAt(pump.pulseIsValve() ? "VALVE PULSE" : "K1 PULSE", W() / 2, y0 + 18, F_TEXT, pump.pulseIsValve() ? TFT_SKYBLUE : TFT_ORANGE, top_center);
  canvas.pushSprite(0, 0);
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
    Serial.println("# commands: status | log | runs | run <id> | clear | pulse k1 | pulse valve | help");
  } else if (c == "status") {
    Serial.printf("# fw %s  preset %s  target %.2f bar  sensor %.3f bar (%.3f V)%s  state %d err %d\n", FW_VERSION,
                  PRESETS[settings.preset].name, settings.target, sensor.bar(settings.calA, settings.calB), sensor.vG1() * DIVIDER,
                  sensor.fault() ? " FAULT" : "", (int)pump.state(), (int)pump.error());
    Serial.printf("# tyre %s  rate %.4f  kTop %.4f  offset %.3f  c %.2f  tauD %.1f  learned %d/%d  cal a %.4f b %.3f\n",
                  TYRES[settings.tyre].name, settings.rate, settings.kTop, settings.offset, settings.c, settings.tauD,
                  settings.learned, settings.learnedD, settings.calA, settings.calB);
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
  } else if (c == "pulse k1" || c == "pulse valve") {
    const bool v = c.endsWith("valve");
    if (v && !DEFLATE) Serial.println("# no valve in this build");
    else if (!pump.pulse(v)) Serial.println("# blocked: a run or a pulse is active");
    else Serial.printf("# %s pulse %lu ms\n", v ? "VALVE" : "K1", (unsigned long)OUTPUT_PULSE_MS);
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
String hmm(uint32_t sec) { char b[16]; snprintf(b, sizeof b, "%lu:%02lu", (unsigned long)(sec / 3600), (unsigned long)(sec / 60 % 60)); return b; }
bool filtersDue() { return settings.svcHours() >= FILTER_SERVICE_H; }
void drawHours(bool confirm) {
  canvas.fillSprite(TFT_BLACK);
  const uint32_t svc = settings.cmpSec - settings.svcSec;
  if (!confirm) {
    drawLines({ {"HOURS", F_TEXT, TFT_CYAN},
                {"compressor " + hmm(settings.cmpSec), F_TEXT, TFT_WHITE},
                {"since filter service " + hmm(svc), F_TEXT, filtersDue() ? TFT_ORANGE : TFT_WHITE, F_SMALL},
                {DEFLATE ? "valve " + hmm(settings.ventSec) : "", F_TEXT, TFT_WHITE} }, 0, H() - 18);
    drawHint("press: back  hold: filters cleaned");
  } else {
    drawLines({ {"FILTERS CLEANED?", F_TEXT, TFT_ORANGE},
                {"since service " + hmm(svc), F_TEXT, TFT_WHITE, F_SMALL} }, 0, H() - 18);
    drawHint("hold 2 s: reset service  press: back");
  }
  canvas.pushSprite(0, 0);
}

void drawSensor() {
  canvas.fillSprite(TFT_BLACK);
  const float vg = sensor.vG1(), vs = vg * DIVIDER;
  drawLines({ {"SENSOR CHECK", F_TEXT, TFT_CYAN},
              {"sensor " + String(vs, 2) + " V", F_MID, TFT_WHITE},
              {SENSOR_PIN_NAME " " + String(vg, 2) + " V", F_MID, TFT_GREENYELLOW},
              {sensor.fault() ? "OUT OF RANGE" : fmt(sensor.bar(settings.calA, settings.calB)) + " " + unitName() + "  (raw " + String(sensor.rawBar(), 2) + ")", F_TEXT, sensor.fault() ? TFT_RED : TFT_WHITE},
              }, 0, H() - 18);
  drawHint("press: back");
  canvas.pushSprite(0, 0);
}

void applyRotation() {
  M5.Display.setRotation(settings.rot);
  canvas.deleteSprite();
  canvas.createSprite(M5.Display.width(), M5.Display.height());
}

// ---------- boot splash: VOLKOV AIR, a generic SUV drives over bumps (1.6 s, a press skips it) ----------
float splashGround(float x, float g, float k) {   // world x in landscape pixels, k = screen width / 240
  x /= k;
  return g - k * (12 * expf(-sq((x - 95) / 10)) + 9 * expf(-sq((x - 165) / 8)) - 3 * expf(-sq((x - 200) / 6)));
}
void splash() {
  const int w = W(), h = H();
  const bool port = h > w;
  const float k = w / 240.0f, g = h - 23 * (port ? 2.0f : 1.0f);
  const uint16_t grey = canvas.color565(110, 110, 110);
  constexpr int N = 40, FRAME_MS = 40;
  for (int i = 0; i < N; i++) {
    const uint32_t t0 = millis();
    if (digitalRead(PIN_BTN) == LOW) break;
    const float t = i / float(N - 1), fade = min(1.0f, t * 2.2f);
    canvas.fillSprite(TFT_BLACK);
    for (int x = 0; x < w - 1; x++) canvas.drawLine(x, splashGround(x, g, k), x + 1, splashGround(x + 1, g, k), grey);
    for (int x = 4; x < w; x += 11) canvas.drawPixel(x, splashGround(x, g, k) + 4, grey);
    // SUV: wheels sit on the track, the body follows the line through both axles
    const float xr = (-40 + t * 300) * k, wb = 26, yr = splashGround(xr, g, k) - 5, yf = splashGround(xr + wb, g, k) - 5;
    const float a = atan2f(yf - yr, wb), ca = cosf(a), sa = sinf(a);
    auto P = [&](float px, float py, int& ox, int& oy) { ox = lroundf(xr + px * ca + py * sa); oy = lroundf(yr + px * sa - py * ca); };
    const float body[][2] = {{-7, 2}, {33, 2}, {33, 10}, {27, 10}, {23, 18}, {1, 18}, {-1, 10}, {-7, 10}};
    for (int j = 0; j < 8; j++) {
      int x0, y0, x1, y1; P(body[j][0], body[j][1], x0, y0); P(body[(j + 1) % 8][0], body[(j + 1) % 8][1], x1, y1);
      canvas.drawLine(x0, y0, x1, y1, TFT_WHITE);
    }
    { int x0, y0, x1, y1; P(11, 10, x0, y0); P(11, 17, x1, y1); canvas.drawLine(x0, y0, x1, y1, TFT_WHITE); }
    for (float wx : {xr, xr + wb}) {
      const float wy = (wx == xr) ? yr : yf;
      canvas.fillCircle(wx, wy, 5, TFT_BLACK); canvas.drawCircle(wx, wy, 5, TFT_WHITE); canvas.drawPixel(wx, wy, TFT_WHITE);
    }
    // title fades in
    const uint8_t v = 255 * fade;
    const uint16_t white = canvas.color565(v, v, v), amb = canvas.color565(255 * fade, 165 * fade, 0), gv = canvas.color565(110 * fade, 110 * fade, 110 * fade);
    canvas.setFont(&fonts::FreeSansBold18pt7b); canvas.setTextDatum(top_left);
    if (port) {
      textAt("VOLKOV", w / 2, 30, &fonts::FreeSansBold18pt7b, white, top_center);
      textAt("AIR", w / 2, 70, &fonts::FreeSansBold18pt7b, amb, top_center);
      textAt(FW_VERSION, w / 2, 112, &fonts::Font2, gv, top_center);
    } else {
      const int tw = canvas.textWidth("VOLKOV "), aw = canvas.textWidth("AIR"), x0 = (w - tw - aw) / 2;
      textAt("VOLKOV ", x0, 18, &fonts::FreeSansBold18pt7b, white, top_left);
      textAt("AIR", x0 + tw, 18, &fonts::FreeSansBold18pt7b, amb, top_left);
      textAt(FW_VERSION, w / 2, 58, &fonts::Font2, gv, top_center);
    }
    canvas.pushSprite(0, 0);
    while (millis() - t0 < FRAME_MS) delay(1);
  }
  while (digitalRead(PIN_BTN) == LOW) delay(5);   // a press that skipped the splash does not reach the main screen
}

// ---------- setup / loop ----------
void setup() {
  // outputs first: compressor and K4 must never twitch at boot
  pinMode(PIN_K1, OUTPUT); digitalWrite(PIN_K1, LOW);
  if (PIN_K4 >= 0) { pinMode(PIN_K4, OUTPUT); digitalWrite(PIN_K4, LOW); }
  pinMode(PIN_HOLD, OUTPUT); digitalWrite(PIN_HOLD, HIGH);

  auto cfg = M5.config();
  M5.begin(cfg);
#if BENCH_USB
  Serial.begin(115200);
  Serial.println("# Wrangler Air " FW_VERSION " bench: type help");
#endif
  M5.Display.setBrightness(255);   // always full brightness (sunlight)
  settings.load();
  settings.bootN++; settings.save();
  wlog::begin(settings.bootN);
  wlog::ev("BOOT", wlog::kv("fw", FW_VERSION) + wlog::kv("tyre", String(TYRES[settings.tyre].name)) + wlog::kv("air_L", settings.airL(), 0) + wlog::kv("target", settings.target, 2) +
                   wlog::kv("offset", settings.offset) + wlog::kv("c", settings.c, 2) + wlog::kv("rate", settings.rate, 4) +
                   wlog::kv("kTop", settings.kTop, 4) + wlog::kv("bias", settings.bias) + wlog::kv("dev", settings.dev) +
                   wlog::kv("learned", settings.learned ? "1" : "0") + wlog::kv("tauD", settings.tauD, 1) +
                   wlog::kv("learnedD", settings.learnedD ? "1" : "0") + wlog::kv("calA", settings.calA, 4) + wlog::kv("calB", settings.calB) +
                   wlog::kv("cmp_h", settings.cmpSec / 3600.0f, 2) + wlog::kv("vent_h", settings.ventSec / 3600.0f, 2) +
                   wlog::kv("svc_h", settings.svcHours(), 2));
  applyRotation();     // WIDE / PORTRAIT, chosen in the menu
  sensor.begin();
  pump.begin(&sensor, &settings);
  cal.begin(&sensor, &settings);
  input::begin();
  splash();          // outputs are already off; sensor and pump wait
}

void loop() {
  M5.update();
  sensor.update();
  pump.update();
  cal.update();
  countHours();
  traceStep();
#if BENCH_USB
  benchLoop();
#endif
  ota.update();

  int steps = input::takeSteps();
  const input::Btn b = input::button();

  switch (screen) {
    case Screen::MAIN: {
      updateNoAir();
      if (armed && steps) { disarm("KNOB"); steps = 0; }               // a turn only cancels the armed start
      if (armed && millis() - armT >= ARM_MS) disarm("TIMEOUT");
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

      // stop never needs two presses: while running any release before the LONG (2 s) stops at once
      if (pump.busy() && (b == input::Btn::SHORT || b == input::Btn::MID)) pump.stop();
      else if (b == input::Btn::SHORT) {
        if (pump.state() == PState::DONE || pump.state() == PState::ERROR) pump.clearResult();
        else if (noAir) { disarm("NO_AIR"); beep(); }                  // CONNECT SYSTEM: nothing to start
        else if (armed) { armed = false; pump.start(); }               // second press within ARM_MS: go
        else if (settings.preset == P_CUSTOM) { customEdit = settings.custom; screen = Screen::CUSTOM; }   // check the value first
        else arm();
      }
      if (b == input::Btn::LONG && !pump.busy()) { disarm("LONG"); screen = Screen::MENU; menuPos = 0; }
      static PState prev = PState::IDLE;
      if (pump.state() != prev) {
        if (pump.state() == PState::DONE) {
          if (pump.inBand()) beep(2);                                  // nothing to do, already there
          else M5.Speaker.tone(3500, DONE_BEEP_MS);                    // target reached: one long beep, non-blocking
        }
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
          case M_SETTLE:  editItem = M_SETTLE; editIdx = setIdx(); screen = Screen::EDIT; break;
          case M_SCREEN:  settings.rot = (settings.rot + 1) % 4; settings.save(); applyRotation(); break;
          case M_RESET:   // two presses within 3 s, so a stray click cannot wipe what was learned
            if (resetArm && millis() - resetArm < 3000) { settings.resetLearning(); wlog::ev("RESET_LEARNING", ""); beep(); resetArm = 0; }
            else resetArm = millis();
            break;
          case M_UPDATE:  ota.start(&settings.psi); screen = Screen::OTA; break;
          case M_VERSION: screen = Screen::VERSION; break;
          case M_HOURS:   screen = Screen::HOURS; break;
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

    case Screen::CUSTOM:   // CUSTOM preset: turn 0.1 bar, press = save and arm, long = cancel
      updateNoAir();
      if (steps) customEdit = roundf(constrain(customEdit + steps * TARGET_STEP, TARGET_MIN, TARGET_MAX) * 10) / 10.0f;
      if (b == input::Btn::SHORT) {
        settings.custom = settings.target = customEdit; settings.save();
        wlog::ev("CUSTOM_SET", wlog::kv("custom", settings.custom, 2));
        screen = Screen::MAIN;
        arm();                                                         // then press again to start
      }
      if (b == input::Btn::LONG) screen = Screen::MAIN;
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
        else if (m == Mode::DEF && now < LEARN_DEF_MIN_BAR) { learnMsg = "PUMP UP TO " + fmt(LEARN_DEF_MIN_BAR) + " FIRST"; screen = Screen::LEARN_MSG; }
        else { learnSel = m; screen = Screen::LEARN_CONF; }
      }
      if (screen == Screen::LEARN_PICK) drawLearnPick();
      break;

    case Screen::LEARN_CONF:
      if (b == input::Btn::LONG) screen = Screen::LEARN_PICK;
      if (b == input::Btn::SHORT) { pump.learnTest(learnSel); screen = Screen::MAIN; }
      if (screen == Screen::LEARN_CONF) drawLearnConf();
      break;

    case Screen::LEARN_MSG:
      if (b == input::Btn::SHORT || b == input::Btn::LONG) screen = Screen::LEARN_PICK;
      else { canvas.fillSprite(TFT_BLACK); drawLines({ {learnMsg, F_TEXT, TFT_ORANGE} }, 0, H() - 18); drawHint("press: back"); canvas.pushSprite(0, 0); }
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
      if (b == input::Btn::SHORT || b == input::Btn::LONG) screen = Screen::MENU;
      else drawVersion();
      break;

    case Screen::OUT_TEST:   // COMPRESSOR (K1) / VALVE (K4, valve builds) / BACK: press = one 1 s pulse
      if (steps) outPos = constrain(outPos + steps, 0, OUT_N - 1);
      if (b == input::Btn::LONG) { pump.stop(); screen = Screen::MENU; }
      if (b == input::Btn::SHORT) {
        if (outPos == OUT_N - 1) { pump.stop(); screen = Screen::MENU; }
        else if (!pump.pulse(outPos == 1)) beep();                     // already pulsing (or a run is active)
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

    case Screen::SENSOR:
      if (b == input::Btn::SHORT || b == input::Btn::LONG) screen = Screen::MENU;
      else drawSensor();
      break;
  }
  delay(20);
}
