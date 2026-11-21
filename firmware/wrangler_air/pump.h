// Run state machine: inflate with the compressor (K1), and in the deflate build (schematic C) vent with the valve (K4).
// Goal: reach the target with as few compressor starts / valve openings as possible.
//
//  start -> SETTLE+MEASURE (static p0)
//        -> within +/-START_BAND of the target: DONE, nothing touched
//        -> p0 below target: INFLATE
//             [first auto run] CAL_PULSE 15 s -> SETTLE+MEASURE -> learn
//             MAIN: compressor on, tyres = sensor - offset, stop at target - margin -> SETTLE+MEASURE -> learn
//             TOPUP (time = deficit / kTop) -> SETTLE+MEASURE -> learn kTop ... up to 3 times
//        -> p0 above target (deflate build only): DEFLATE
//             VENT: valve open for t = tauD * ln(p_abs / p_aim_abs), compressor off -> SETTLE+MEASURE -> learn tauD
//             first vent before anything is learned goes only 60 % of the way; then more vents, up to 4
//             overshoot below the target: a timed compressor top-up brings it back (hybrid)
//        -> DONE
//  learnTest(INF / DEF) (menu LEARN): SETTLE+MEASURE -> one fixed 15 s compressor pulse (same as CAL_PULSE, learnMain)
//        or one 20 s valve opening (tauD set directly from it) -> SETTLE+MEASURE -> DONE. Same safety checks as a run.
//
// Inflate offset while pumping (sensor reads high because air is flowing):
//   AUTO   offset = c * rise rate   (c learned; scalar learned offset until rate is valid)
//   MANUAL offset = fixed value from the menu
// Deflate does not use the reading while the valve is open (the line pressure says little), only static measurements.
// Settling before a static measurement:
//   AUTO   wait until the reading is steady (< 0.005 bar/s), 1..6 s
//   MANUAL fixed time from the menu
// Stop margin adapts to how well past stops landed (bias and spread of the miss).
// Every phase, measurement and learning step goes to the log (log.h).
#pragma once
#include <Arduino.h>
#include "config.h"
#include "sensor.h"
#include "storage.h"
#include "log.h"

enum class PState { IDLE, SETTLE, MEASURE, CAL_PULSE, MAIN, TOPUP, VENT, DONE, ERROR };
enum class PErr   { NONE, SENSOR, OVER, LEAK, TIMEOUT, LOW_P, LOW_V, VALVE, PUMP_ON };   // PUMP_ON: pressure rises with both outputs off   // VALVE: pressure falls while the valve should be shut   // LOW_P: LEARN DEFLATE below LEARN_DEF_MIN_BAR; LOW_V: AUX1 sagged
enum class Phase  { START, AFTER_CAL, AFTER_MAIN, AFTER_TOPUP, AFTER_VENT };
enum class Mode   { NONE, INF, DEF };

class Pump {
 public:
  void begin(Sensor* s, Settings* st) { s_ = s; st_ = st; relay(false); valve(false); }

