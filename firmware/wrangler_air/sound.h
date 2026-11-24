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

// 0.7.6, owner: every sound is repeated on the phone. mark() publishes its id in the STATE packet (ble.h); the app holds
// the same tones under the same ids: 1 tick, 2 click, 3 hold, 4 menu, 5 beep, 6 done, 7 two beeps, 8 off target,
// 9 error, 10 error fast series, 11 refused, 12 compressor tick, 13 valve knock, 14 three beeps
inline void mark(uint8_t id) { g_sndId = id; g_sndSeq = g_sndSeq + 1; }
inline void beep(int n = 1)  { mark(n == 1 ? 5 : n == 2 ? 7 : 14); repeat({HIGH_HZ, 120, 60}, n); }          // acknowledge / refused press
// input feedback (0.6.2, owner): a turn ticks, a press clicks, a hold is one lower tone, opening the menu a rising three-note call.
// They never cut a result or an error sound short: input feedback is skipped while one is playing (the .ino checks busy()).
inline void tick()           { mark(1); repeat({2600, 15, 0}, 1); }
inline void click()          { mark(2); repeat({HIGH_HZ, 45, 0}, 1); }
inline void hold()           { mark(3); repeat({2200, 160, 0}, 1); }
inline void menuIn()         { mark(4); static const Note n[] = {{2400, 70, 25}, {3000, 70, 25}, {3700, 130, 0}}; play(n, 3); }
// working sound (0.6.2, owner): told apart by rhythm, not only by pitch (the first version, 2100 / 1500 Hz at the same
// pace, sounded alike). Compressor: a fast high rattle, a tick every 150 ms. Valve: a slow low knock every 700 ms.
// Never over another sound (the .ino checks busy()).
// 0.7.1, owner: the pace quickens as the pressure nears the target, like a parking sensor. f = share of the way
// from the start pressure to the target, 0..1. Compressor 300 -> 70 ms, valve 1000 -> 250 ms.
inline uint32_t workEveryMs(bool valve, float f) {
  f = constrain(f, 0.0f, 1.0f);
  return valve ? (uint32_t)(1000 - 750 * f) : (uint32_t)(300 - 230 * f);
}
inline void work(bool valve) { mark(valve ? 13 : 12); if (valve) repeat({1000, 60, 0}, 1); else repeat({2800, 15, 0}, 1); }
// refused (0.7.5, owner): a start pressed while CONNECT SYSTEM is shown, or with nothing to do: two short low beeps,
// clearly not the single high "accepted" click. Errors stay three low beeps.
inline void refuse()         { mark(11); repeat({LOW_HZ, 110, 90}, 2); }
inline void done()           { mark(6); repeat({HIGH_HZ, 1500, 0}, 1); }          // target reached
inline void inBand()         { beep(2); }                                 // nothing to do, already there
inline void offTarget()      { mark(8); repeat({LOW_HZ, 120, 100}, 3); }           // run over, result outside DONE_TOL
// every error: three short low beeps; OVER and PUMP_ON (air with no output) a fast low series ~2 s
inline void error(PErr e) {
  if (e == PErr::OVER || e == PErr::PUMP_ON) { mark(10); repeat({LOW_HZ, 90, 110}, 10); }
  else { mark(9); repeat({LOW_HZ, 120, 100}, 3); }
}
}  // namespace sound
