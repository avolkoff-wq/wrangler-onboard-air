// Inflation state machine, stage 1: K1 switches the compressor relay K2.
// Goal: reach the target with as few compressor starts as possible.
//
//  start -> SETTLE+MEASURE (static p0)
//        -> [first auto run] CAL_PULSE 15 s -> SETTLE+MEASURE -> learn
//        -> MAIN: compressor on, tyres = sensor - offset, stop at target - margin
//        -> SETTLE+MEASURE -> learn offset, miss, rate
//        -> TOPUP (time = deficit / kTop) -> SETTLE+MEASURE -> learn kTop ... up to 3 times
//        -> DONE
//
// Offset while pumping (sensor reads high because air is flowing):
//   AUTO   offset = c * rise rate   (c learned; scalar learned offset until rate is valid)
//   MANUAL offset = fixed value from the menu
// Settling before a static measurement:
//   AUTO   wait until the reading is steady (< 0.005 bar/s), 1..6 s
//   MANUAL fixed time from the menu
// Stop margin adapts to how well past stops landed (bias and spread of the miss).
#pragma once
#include <Arduino.h>
#include "config.h"
#include "sensor.h"
#include "storage.h"

enum class PState { IDLE, SETTLE, MEASURE, CAL_PULSE, MAIN, TOPUP, DONE, ERROR };
enum class PErr   { NONE, SENSOR, OVER, LEAK, TIMEOUT };
enum class Phase  { START, AFTER_CAL, AFTER_MAIN, AFTER_TOPUP };

class Pump {
 public:
  void begin(Sensor* s, Settings* st) { s_ = s; st_ = st; relay(false); }

  void start() {
    if (busy()) return;
    err_ = PErr::NONE; topups_ = 0; starts_ = 0; runStart_ = millis();
    goSettle(Phase::START);
  }
  void stop() { relay(false); state_ = PState::IDLE; endT_ = millis(); }
  bool busy() const { return state_ != PState::IDLE && state_ != PState::DONE && state_ != PState::ERROR; }
  void clearResult() { if (!busy()) state_ = PState::IDLE; }

  PState state() const { return state_; }
  PErr   error() const { return err_; }
  bool   compressorOn() const { return on_; }
  float  staticBar() const { return pStatic_; }
  float  estimate() const { return est_; }
  bool   estimating() const { return on_; }
  int    starts() const { return starts_; }      // compressor starts in the last run
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