  void start() {
    if (busy()) return;
    test_ = Mode::NONE; testOk_ = false; testRes_ = 0;
    err_ = PErr::NONE; topups_ = 0; starts_ = 0; vents_ = 0; inBand_ = false; mode_ = Mode::NONE; runStart_ = millis();
    wlog::ev("RUN", wlog::kv("target", st_->target, 2) + wlog::kv("fw", FW_VERSION));
    goSettle(Phase::START);
  }
  // menu LEARN: one fixed pulse (INF) or valve opening (DEF) between two static measurements
  void learnTest(Mode m) {
    if (busy() || m == Mode::NONE || (m == Mode::DEF && !DEFLATE)) return;
    start();
    test_ = m;
    wlog::ev("LEARN_TEST", wlog::kv("mode", String(m == Mode::DEF ? "DEF" : "INF")));
  }
  Mode  testMode() const { return test_; }       // NONE = normal run
  bool  testOk() const { return testOk_; }       // last test gave a sane value
  float testResult() const { return testRes_; }  // tauD (s) after DEF, rate (bar/s) after INF
  float testBefore() const { return p0_; }
  // menu OUTPUT TEST: one OUTPUT_PULSE_MS pulse on the compressor (K1) or the valve (K4), never latched.
  // relay() / valve() keep the other output off. Not during a run.
  bool pulse(bool valveOut, uint32_t ms = OUTPUT_PULSE_MS) {
    if (busy() || pulseT_ || (valveOut && !DEFLATE)) return false;
    pulseLen_ = ms;
    pulseValve_ = valveOut; pulseBefore_ = s_->bar(st_->calA, st_->calB);
    if (valveOut) { relay(false); valve(true); } else relay(true);
    pulseT_ = millis(); if (!pulseT_) pulseT_ = 1;
    return true;
  }
  bool pulsing() const { return pulseT_ != 0; }
  bool pulseIsValve() const { return pulseValve_; }
  uint32_t pulseLeft() const { return pulseT_ ? pulseLen_ - min(pulseLen_, (uint32_t)(millis() - pulseT_)) : 0; }
  void pulseOff() { if (pulseT_) { pulseLen_ = 0; } }   // ends at the next update(), logged like a timed pulse
  void stop() {
    pulseT_ = 0;
    relay(false); valve(false);
    if (busy()) logEnd("STOPPED");
    state_ = PState::IDLE; endT_ = millis();
  }
  // main loop, idle: the line keeps falling with both outputs off -> valve (or its relay) stuck open; outputs off, ERR VALVE OPEN
  void valveStuck(float p) { pulseT_ = 0; p0_ = pStatic_ = p; mode_ = Mode::NONE; runStart_ = millis(); fail(PErr::VALVE); }
  // main loop, idle: the line keeps rising with both outputs off -> compressor running on its own (relay contacts welded)
  void pumpStuck(float p) { pulseT_ = 0; p0_ = pStatic_ = p; mode_ = Mode::NONE; runStart_ = millis(); fail(PErr::PUMP_ON); }
  void lowVoltage() { if (busy() && on_) fail(PErr::LOW_V); }   // main loop: AUX1 below LOW_V for LOW_V_MS
  bool busy() const { return state_ != PState::IDLE && state_ != PState::DONE && state_ != PState::ERROR; }
  void clearResult() { if (!busy()) state_ = PState::IDLE; }

  PState state() const { return state_; }
  PErr   error() const { return err_; }
  Mode   mode() const { return mode_; }
  bool   inBand() const { return inBand_; }        // last run: nothing to do, already within +/-START_BAND
  bool   compressorOn() const { return on_; }
  bool   valveOpen() const { return vOn_; }
  float  staticBar() const { return pStatic_; }
  float  estimate() const { return est_; }
  float  p0() const { return p0_; }                // static pressure at the start of the run (valid once mode() is set)
  bool   estimating() const { return on_; }
  int    starts() const { return starts_; }      // compressor starts in the last run
  int    vents() const { return vents_; }        // valve openings in the last run
  // timers for the screen
  uint32_t runMs()   const { return (busy() ? millis() : endT_) - runStart_; }
  uint32_t phaseMs() const { return millis() - t0_; }
  bool paused() const { return state_ == PState::SETTLE || state_ == PState::MEASURE; }
  // time left until the static measurement is done (settle + measure).
  // AUTO settle has no fixed length: the last learned settle time is the estimate.
  uint32_t pauseLeftMs() const {
    const uint32_t el = millis() - t0_;
    if (state_ == PState::MEASURE) return el >= MEASURE_MS ? 0 : MEASURE_MS - el;
    if (state_ != PState::SETTLE) return 0;
    const uint32_t len = (uint32_t)((st_->settleAuto ? st_->settleLearn : st_->settleManual) * 1000);
    return (el >= len ? 0 : len - el) + MEASURE_MS;
  }
  // planned length of the current phase, 0 = open-ended (show elapsed instead)
  uint32_t phaseLenMs() const {
    switch (state_) {
      case PState::SETTLE:    return st_->settleAuto ? 0 : (uint32_t)(st_->settleManual * 1000);
      case PState::MEASURE:   return MEASURE_MS;
      case PState::CAL_PULSE: return CAL_PULSE_MS;
      case PState::TOPUP:     return topupMs_;
      case PState::VENT:      return ventMs_;
      default:                return 0;
    }
  }
  float  margin() const {
    if (!st_->learned) return STOP_MARGIN;
    return constrain(MARGIN_MIN + 2.0f * st_->dev, MARGIN_MIN, MARGIN_MAX);
  }

