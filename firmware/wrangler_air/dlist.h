// Display list for the phone (0.7.1): while a phone is connected, every text, bar, box and preset icon the screens draw
// is also recorded with its position, size and colour, one frame per push(). ble.h sends the frame, the phone draws
// it scaled. So every screen (menus, editors, calibration, LEARN, errors) reaches the phone without a second layout,
// and the phone never shows anything the DinMeter does not. Not pixels: a frame is 0.2-0.6 kB instead of 65 kB.
// The screens call the dl* wrappers instead of canvas.drawString / fillSprite / fillRect / ... (same arguments).
// Runs on the main loop only, like the drawing itself.
//
// Frame (little endian): 'D', version 1, frame id, W, H (canvas size, follows the SCREEN rotation), background 565 (u16),
// item count, then items: kind u8, x i16, y i16, w u8, h u8, colour 565 u16, aux u8, datum u8 [, len u8, text]
//   kind 1 TEXT        x, y = anchor, datum = LovyanGFX textdatum (bits 0-1 left / centre / right, 2-3 top / middle /
//                      bottom, 16 baseline), w, h = text width and font height in px, aux = font id (below)
//   kind 2 FILL_RECT   3 FILL_RRECT (aux = radius)   4 DRAW_RECT (outline)   5 FILL_CIRCLE (x, y = centre, w = r)
//   kind 6 ARROW       triangle in the box x, y, w, h; aux 0 = pointing up, 1 = down
//   kind 7 ICON        preset icon in the box x, y, w, h (= 0.62 w); aux = preset, colour grey when dimmed
// Font ids: 0 Font0 (8 px), 1 Font2 (16 px), 2 FreeSans 9 pt, 3 FreeSansBold 9 pt, 4 Bold 12 pt, 5 Bold 18 pt,
//           6 Font7 (7-segment digits, 48 px), 7 other
#pragma once
#include <M5Unified.h>

extern M5Canvas canvas;

namespace dl {
enum Kind : uint8_t { TEXT = 1, FILL_RECT, FILL_RRECT, DRAW_RECT, FILL_CIRCLE, ARROW, ICON };
constexpr int MAXI = 56, MAXT = 40;
struct Item { uint8_t kind, aux, datum, len; int16_t x, y; uint8_t w, h; uint16_t col; char t[MAXT]; };

inline bool on = false;          // record only while a phone is connected (set by ble.h)
inline bool force = false;       // USB `dl`: record without a phone, for the bench layout check
inline int mute = 0;             // > 0 inside a preset icon: its own strokes are not recorded
inline Item cur[MAXI]; inline int nCur = 0; inline uint16_t bgCur = 0;
inline Item ready[MAXI]; inline int nReady = 0; inline uint16_t bgReady = 0; inline uint8_t W = 0, H = 0;
inline uint32_t frames = 0;      // finished frames, ble.h sends the newest

inline uint8_t u8c(int v) { return (uint8_t)constrain(v, 0, 255); }
inline uint8_t fontId(const lgfx::IFont* f) {
  if (f == &fonts::Font0) return 0;            if (f == &fonts::Font2) return 1;
  if (f == &fonts::FreeSans9pt7b) return 2;    if (f == &fonts::FreeSansBold9pt7b) return 3;
  if (f == &fonts::FreeSansBold12pt7b) return 4; if (f == &fonts::FreeSansBold18pt7b) return 5;
  if (f == &fonts::Font7) return 6;
  return 7;
}
inline Item* add(Kind k, int x, int y, int w, int h, uint16_t c, uint8_t aux = 0) {
  if (!on || mute || nCur >= MAXI) return nullptr;
  Item& it = cur[nCur++];
  it.kind = k; it.x = x; it.y = y; it.w = u8c(w); it.h = u8c(h); it.col = c; it.aux = aux; it.datum = 0; it.len = 0;
  return &it;
}
// push(): the frame is complete
inline void finish() {
  if (!on) { nCur = 0; return; }
  memcpy(ready, cur, sizeof(Item) * nCur); nReady = nCur; bgReady = bgCur;
  W = u8c(canvas.width()); H = u8c(canvas.height()); frames++;
  nCur = 0;
}
// the frame as bytes; returns the length (0 = nothing yet)
inline size_t encode(uint8_t* b, size_t cap, uint8_t id) {
  if (!frames || cap < 8) return 0;
  size_t n = 0;
  b[n++] = 'D'; b[n++] = 1; b[n++] = id; b[n++] = W; b[n++] = H; b[n++] = bgReady & 0xFF; b[n++] = bgReady >> 8;
  const size_t cntAt = n++; uint8_t cnt = 0;
  for (int i = 0; i < nReady; i++) {
    const Item& it = ready[i];
    const size_t need = 11 + (it.kind == TEXT ? 1 + it.len : 0);
    if (n + need > cap) break;
    b[n++] = it.kind; b[n++] = it.x & 0xFF; b[n++] = (it.x >> 8) & 0xFF; b[n++] = it.y & 0xFF; b[n++] = (it.y >> 8) & 0xFF;
    b[n++] = it.w; b[n++] = it.h; b[n++] = it.col & 0xFF; b[n++] = it.col >> 8; b[n++] = it.aux; b[n++] = it.datum;
    if (it.kind == TEXT) { b[n++] = it.len; memcpy(b + n, it.t, it.len); n += it.len; }
    cnt++;
  }
  b[cntAt] = cnt;
  return n;
}
// ---------- preset icon, vector (0.7.2): the icon's own strokes in 1/8 px relative to its box ----------
// Sent once per connection and icon ('I' blob below), the frame refers to it by a 16-bit key (ICON item, colour field).
// Kinds: 1 LINE a,b - c,d width e px; 2 TRI a,b c,d f,g; 3 CIRCLE centre a,b radius c; 4 RING centre a,b radius c, e px thick;
//        5 RECT corners a,b - c,d (inclusive); 6 RRECT the same, radius f; 7 SUV ground under the rear wheel a,b,
//        c = px per unit x 256, d = angle x 100 (degrees, negative = nose up), bitmap suv_bitmap.h in the app
enum IKind : uint8_t { I_LINE = 1, I_TRI, I_CIRCLE, I_RING, I_RECT, I_RRECT, I_SUV };
struct IItem { uint8_t k, e; int16_t a, b, c, d, f, g; uint16_t col; };
constexpr int MAXIC = 200;
inline IItem ic[MAXIC]; inline int nIc = 0; inline float icX = 0, icY = 0; inline bool icRec = false;
inline uint16_t icKey = 0;
inline int16_t q8(float v) { return (int16_t)constrain(lroundf(v * 8), -32768L, 32767L); }
inline void icBegin(float ox, float oy) { nIc = 0; icX = ox; icY = oy; icRec = on; }
inline void icAdd(IKind k, float a, float b, float c, float d, float f, float g, uint8_t e, uint16_t col, bool rel = true) {
  if (!icRec || nIc >= MAXIC) return;
  IItem& it = ic[nIc++];
  const float dx = rel ? icX : 0, dy = rel ? icY : 0;
  it.k = k; it.e = e; it.col = col;
  it.a = q8(a - dx); it.b = q8(b - dy);
  if (k == I_LINE || k == I_RECT || k == I_RRECT || k == I_TRI) { it.c = q8(c - dx); it.d = q8(d - dy); } else { it.c = q8(c); it.d = (int16_t)d; }
  if (k == I_TRI) { it.f = q8(f - dx); it.g = q8(g - dy); } else { it.f = q8(f); it.g = 0; }
}
inline size_t icEncode(uint8_t* b, size_t cap) {
  size_t n = 0; if (cap < 6) return 0;
  b[n++] = 'I'; b[n++] = 1; b[n++] = icKey & 0xFF; b[n++] = icKey >> 8; const size_t cntAt = n++; uint8_t cnt = 0;
  for (int i = 0; i < nIc && n + 16 <= cap && cnt < 255; i++, cnt++) {
    const IItem& it = ic[i];
    b[n++] = it.k; b[n++] = it.e;
    for (int16_t v : {it.a, it.b, it.c, it.d, it.f, it.g}) { b[n++] = v & 0xFF; b[n++] = (v >> 8) & 0xFF; }
    b[n++] = it.col & 0xFF; b[n++] = it.col >> 8;
  }
  b[cntAt] = cnt;
  return n;
}
// end of the icon: its key (FNV-1a over the strokes), so the phone draws a cached copy
inline uint16_t icEnd() {
  if (!icRec) return 0;
  icRec = false;
  uint32_t h = 2166136261u;
  const uint8_t* p = (const uint8_t*)ic;
  for (size_t i = 0; i < sizeof(IItem) * nIc; i++) { h ^= p[i]; h *= 16777619u; }
  icKey = (uint16_t)((h >> 16) ^ h); if (!icKey) icKey = 1;
  return icKey;
}

inline uint16_t to565(uint32_t rgb888) { return ((rgb888 >> 8) & 0xF800) | ((rgb888 >> 5) & 0x07E0) | ((rgb888 >> 3) & 0x001F); }
}  // namespace dl