    if (busy()) {
      if (s_->fault())                       return fail(PErr::SENSOR);
      if (now - runStart_ > RUN_TIMEOUT_MS)  return fail(PErr::TIMEOUT);
      if (!on_ && sb > TARGET_MAX + 0.05f)   return fail(PErr::OVER);
    }
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
          state_ = PState::MEASURE; t0_ = now; avg_.start();
        }
      } break;

      case PState::MEASURE:
        avg_.add(sb);
        if (now - t0_ >= MEASURE_MS) onStatic(avg_.value());
        break;

      case PState::CAL_PULSE:
        if (now - t0_ >= CAL_PULSE_MS) { markStop(now, sb); goSettle(Phase::AFTER_CAL); }
        break;

      case PState::MAIN:
        if (est_ > leakRef_ + LEAK_MIN_RISE) { leakRef_ = est_; leakT_ = now; }
        if (now - leakT_ > LEAK_WINDOW_MS) return fail(PErr::LEAK);
        // aim: real ~ est + bias; stop early by the margin
        if (est_ + (st_->learned ? st_->bias : 0) >= st_->target - margin()) { markStop(now, sb); goSettle(Phase::AFTER_MAIN); }
        break;

      case PState::TOPUP:
        if (now - t0_ >= topupMs_) { relay(false); pumpTime_ = topupMs_ / 1000.0f; goSettle(Phase::AFTER_TOPUP); }
        break;

      default: break;
    }
  }

 private:
  // ---------- outputs ----------
  void relay(bool on) {
    if (on && !on_) { starts_++; onT_ = millis(); rN_ = 0; }
    on_ = on; digitalWrite(PIN_K1, on ? HIGH : LOW);
  }
  void fail(PErr e) { relay(false); err_ = e; state_ = PState::ERROR; endT_ = millis(); }
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
  void markStop(uint32_t now, float sb) {
    dynStop_ = sb; estStop_ = est_; offStop_ = offsetNow();
    rateStop_ = rateValid() ? rateNow() : 0;
    pumpTime_ = (now - t0_) / 1000.0f;
    relay(false);
  }
  static float ema(float old, float v) { return (1 - LEARN_ALPHA) * old + LEARN_ALPHA * v; }

  void learnMain(float pBefore, float p, bool firstPulse) {
    const float offMeas = dynStop_ - p;                       // what the offset really was
    if (st_->offAuto && offMeas > 0 && offMeas < 2.0f) {
      st_->offset = st_->learned ? ema(st_->offset, offMeas) : offMeas;
      if (rateStop_ > 0.005f) {
        const float cMeas = offMeas / rateStop_;
        if (cMeas > 0.5f && cMeas < 60) st_->c = st_->c > 0 ? ema(st_->c, cMeas) : cMeas;
      }
    }
    const float r = pumpTime_ > 0.5f ? (p - pBefore) / pumpTime_ : 0;
    if (r > 0.001f && r < 2) st_->rate = st_->learned ? ema(st_->rate, r) : r;
    // until a top-up has been measured, estimate its gain from the main-run rate (spin-up costs ~15 %)
    if (!st_->topLearned) st_->kTop = 0.85f * st_->rate;
    if (!firstPulse && st_->learned) {                        // how well did the stop land?
      const float miss = p - estStop_;
      st_->bias = ema(st_->bias, miss);
      st_->dev  = ema(st_->dev, fabsf(miss - st_->bias));
    }
    st_->learned = true;
    st_->save();
  }
  void learnTopup(float pBefore, float p) {
    const float k = pumpTime_ > 0.2f ? (p - pBefore) / pumpTime_ : 0;
    if (k > 0.001f && k < 2) { st_->kTop = st_->topLearned ? ema(st_->kTop, k) : k; st_->topLearned = true; st_->save(); }
  }

  void onStatic(float p) {
    const float before = pStatic_;
    pStatic_ = p;
    if (phase_ == Phase::START) p0_ = p;
    if (phase_ == Phase::AFTER_CAL)   learnMain(p0_, p, true);
    if (phase_ == Phase::AFTER_MAIN)  learnMain(before, p, false);
    if (phase_ == Phase::AFTER_TOPUP) learnTopup(before, p);

    const float deficit = st_->target - p;
    if (deficit <= DONE_TOL) { state_ = PState::DONE; endT_ = millis(); return; }

    if (phase_ == Phase::START && st_->offAuto && !st_->learned) {   // first auto run ever
      relay(true); state_ = PState::CAL_PULSE; t0_ = millis(); return;
    }
    // main run when far away; for small deficits a timed top-up is enough
    if ((phase_ == Phase::START || phase_ == Phase::AFTER_CAL) && deficit > 0.25f) {
      relay(true); state_ = PState::MAIN; t0_ = millis(); leakRef_ = p; leakT_ = t0_; return;
    }
    if (topups_ >= MAX_TOPUPS) { state_ = PState::DONE; endT_ = millis(); return; }
    // aim a little short: a top-up that lands just under the target is done, one over it cannot be undone
    float s = (deficit - 0.4f * DONE_TOL) / max(0.005f, st_->kTop);
    s = constrain(s, TOPUP_MIN_S, TOPUP_MAX_S);
    topupMs_ = (uint32_t)(s * 1000); topups_++;
    relay(true); state_ = PState::TOPUP; t0_ = millis();
  }

  static constexpr int RN = 32;
  uint32_t rT_[RN] = {0}; float rV_[RN] = {0}; int rH_ = 0, rN_ = 0;

  Sensor* s_ = nullptr; Settings* st_ = nullptr;
  PState state_ = PState::IDLE; PErr err_ = PErr::NONE; Phase phase_ = Phase::START;
  bool on_ = false;
  uint32_t t0_ = 0, runStart_ = 0, endT_ = 0, topupMs_ = 0, leakT_ = 0, onT_ = 0, tS_ = 0;
  float pStatic_ = 0, p0_ = 0, est_ = 0, pumpTime_ = 0, leakRef_ = 0;
  float dynStop_ = 0, estStop_ = 0, offStop_ = 0, rateStop_ = 0, sS_ = 0, slope_ = 99;
  int topups_ = 0, starts_ = 0;
  Averager avg_;
};