  void update() {
    const uint32_t now = millis();
    const float sb = s_->bar(st_->calA, st_->calB);
    if (pulseT_ && (now - pulseT_ >= pulseLen_ || s_->fault() || sb > TARGET_MAX + 0.05f)) {
      relay(false); valve(false); pulseT_ = 0;
      wlog::ev("OUTPUT_TEST", wlog::kv("output", String(pulseValve_ ? "VALVE" : "K1")) + wlog::kv("p_before", pulseBefore_) +
               wlog::kv("p_after", sb) + wlog::kv("fault", s_->fault() ? "1" : "0"));
    }

    if (busy()) {
      if (s_->fault())                       return fail(PErr::SENSOR);
      if (now - runStart_ > RUN_TIMEOUT_MS)  return fail(PErr::TIMEOUT);
      // over-pressure after our own pumping (tyres that start above 3 bar are fine to deflate)
      if (!on_ && starts_ > 0 && sb > TARGET_MAX + 0.05f) return fail(PErr::OVER);
    }
    // compressor has been running for a while, yet the sensor reads ~0: signal wire broken or sensor dead
    if (busy() && on_ && now - onT_ > DEAD_AFTER_MS && sb < DEAD_BAR) return fail(PErr::SENSOR);
    if (on_) trackRate(now, sb);
    est_ = on_ ? sb - offsetNow() : sb;
    if (busy() && on_ && est_ > TARGET_MAX)  return fail(PErr::OVER);

    switch (state_) {
      case PState::SETTLE: {
        const uint32_t el = now - t0_;
        bool steady;
        if (st_->settleAuto) {
          if (now - tS_ >= 250) { slope_ = fabsf(sb - sS_) / ((now - tS_) / 1000.0f); sS_ = sb; tS_ = now; }
          steady = (el >= SETTLE_MIN_MS && slope_ < SETTLE_SLOPE) || el >= SETTLE_MAX_MS;
        } else {
          steady = el >= (uint32_t)(st_->settleManual * 1000);
        }
        if (steady) {
          if (st_->settleAuto) st_->settleLearn = 0.7f * st_->settleLearn + 0.3f * (el / 1000.0f);
          settleEl_ = el;
          state_ = PState::MEASURE; t0_ = now; avg_.start();
        }
      } break;

      case PState::MEASURE:
        avg_.add(sb);
        if (now - t0_ >= MEASURE_MS) onStatic(avg_.value());
        break;

      case PState::CAL_PULSE:
        if (now - t0_ >= CAL_PULSE_MS) { markStop(now, sb, "CAL_PULSE"); goSettle(Phase::AFTER_CAL); }
        break;

      case PState::MAIN:
        if (est_ > leakRef_ + LEAK_MIN_RISE) { leakRef_ = est_; leakT_ = now; }
        if (now - leakT_ > LEAK_WINDOW_MS) return fail(PErr::LEAK);
        // aim: real ~ est + bias; stop early by the margin
        if (est_ + (st_->learned ? st_->bias : 0) >= st_->target - margin()) { markStop(now, sb, "MAIN"); goSettle(Phase::AFTER_MAIN); }
        break;

      case PState::TOPUP:
        if (now - t0_ >= topupMs_) {
          relay(false); pumpTime_ = topupMs_ / 1000.0f;
          wlog::ev("STOP", wlog::kv("phase", String("TOPUP")) + wlog::kv("t_s", pumpTime_, 2) + wlog::kv("line_bar", sb));
          goSettle(Phase::AFTER_TOPUP);
        }
        break;

      case PState::VENT:
        if (now - t0_ >= ventMs_) {
          valve(false);
          wlog::ev("STOP", wlog::kv("phase", String("VENT")) + wlog::kv("t_s", ventMs_ / 1000.0f, 2) + wlog::kv("line_bar", sb));
          goSettle(Phase::AFTER_VENT);
        }
        break;

      default: break;
    }
  }