// ---------- wrappers: draw on the canvas and record ----------
inline void dlClear(uint16_t c) { canvas.fillSprite(c); dl::nCur = 0; dl::bgCur = c; }
inline void dlString(const String& t, int x, int y) {
  canvas.drawString(t, x, y);
  if (!dl::on || dl::mute || !t.length()) return;
  const auto& st = canvas.getTextStyle();
  dl::Item* it = dl::add(dl::TEXT, x, y, canvas.textWidth(t), canvas.fontHeight(), dl::to565(st.fore_rgb888), dl::fontId(canvas.getFont()));
  if (!it) return;
  it->datum = (uint8_t)st.datum;
  it->len = (uint8_t)min((int)t.length(), dl::MAXT); memcpy(it->t, t.c_str(), it->len);
}
inline void dlFillRect(int x, int y, int w, int h, uint16_t c) { canvas.fillRect(x, y, w, h, c); dl::add(dl::FILL_RECT, x, y, w, h, c); }
inline void dlFillRRect(int x, int y, int w, int h, int r, uint16_t c) { canvas.fillRoundRect(x, y, w, h, r, c); dl::add(dl::FILL_RRECT, x, y, w, h, c, dl::u8c(r)); }
inline void dlDrawRect(int x, int y, int w, int h, uint16_t c) { canvas.drawRect(x, y, w, h, c); dl::add(dl::DRAW_RECT, x, y, w, h, c); }
inline void dlFillCircle(int x, int y, int r, uint16_t c) { canvas.fillCircle(x, y, r, c); dl::add(dl::FILL_CIRCLE, x, y, r, r, c); }
// direction arrow in the box (x, y, w, h)
inline void dlArrow(int x, int y, int w, int h, bool up, uint16_t c) {
  if (up) canvas.fillTriangle(x, y + h, x + w, y + h, x + w / 2, y, c);
  else    canvas.fillTriangle(x, y, x + w, y, x + w / 2, y + h, c);
  dl::add(dl::ARROW, x, y, w, h, c, up ? 0 : 1);
}
