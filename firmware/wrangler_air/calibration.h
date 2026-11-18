// 4-point calibration against an analogue gauge: 0, ~1, ~2, ~3 bar.
// Point 0: hose open to air. Points 1-3: the program pumps to about the point,
// stops, waits, then the driver dials in what the gauge shows and confirms.
// Result: least-squares line calibrated = a * raw + b.
#pragma once
#include <Arduino.h>
#include "config.h"
#include "sensor.h"
#include "storage.h"
#include "log.h"

enum class CState { OFF, ZERO_WAIT, PUMPING, SETTLING, ENTER, DONE, FAILED };

class Calibration {
 public:
  static constexpr int N = 4;
  const float points[N] = {0.0f, 1.0f, 2.0f, 3.0f};

  void begin(Sensor* s, Settings* st) { s_ = s; st_ = st; }
  void start() { i_ = 0; state_ = CState::ZERO_WAIT; relay(false); tStart_ = millis(); }
  uint32_t totalMs() const { return millis() - tStart_; }
  void abort() { relay(false); state_ = CState::OFF; }
  bool active() const { return state_ != CState::OFF; }
  CState state() const { return state_; }
  int    index() const { return i_; }
  float  entry() const { return entry_; }
  float  fitErr() const { return fitErr_; }   // worst point vs the fitted line, bar
  float  rawNow() const { return s_->rawBar(); }
  uint32_t phaseMs() const { return millis() - t0_; }
  static constexpr uint32_t SETTLE_TOTAL_MS = SETTLE_MS + 1000;
  void   turn(int steps) { if (state_ == CState::ENTER) entry_ = max(0.0f, entry_ + steps * 0.05f); }

  // button press: confirm current step
  void press() {
    switch (state_) {
      case CState::ZERO_WAIT: raw_[0] = s_->rawBar(); ref_[0] = 0; logPoint(0); i_ = 1; pumpTo(); break;
      case CState::ENTER:     ref_[i_] = entry_; logPoint(i_); if (++i_ < N) pumpTo(); else finish(); break;
      case CState::DONE: case CState::FAILED: state_ = CState::OFF; break;
      default: break;
    }
  }

  void update() {
    const uint32_t now = millis();
    if (state_ == CState::PUMPING) {
      if (s_->fault() || now - t0_ > 180000UL) { relay(false); state_ = CState::FAILED; wlog::ev("CAL_FAIL", "reason=sensor_or_timeout;"); return; }
      if (now - t0_ > DEAD_AFTER_MS && s_->rawBar() < DEAD_BAR) { relay(false); state_ = CState::FAILED; return; }  // wire broken
      // stop a bit before the point using the raw line minus the default offset
      if (s_->rawBar() - DEFAULT_OFFSET >= points[i_] - 0.05f || s_->rawBar() > TARGET_MAX + 0.6f) {
        relay(false); state_ = CState::SETTLING; t0_ = now;
      }
    } else if (state_ == CState::SETTLING && now - t0_ >= SETTLE_TOTAL_MS) {
      raw_[i_] = s_->rawBar();
      entry_ = roundf(raw_[i_] * 20) / 20.0f;   // start the dial at the raw value
      state_ = CState::ENTER;
    }
  }

 private:
  void relay(bool on) { if (on && PIN_K4 >= 0) digitalWrite(PIN_K4, LOW); digitalWrite(PIN_K1, on ? HIGH : LOW); }
  void logPoint(int k) { wlog::ev("CAL_POINT", wlog::kv("n", (float)k, 0) + wlog::kv("raw", raw_[k]) + wlog::kv("gauge", ref_[k], 2)); }
  void pumpTo() { relay(true); state_ = CState::PUMPING; t0_ = millis(); }
  void finish() {
    float sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (int k = 0; k < N; k++) { sx += raw_[k]; sy += ref_[k]; sxx += raw_[k] * raw_[k]; sxy += raw_[k] * ref_[k]; }
    const float d = N * sxx - sx * sx;
    if (fabsf(d) < 1e-6f) { state_ = CState::FAILED; wlog::ev("CAL_FAIL", "reason=flat;"); return; }
    const float a = (N * sxy - sx * sy) / d;
    const float b = (sy - a * sx) / N;
    if (a < 0.7f || a > 1.3f || fabsf(b) > 0.5f) { state_ = CState::FAILED; wlog::ev("CAL_FAIL", wlog::kv("a", a, 4) + wlog::kv("b", b)); return; }  // implausible
    fitErr_ = 0;
    for (int k = 0; k < N; k++) fitErr_ = max(fitErr_, fabsf(a * raw_[k] + b - ref_[k]));
    wlog::ev("CAL_DONE", wlog::mv("calA", st_->calA, a, 4) + wlog::mv("calB", st_->calB, b) + wlog::kv("fit_err", fitErr_));
    st_->calA = a; st_->calB = b; st_->save();
    state_ = CState::DONE;
  }

  Sensor* s_ = nullptr; Settings* st_ = nullptr;
  CState state_ = CState::OFF;
  int i_ = 0; uint32_t t0_ = 0, tStart_ = 0;
  float raw_[N] = {0}, ref_[N] = {0}, entry_ = 0, fitErr_ = 0;
};