 private:
  // ---------- outputs: compressor and valve are never on together ----------
  void relay(bool on) {
    if (on) valve(false);
    if (on && !on_) { starts_++; onT_ = millis(); rN_ = 0; }
    on_ = on; digitalWrite(PIN_K1, on ? HIGH : LOW);
  }
  void valve(bool open) {
    if (PIN_K4 < 0 || !DEFLATE) { vOn_ = false; return; }
    if (open) { digitalWrite(PIN_K1, LOW); on_ = false; vents_++; }
    vOn_ = open; digitalWrite(PIN_K4, open ? HIGH : LOW);
  }
  void fail(PErr e) {
    relay(false); valve(false); err_ = e; state_ = PState::ERROR; endT_ = millis();
    static const char* N[] = {"NONE", "SENSOR", "OVER", "LEAK", "TIMEOUT", "LOW_P", "LOW_V", "VALVE", "PUMP_ON"};
    logEnd(String("ERR_") + N[(int)e]);
  }
  void done() { state_ = PState::DONE; endT_ = millis(); logEnd(inBand_ ? "IN_BAND" : "DONE"); }
  void logEnd(const String& res) {
    wlog::ev("END", wlog::kv("result", res) + wlog::kv("mode", modeName()) + wlog::kv("target", st_->target, 2) +
                    wlog::kv("p0", p0_, 3) + wlog::kv("p", pStatic_, 3) + wlog::kv("starts", (float)starts_, 0) +
                    wlog::kv("vents", (float)vents_, 0) + wlog::kv("run_s", runMs() / 1000.0f, 1) +
                    wlog::kv("cmp_h", st_->cmpSec / 3600.0f, 2) + wlog::kv("vent_h", st_->ventSec / 3600.0f, 2));
  }
  String modeName() const { return mode_ == Mode::INF ? "INF" : mode_ == Mode::DEF ? "DEF" : "-"; }
  void goSettle(Phase ph) { phase_ = ph; state_ = PState::SETTLE; t0_ = millis(); tS_ = t0_; sS_ = -99; slope_ = 99; }

  // ---------- live rise rate while pumping ----------
  void trackRate(uint32_t now, float sb) {
    if (rN_ == 0 || now - rT_[(rH_ + rN_ - 1) % RN] >= 100) {   // one sample per 100 ms
      const int i = (rH_ + rN_) % RN;
      if (rN_ == RN) rH_ = (rH_ + 1) % RN; else rN_++;
      rT_[i] = now; rV_[i] = sb;
    }
    while (rN_ > 2 && now - rT_[rH_] > RATE_WINDOW_MS) { rH_ = (rH_ + 1) % RN; rN_--; }
  }
  bool rateValid() const { return on_ && millis() - onT_ >= RATE_VALID_MS && rN_ >= 10; }
  float rateNow() const {   // least-squares slope over the window, bar/s
    float st = 0, sv = 0, stt = 0, stv = 0; const uint32_t t0 = rT_[rH_];
    for (int k = 0; k < rN_; k++) {
      const int i = (rH_ + k) % RN; const float t = (rT_[i] - t0) / 1000.0f;
      st += t; sv += rV_[i]; stt += t * t; stv += t * rV_[i];
    }
    const float d = rN_ * stt - st * st;
    return d > 1e-6f ? (rN_ * stv - st * sv) / d : 0;
  }
  float offsetNow() const {
    if (!st_->offAuto) return st_->offManual;
    if (st_->c > 0 && rateValid()) return st_->c * max(0.0f, rateNow());
    return st_->offset;
  }

