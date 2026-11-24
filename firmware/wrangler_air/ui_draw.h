// Drawing helpers, scenery and icons extracted from wrangler_air.ino
// Header-only: included once from the .ino after globals and enums.
#pragma once
#include <M5Unified.h>
#include <initializer_list>
#include "config.h"
#include "suv_bitmap.h"
#include "storage.h"

extern M5Canvas canvas;
extern Settings settings;

// ---------- helpers (fixed buffers, avoid String in hot paths) ----------
inline float toUnit(float bar) { return settings.psi ? bar * 14.5038f : bar; }
inline const char* unitName() { return settings.psi ? "psi" : "bar"; }

// rotating scratch buffers: safe for a few simultaneous uses in one expression
inline char* uiScratch() {
  static char bufs[8][48];
  static uint8_t i;
  return bufs[i++ & 7];
}
inline const char* fmt(float bar) {
  char* b = uiScratch();
  snprintf(b, 48, settings.psi ? "%.0f" : "%.1f", toUnit(bar));
  return b;
}
// m:ss.t (tenths of a second)
inline const char* mmss(uint32_t ms) {
  const uint32_t t = ms / 100, s = t / 10;
  char* b = uiScratch();
  snprintf(b, 48, "%lu:%02lu.%lu", (unsigned long)(s / 60), (unsigned long)(s % 60), (unsigned long)(t % 10));
  return b;
}
// countdown when the length is known, elapsed otherwise
inline const char* phaseTimer(uint32_t elapsed, uint32_t len) {
  char* b = uiScratch();
  if (len) {
    const uint32_t left = elapsed >= len ? 0 : len - elapsed;
    snprintf(b, 48, "%.1fs", left / 1000.0f);
  } else {
    const uint32_t t = elapsed / 100, s = t / 10;
    snprintf(b, 48, "%lu:%02lu.%lu", (unsigned long)(s / 60), (unsigned long)(s % 60), (unsigned long)(t % 10));
  }
  return b;
}
inline const char* offText(int i) {
  if (i == 0) return "AUTO";
  char* b = uiScratch();
  snprintf(b, 48, "%.2f bar", i * 0.05f);
  return b;
}
inline const char* setText(int i) {
  if (i == 0) return "AUTO";
  char* b = uiScratch();
  snprintf(b, 48, "%.1f s", 0.5f + i * 0.5f);
  return b;
}
inline const char* hmm(uint32_t sec) {
  char* b = uiScratch();
  snprintf(b, 48, "%lu:%02lu", (unsigned long)(sec / 3600), (unsigned long)(sec / 60 % 60));
  return b;
}
inline const char* rotName(uint8_t rot) {
  switch (rot) { case 0: return "PORTRAIT"; case 2: return "PORTRAIT 180"; case 3: return "WIDE 180"; default: return "WIDE"; }
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

// SUV side view, nose to the right: the owner's reference mask (suv_bitmap.h, spec docs/screen/sim2.py suv()), 40 units long,
// pushed rotated and zoomed. (x, y) = ground under the rear wheel, s = px per unit, ang in degrees (negative = nose up)
constexpr float SUV_LEN = 40.0f, SUV_WB = 40.0f * (760 - 186.5f) / 891;   // length, wheelbase (units)
M5Canvas suvSpr(&canvas);
void drawSuv(float x, float y, float s, float ang = 0, float sink = 0) {
  (void)sink;
  if (!suvSpr.getBuffer()) {
    suvSpr.setColorDepth(1);
    suvSpr.createSprite(SUV_BMP_W, SUV_BMP_H);
    suvSpr.setPaletteColor(0, TFT_BLACK); suvSpr.setPaletteColor(1, TFT_WHITE);
    suvSpr.fillSprite(0);
    suvSpr.drawBitmap(0, 0, SUV_BMP, SUV_BMP_W, SUV_BMP_H, 1);
    suvSpr.setPivot(SUV_PIVOT_X, SUV_PIVOT_Y);
  }
  const float z = SUV_LEN * s / SUV_BMP_W;
  suvSpr.pushRotateZoomWithAA(&canvas, x, y, ang, z, z, 0);           // palette index 0 transparent
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
  // 0.6.2, owner: the Font2 hint was too small to read. One line of the bold 9 pt font in capitals: every group if they
  // fit (bold, then the regular 9 pt), else as many leading groups as fit in bold, so each call lists the main action
  // first. Only when even the first group is too wide (the PUMP CHECK advice) the old wrapped Font2 text is used.
  {
    String up = t; up.toUpperCase();
    const int maxW = W() - 6;
    Font bf = nullptr; String show;
    canvas.setFont(F_TEXT);
    if (canvas.textWidth(up) <= maxW) { bf = F_TEXT; show = up; }
    else {
      canvas.setFont(F_SMALL);
      if (canvas.textWidth(up) <= maxW) { bf = F_SMALL; show = up; }
      else {
        canvas.setFont(F_TEXT);
        int i = 0;
        while (i < (int)up.length()) {
          int j = up.indexOf("  ", i); if (j < 0) j = up.length();
          const String cand = up.substring(0, j);
          if (canvas.textWidth(cand) > maxW) break;
          show = cand; bf = F_TEXT; i = j + 2;
        }
      }
    }
    if (bf) {
      const int top = bottom - 18;
      textAt(show, W() / 2, top, bf, c == C_GREY ? C_LGREY : c, top_center);
      hintTop = top;
      return top;
    }
  }
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

