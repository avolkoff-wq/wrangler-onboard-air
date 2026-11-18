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
// Screen (layout: docs/screen/screen_mockup.py, screen_mockup_idle_direction.py)
//   idle: the preset icon (SUV on its terrain), the target in big digits, the preset name and, under a coloured
//   rule, the live pressure ("actual x.x") and what a start would do: INFLATE / DEFLATE (with "press to start"),
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

// idle screen and the CUSTOM value screen (edit = true): icon, big target value, unit, preset name and, below a
// coloured rule, the live pressure and what a start would do (spec: docs/screen/screen_mockup_idle_direction.py)
void drawIdle(bool edit) {
  canvas.fillSprite(TFT_BLACK);
  const int p = settings.preset;
  const float bar = edit ? customEdit : settings.target;
  const bool low = bar < TARGET_LOW_WARN;
  const float now = sensor.bar(settings.calA, settings.calB);
  // what a start would do
  enum { D_UP, D_DOWN, D_OK, D_ABOVE, D_DEAD } dir;
  if (noAir) dir = D_DEAD;                                             // hoses not connected / no air
  else if (fabsf(now - bar) <= START_BAND) dir = D_OK;
  else if (now < bar) dir = D_UP;
  else dir = DEFLATE ? D_DOWN : D_ABOVE;                               // inflate-only builds do nothing above the target
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
      textAt(nowT, W() / 2, y, F_TEXT, dc, top_center); part2((W() - w2) / 2, y + 18);
    }
  };
  const bool hint = dir == D_UP || dir == D_DOWN;                      // "press to start" only when a start would do something
  const Font F_HINT = &fonts::Font2;
  iconDim = dead;
  if (!portrait()) {
    drawPresetIcon(p, 6, 6, 104);
    iconDim = false;
    bigValue(fmt(bar), false, W() - 8, 2, numc, 1);
    textAt(unitName(), W() - 8, 56, F_TEXT, unitc, top_right);
    textAt(name + (low && !edit ? "  LOW" : ""), W() / 2, 72, F_MID, namec, top_center, F_TEXT, W() - 8);
    if (edit) textAt("turn 0.1   press: OK + start", W() / 2, 110, F_SMALL, C_GREY, top_center);
    else {
      canvas.fillRect(10, 94, W() - 20, 2, dead ? dim : dc);
      if (dead) {
        textAt("CONNECT SYSTEM", W() / 2, 99, F_TEXT, C_LGREY, top_center);
        textAt(nowT + " " + unitName(), W() / 2, 119, F_HINT, C_GREY, top_center);
      } else dirLine(99, false);
      if (hint && !dead) textAt("press to start", W() / 2, 119, F_HINT, C_GREY, top_center);
    }
  } else {
    drawPresetIcon(p, 6, !strcmp(PRESETS[p].name, "ROCK") ? 14 : 8, 123);   // tilted ROCK SUV would touch the top edge
    iconDim = false;
    textAt(name, W() / 2, 90, F_MID, namec, top_center, F_TEXT, W() - 6);
    bigValue(fmt(bar), false, W() / 2, 110, numc, 0);
    textAt(String(unitName()) + (low && !edit ? "  LOW" : ""), W() / 2, 164, F_TEXT, low && !edit && !dead ? TFT_ORANGE : unitc, top_center);
    if (edit) textAt("turn 0.1, press OK", W() / 2, 200, F_SMALL, C_GREY, top_center);
    else {
      canvas.fillRect(10, 183, W() - 20, 2, dead ? dim : dc);
      if (dead) {
        textAt("CONNECT", W() / 2, 188, F_TEXT, C_LGREY, top_center);
        textAt("SYSTEM", W() / 2, 206, F_TEXT, C_LGREY, top_center);
        textAt(nowT, W() / 2, 224, F_HINT, C_GREY, top_center);
      } else dirLine(188, true);
      if (hint && !dead) textAt("press to start", W() / 2, 224, F_HINT, C_GREY, top_center);
    }
  }
  canvas.pushSprite(0, 0);
}

void drawMain() {
  const PState st = pump.state();
  if (st == PState::IDLE) { drawIdle(false); return; }
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
    case PState::CAL_PULSE: line = "LEARNING " + tm; col = TFT_ORANGE; break;
    case PState::MAIN:      line = "PUMP " + tm; col = TFT_ORANGE; break;
    case PState::TOPUP:     line = "TOP-UP " + tm; col = TFT_ORANGE; break;
    case PState::VENT:      line = "VENT " + tm; col = TFT_SKYBLUE; break;
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
    y = 78; x0 = 10; x1 = W() - 10;
  } else {
    textAt((DEFLATE ? mode : "") + mmss(pump.runMs()), 4, 2, F_SMALL, C_LGREY, top_left);
    drawPresetIcon(settings.preset, 22, 22, 90);
    bigValue(val, est, W() / 2, 82, valc, 0);
    y = 146; x0 = 8; x1 = W() - 8;
  }
  // progress bar p0 -> target, coloured by what is happening now
  const bool known = pump.mode() != Mode::NONE;
  const float p0 = pump.p0(), span = settings.target - p0;
  const float f = !known ? 0 : fabsf(span) < 0.01f ? 1.0f : constrain((cur - p0) / span, 0.0f, 1.0f);
  const uint16_t bc = pump.compressorOn() ? TFT_ORANGE : pump.valveOpen() ? TFT_SKYBLUE : (st == PState::DONE ? TFT_GREEN : TFT_CYAN);
  canvas.fillRect(x0, y, x1 - x0 + 1, 13, TFT_BLACK);              // bar inside stays black so the fill reads
  canvas.drawRect(x0, y, x1 - x0 + 1, 13, C_GREY);
  if (known) canvas.fillRect(x0 + 2, y + 2, (int)((x1 - x0 - 4) * f) + 1, 9, bc);
  if (known) textAt(fmt(p0), x0, y + 14, F_SMALL, C_GREY, top_left);
  textAt(fmt(settings.target), x1, y + 14, F_SMALL, C_GREY, top_right);
  if (!portrait()) textAt(line, W() / 2, H() - 22, F_TEXT, col, top_center, F_SMALL, W() - 4);
  else {
    textAt(line, W() / 2, H() - 50, F_TEXT, col, top_center, F_SMALL, W() - 4);
    textAt(counts, W() / 2, H() - 22, F_SMALL, C_GREY, top_center);
  }
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

void drawCustom() { drawIdle(true); }   // CUSTOM value: idle layout, value and name in cyan

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
      updateNoAir();
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
        else if (noAir) beep();                                        // CONNECT SYSTEM: nothing to start
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
      updateNoAir();
      if (steps) customEdit = roundf(constrain(customEdit + steps * TARGET_STEP, TARGET_MIN, TARGET_MAX) * 10) / 10.0f;
      if (b == input::Btn::SHORT) {
        settings.custom = settings.target = customEdit; settings.save();
        wlog::ev("CUSTOM_SET", wlog::kv("custom", settings.custom, 2));
        screen = Screen::MAIN;
        if (noAir) beep(); else pump.start();
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