  // ---------- stop + learning ----------
  void markStop(uint32_t now, float sb, const char* ph) {
    dynStop_ = sb; estStop_ = est_; offStop_ = offsetNow();
    rateStop_ = rateValid() ? rateNow() : 0;
    pumpTime_ = (now - t0_) / 1000.0f;
    relay(false);
    wlog::ev("STOP", wlog::kv("phase", String(ph)) + wlog::kv("t_s", pumpTime_, 2) + wlog::kv("line_bar", dynStop_) +
                     wlog::kv("est_bar", estStop_) + wlog::kv("offset", offStop_) + wlog::kv("rate", rateStop_, 4) +
                     wlog::kv("margin", margin()));
  }
  static float ema(float old, float v) { return (1 - LEARN_ALPHA) * old + LEARN_ALPHA * v; }

  void learnMain(float pBefore, float p, bool firstPulse) {
    const Settings o = *st_;                                  // old values, for the log
    const float offMeas = dynStop_ - p;                       // what the offset really was
    float cMeas = 0, miss = 0;
    if (st_->offAuto && offMeas > 0 && offMeas < 2.0f) {
      st_->offset = st_->learned ? ema(st_->offset, offMeas) : offMeas;
      if (rateStop_ > 0.005f) {
        cMeas = offMeas / rateStop_;
        if (cMeas > 0.5f && cMeas < 60) st_->c = st_->c > 0 ? ema(st_->c, cMeas) : cMeas;
      }
    }
    const float r = pumpTime_ > 0.5f ? (p - pBefore) / pumpTime_ : 0;
    if (r > 0.001f && r < 2) st_->rate = st_->learned ? ema(st_->rate, r) : r;
    // until a top-up has been measured, estimate its gain from the main-run rate (spin-up costs ~15 %)
    if (!st_->topLearned) st_->kTop = 0.85f * st_->rate;
    if (!firstPulse && st_->learned) {                        // how well did the stop land?
      miss = p - estStop_;
      st_->bias = ema(st_->bias, miss);
      st_->dev  = ema(st_->dev, fabsf(miss - st_->bias));
    }
    st_->learned = true;
    st_->save();
    wlog::ev("LEARN_INF", wlog::kv("from", String(firstPulse ? "CAL_PULSE" : "MAIN")) +
             wlog::kv("off_meas", offMeas) + wlog::kv("c_meas", cMeas, 2) + wlog::kv("rate_meas", r, 4) + wlog::kv("miss", miss) +
             wlog::mv("offset", o.offset, st_->offset) + wlog::mv("c", o.c, st_->c, 2) + wlog::mv("rate", o.rate, st_->rate, 4) +
             wlog::mv("kTop", o.kTop, st_->kTop, 4) + wlog::mv("bias", o.bias, st_->bias) + wlog::mv("dev", o.dev, st_->dev));
  }
  void learnTopup(float pBefore, float p) {
    const float k = pumpTime_ > 0.2f ? (p - pBefore) / pumpTime_ : 0;
    const float old = st_->kTop;
    if (k > 0.001f && k < 2) { st_->kTop = st_->topLearned ? ema(st_->kTop, k) : k; st_->topLearned = true; st_->save(); }
    wlog::ev("LEARN_INF", wlog::kv("from", String("TOPUP")) + wlog::kv("k_meas", k, 4) + wlog::mv("kTop", old, st_->kTop, 4));
  }
  // deflate: p_abs(t) = p_abs0 * exp(-t / tau)  ->  tau = t / ln(p_abs_before / p_abs_after)
  void learnVent(float pBefore, float p) {
    const float t = ventMs_ / 1000.0f, old = st_->tauD;
    float tauM = 0;
    if (t > 0.5f && pBefore - p > 0.02f) {
      tauM = t / logf((pBefore + P_ATM) / (p + P_ATM));
      if (tauM > 5 && tauM < 3000) {
        // far off (> 25 %): move fast, otherwise the normal slow average
        const float a = fabsf(tauM - st_->tauD) > 0.25f * st_->tauD ? 0.6f : LEARN_ALPHA;
        st_->tauD = st_->learnedD ? (1 - a) * st_->tauD + a * tauM : tauM; st_->learnedD = true; st_->save();
      }
    }
    wlog::ev("LEARN_DEF", wlog::kv("t_s", t, 2) + wlog::kv("p_before", pBefore) + wlog::kv("p_after", p) +
             wlog::kv("aim", aim_) + wlog::kv("miss", p - aim_) + wlog::kv("tau_meas", tauM, 1) + wlog::mv("tauD", old, st_->tauD, 1));
  }

