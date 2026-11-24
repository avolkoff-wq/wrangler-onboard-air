// Non-blocking beeps (review item 4): a short note list played from loop(), so a stop pressed on the knob or sent by
// the phone is handled at once. The old beep() / beepErr() blocked the loop for 0.2-2 s with delay().
// A new sound replaces one still playing. Sounds: high = good, low = look at the screen.
#pragma once
#include <M5Unified.h>
#include "pump.h"

namespace sound {
struct Note { uint16_t f, ms, gap; };   // tone Hz, length ms, silence after it ms
constexpr int QN = 12;
inline Note q[QN]; inline int qn = 0, qi = 0; inline uint32_t nextT = 0;
constexpr uint16_t HIGH_HZ = 3500, LOW_HZ = 1800;

inline void play(const Note* n, int k) {
  qn = 0; qi = 0; nextT = 0;
  for (int i = 0; i < k && qn < QN; i++) q[qn++] = n[i];
}
inline void repeat(Note n, int k) { qn = 0; qi = 0; nextT = 0; for (int i = 0; i < k && qn < QN; i++) q[qn++] = n; }
// call every loop pass
inline void update() {
  if (qi >= qn) return;
  const uint32_t now = millis();
  if (nextT && (int32_t)(now - nextT) < 0) return;
  M5.Speaker.tone(q[qi].f, q[qi].ms);
  nextT = now + q[qi].ms + q[qi].gap; if (!nextT) nextT = 1;
  qi++;
}
inline bool busy() { return qi < qn; }

inline void beep(int n = 1)  { repeat({HIGH_HZ, 120, 60}, n); }          // acknowledge / refused press
inline void done()           { repeat({HIGH_HZ, 1500, 0}, 1); }          // target reached
inline void inBand()         { beep(2); }                                 // nothing to do, already there
inline void offTarget()      { repeat({LOW_HZ, 120, 100}, 3); }           // run over, result outside DONE_TOL
// every error: three short low beeps; OVER and PUMP_ON (air with no output) a fast low series ~2 s
inline void error(PErr e) {
  if (e == PErr::OVER || e == PErr::PUMP_ON) repeat({LOW_HZ, 90, 110}, 10);
  else repeat({LOW_HZ, 120, 100}, 3);
}
}  // namespace sound
