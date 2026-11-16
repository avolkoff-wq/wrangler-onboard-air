// Inflation state machine, stage 1: K1 switches the compressor relay K2.
//
//  start -> SETTLE+MEASURE (static p0)
//        -> [first ever run] CAL_PULSE 15 s -> SETTLE+MEASURE -> learn offset/rate
//        -> MAIN: compressor on, tyres ~= sensor - offset, stop at target - 0.1
//        -> SETTLE+MEASURE (static) -> learn offset/rate
//        -> TOPUP (time = (target - p)/rate) -> SETTLE+MEASURE ... up to 3 times
//        -> DONE
//  Any fault or a button press -> compressor off.
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
    err_ = PErr::NONE; topups_ = 0; runStart_ = millis();
    goSettle(Phase::START);
  }
  void stop() { relay(false); state_ = PState::IDLE; }
  bool busy() const { return state_ != PState::IDLE && state_ != PState::DONE && state_ != PState::ERROR; }
  void clearResult() { if (!busy()) state_ = PState::IDLE; }

  PState state() const { return state_; }
  PErr   error() const { return err_; }
  bool   compressorOn() const { return on_; }
  float  staticBar() const { return pStatic_; }       // last static measurement
  float  estimate() const { return est_; }            // tyres now (estimated while pumping)
  bool   estimating() const { return on_; }

  void update() {
    const uint32_t now = millis();
    const float sb = s_->bar(st_->calA, st_->calB);

    // ---- protections, always ----
    if (busy()) {
      if (s_->fault())                         return fail(PErr::SENSOR);
      if (now - runStart_ > RUN_TIMEOUT_MS)    return fail(PErr::TIMEOUT);
      if (on_ && sb - st_->offset > TARGET_MAX) return fail(PErr::OVER);
      if (!on_ && sb > TARGET_MAX + 0.05f)     return fail(PErr::OVER);
    }
    est_ = on_ ? sb - st_->offset : sb;

    switch (state_) {
      case PState::SETTLE:
        if (now - t0_ >= SETTLE_MS) { state_ = PState::MEASURE; t0_ = now; avg_.start(); }
        break;

      case PState::MEASURE:
        avg_.add(sb);
        if (now - t0_ >= MEASURE_MS) onStatic(avg_.value());
        break;

      case PState::CAL_PULSE:
        dyn_ = sb;
        if (now - t0_ >= CAL_PULSE_MS) { pumpTime_ = (now - t0_) / 1000.0f; relay(false); goSettle(Phase::AFTER_CAL); }
        break;

      case PState::MAIN:
        dyn_ = sb;
        if (est_ > leakRef_ + LEAK_MIN_RISE) { leakRef_ = est_; leakT_ = now; }
        if (now - leakT_ > LEAK_WINDOW_MS) return fail(PErr::LEAK);
        if (est_ >= st_->target - STOP_MARGIN) { pumpTime_ = (now - t0_) / 1000.0f; relay(false); goSettle(Phase::AFTER_MAIN); }
        break;

      case PState::TOPUP:
        if (now - t0_ >= topupMs_) { relay(false); goSettle(Phase::AFTER_TOPUP); }
        break;

      default: break;
    }
  }

 private:
  void relay(bool on) { on_ = on; digitalWrite(PIN_K1, on ? HIGH : LOW); }
  void fail(PErr e) { relay(false); err_ = e; state_ = PState::ERROR; }
  void goSettle(Phase ph) { phase_ = ph; state_ = PState::SETTLE; t0_ = millis(); }

  void learn(float pBefore, float pAfter) {
    const float off  = dyn_ - pAfter;
    const float rate = pumpTime_ > 0.5f ? (pAfter - pBefore) / pumpTime_ : st_->rate;
    if (off > 0 && off < 2.0f)    st_->offset = st_->learned ? (1 - LEARN_ALPHA) * st_->offset + LEARN_ALPHA * off : off;
    if (rate > 0.001f && rate < 2) st_->rate   = st_->learned ? (1 - LEARN_ALPHA) * st_->rate   + LEARN_ALPHA * rate : rate;
    st_->learned = true;
    st_->save();
  }

  void onStatic(float p) {
    const float before = pStatic_;
    pStatic_ = p;
    if (phase_ == Phase::START) p0_ = p;

    if (phase_ == Phase::AFTER_CAL)  learn(p0_, p);
    if (phase_ == Phase::AFTER_MAIN) learn(before, p);

    if (p >= st_->target - DONE_TOL) { state_ = PState::DONE; return; }

    if (phase_ == Phase::START && !st_->learned) {           // first ever run
      relay(true); state_ = PState::CAL_PULSE; t0_ = millis(); return;
    }
    if (phase_ == Phase::START || phase_ == Phase::AFTER_CAL) {
      if (st_->target - p > STOP_MARGIN + 0.05f) {             // far: main run without pauses
        relay(true); state_ = PState::MAIN; t0_ = millis();
        leakRef_ = p; leakT_ = t0_; return;
      }
    }
    if (topups_ >= MAX_TOPUPS) { state_ = PState::DONE; return; }  // close enough, stop trying
    float s = (st_->target - p) / st_->rate;
    s = constrain(s, TOPUP_MIN_S, TOPUP_MAX_S);
    topupMs_ = (uint32_t)(s * 1000); topups_++;
    relay(true); state_ = PState::TOPUP; t0_ = millis();
  }

  Sensor* s_ = nullptr; Settings* st_ = nullptr;
  PState state_ = PState::IDLE; PErr err_ = PErr::NONE; Phase phase_ = Phase::START;
  bool on_ = false;
  uint32_t t0_ = 0, runStart_ = 0, topupMs_ = 0, leakT_ = 0;
  float pStatic_ = 0, p0_ = 0, dyn_ = 0, est_ = 0, pumpTime_ = 0, leakRef_ = 0;
  int topups_ = 0;
  Averager avg_;
};