  void onStatic(float p) {
    const float before = pStatic_;
    pStatic_ = p;
    static const char* PH[] = {"START", "AFTER_CAL", "AFTER_MAIN", "AFTER_TOPUP", "AFTER_VENT"};
    wlog::ev("STATIC", wlog::kv("after", String(PH[(int)phase_])) + wlog::kv("p", p) + wlog::kv("settle_s", settleEl_ / 1000.0f, 1));
    if (test_ != Mode::NONE) return onTestStatic(p);
    if (phase_ == Phase::START)       p0_ = p;
    if (phase_ == Phase::AFTER_CAL)   learnMain(p0_, p, true);
    if (phase_ == Phase::AFTER_MAIN)  learnMain(before, p, false);
    if (phase_ == Phase::AFTER_TOPUP) learnTopup(before, p);
    if (phase_ == Phase::AFTER_VENT)  learnVent(before, p);

    if (phase_ == Phase::START) {
      if (fabsf(p - st_->target) <= START_BAND) { inBand_ = true; return done(); }   // close enough: do nothing
      mode_ = (DEFLATE && p > st_->target) ? Mode::DEF : Mode::INF;
      wlog::ev("MODE", wlog::kv("mode", modeName()) + wlog::kv("p0", p) + wlog::kv("target", st_->target, 2));
    }
    // tyres can not drop to ~0 within one aimed vent: the signal wire is gone
    if (mode_ == Mode::DEF && p < DEAD_BAR && p0_ > 0.5f) return fail(PErr::SENSOR);

    const float deficit = st_->target - p;
    if (fabsf(deficit) <= DONE_TOL) return done();
    if (deficit < 0) {                                         // above the target
      if (mode_ != Mode::DEF || vents_ >= MAX_VENTS) return done();   // inflate build: never vents
      return vent(p);
    }
    // below the target: pump (in DEF mode this is the hybrid fix of an overshoot, top-ups only)
    if (mode_ == Mode::INF && phase_ == Phase::START && st_->offAuto && !st_->learned) {   // first auto run ever
      relay(true); state_ = PState::CAL_PULSE; t0_ = millis(); return;
    }
    // main run when far away; for small deficits a timed top-up is enough
    if (mode_ == Mode::INF && (phase_ == Phase::START || phase_ == Phase::AFTER_CAL) && deficit > 0.25f) {
      relay(true); state_ = PState::MAIN; t0_ = millis(); leakRef_ = p; leakT_ = t0_; return;
    }
    if (topups_ >= MAX_TOPUPS) return done();
    // aim a little short: a top-up that lands just under the target is done, one over it costs a vent (or can not be undone)
    float s = (deficit - 0.4f * DONE_TOL) / max(0.005f, st_->kTop);
    s = constrain(s, TOPUP_MIN_S, TOPUP_MAX_S);
    topupMs_ = (uint32_t)(s * 1000); topups_++;
    relay(true); state_ = PState::TOPUP; t0_ = millis();
  }

  // menu LEARN: static measurements of the test
  void onTestStatic(float p) {
    if (phase_ == Phase::START) {
      p0_ = p; mode_ = test_;
      if (test_ == Mode::DEF) {
        if (p < LEARN_DEF_MIN_BAR) return fail(PErr::LOW_P);
        aim_ = 0; ventMs_ = LEARN_DEF_MS;
        relay(false); valve(true); state_ = PState::VENT; t0_ = millis();
      } else {
        relay(true); state_ = PState::CAL_PULSE; t0_ = millis();
      }
      return;
    }
    if (phase_ == Phase::AFTER_VENT) {
      if (p < DEAD_BAR && p0_ > 0.5f) return fail(PErr::SENSOR);
      const float old = st_->tauD, t = ventMs_ / 1000.0f;
      float tau = 0;
      if (p0_ - p > 0.01f) tau = t / logf((p0_ + P_ATM) / (p + P_ATM));
      testOk_ = tau > 5 && tau < 3000;
      if (testOk_) { st_->tauD = tau; st_->learnedD = true; st_->save(); testRes_ = tau; }
      wlog::ev("LEARN_DEF_TEST", wlog::kv("p_before", p0_) + wlog::kv("p_after", p) + wlog::kv("t_s", t, 1) +
               wlog::kv("tau_meas", tau, 1) + wlog::kv("ok", testOk_ ? "1" : "0") + wlog::mv("tau", old, st_->tauD, 1));
      return done();
    }
    if (phase_ == Phase::AFTER_CAL) {
      const float old = st_->rate;
      learnMain(p0_, p, true);
      testOk_ = p > p0_ + 0.005f; testRes_ = st_->rate;
      wlog::ev("LEARN_INF_TEST", wlog::kv("p_before", p0_) + wlog::kv("p_after", p) + wlog::kv("t_s", pumpTime_, 1) +
               wlog::kv("ok", testOk_ ? "1" : "0") + wlog::mv("rate", old, st_->rate, 4));
      return done();
    }
    done();
  }

  // one valve opening, timed from the exponential model
  void vent(float p) {
    // a vent below the target costs a compressor top-up, so always land a little above:
    // learned: stop 15 % of the drop short (at least 0.02 bar), the rest is one short vent; first time ever: only 60 % of the way
    aim_ = st_->learnedD ? st_->target + max(0.4f * DONE_TOL, 0.15f * (p - st_->target))
                         : p - VENT_FIRST_FRAC * (p - st_->target);
    float s = st_->tauD * logf((p + P_ATM) / (aim_ + P_ATM));
    s = constrain(s, VENT_MIN_S, VENT_MAX_S);
    ventMs_ = (uint32_t)(s * 1000);
    wlog::ev("VENT", wlog::kv("p", p) + wlog::kv("aim", aim_) + wlog::kv("tauD", st_->tauD, 1) + wlog::kv("t_s", s, 2));
    relay(false); valve(true); state_ = PState::VENT; t0_ = millis();
  }

  static constexpr int RN = 32;
  uint32_t rT_[RN] = {0}; float rV_[RN] = {0}; int rH_ = 0, rN_ = 0;

  Sensor* s_ = nullptr; Settings* st_ = nullptr;
  PState state_ = PState::IDLE; PErr err_ = PErr::NONE; Phase phase_ = Phase::START; Mode mode_ = Mode::NONE;
  Mode test_ = Mode::NONE; bool testOk_ = false; float testRes_ = 0;
  uint32_t pulseT_ = 0, pulseLen_ = OUTPUT_PULSE_MS; bool pulseValve_ = false; float pulseBefore_ = 0;
  bool on_ = false, vOn_ = false, inBand_ = false;
  uint32_t t0_ = 0, runStart_ = 0, endT_ = 0, topupMs_ = 0, ventMs_ = 0, leakT_ = 0, onT_ = 0, tS_ = 0, settleEl_ = 0;
  float pStatic_ = 0, p0_ = 0, est_ = 0, pumpTime_ = 0, leakRef_ = 0, aim_ = 0;
  float dynStop_ = 0, estStop_ = 0, offStop_ = 0, rateStop_ = 0, sS_ = 0, slope_ = 99;
  int topups_ = 0, starts_ = 0, vents_ = 0;
  Averager avg_;
};
