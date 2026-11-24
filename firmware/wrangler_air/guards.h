// Idle safety checks, moved out of loop() so the desk test can replay them (review item 3).
// update() is called every loop pass; it only decides, the caller logs and acts (pump.valveStuck / pumpStuck /
// lowVoltage, cal.fail). Logic and thresholds are the 0.5.2 ones, unchanged.
//   VALVE CHECK  idle, both outputs off, the line falls steadily (valve or its relay stuck open). Only with VALVE.
//   PUMP CHECK   idle, both outputs off, the line keeps rising (AUX2 bypass, a welded relay contact), or stays above
//                PCHK_HIGH for 3 s.
//   both: one alarm per episode, re-armed after the line was steady for PCHK_REARM_S, or by a run.
//   LOW VOLTAGE  compressor on (run, pulse or calibration), AUX1 below LOW_V for LOW_V_MS.
#pragma once
#include <Arduino.h>
#include "config.h"

struct GuardIn {
  uint32_t now;
  bool  idle;          // nothing of ours drives K1 / K4: no run, no pulse, no calibration, no Wi-Fi, no DEMO
  bool  fault;         // sensor fault
  float p;             // calibrated line pressure, bar
  bool  vchkOn;        // menu VALVE CHECK and a valve fitted
  bool  pchkOn;        // menu PUMP CHECK
  bool  pumpBusy;      // a run is going (re-arms both checks)
  bool  calActive;     // calibration is going (re-arms PUMP CHECK)
  bool  compressorOn;  // K1 on: run, pulse or calibration pumping
  bool  voltOk;        // VOLT CHECK on and a module fitted
  float volts;         // AUX1
};

enum class GuardEv { NONE, VALVE_STUCK, PUMP_STUCK, LOW_V };

class Guards {
 public:
  GuardEv update(const GuardIn& in) {
    if (valveCheck(in)) return GuardEv::VALVE_STUCK;
    if (pumpCheck(in))  return GuardEv::PUMP_STUCK;
    if (lowVolt(in))    return GuardEv::LOW_V;
    return GuardEv::NONE;
  }
  // details of the last event, for the log
  float rate() const { return vRate_; }    // VALVE_STUCK: fall, bar/s
  float rise() const { return pRise_; }    // PUMP_STUCK: rise over the window, bar
  bool  high() const { return pHigh_; }    // PUMP_STUCK: above PCHK_HIGH for 3 s

 private:
  bool valveCheck(const GuardIn& in) {
    if (!in.idle || !in.vchkOn || in.fault) { vN_ = 0; vQuiet_ = in.now; if (in.pumpBusy) vFired_ = false; return false; }
    if (in.now - vQuiet_ <= VCHK_QUIET_MS || in.now - vLast_ < 1000) return false;
    vLast_ = in.now;
    push(vHist_, vN_, VCHK_WINDOW_S, in.p);
    if (vN_ <= VCHK_WINDOW_S) return false;
    int down = 0; float maxStep = 0;
    for (int i = 1; i <= VCHK_WINDOW_S; i++) { const float d = vHist_[i - 1] - vHist_[i]; if (d > 0) down++; maxStep = max(maxStep, d); }
    const float rate = (vHist_[0] - vHist_[VCHK_WINDOW_S]) / VCHK_WINDOW_S;
    if (vFired_) {   // wait until it stops falling
      vSteady_ = (fabsf(vHist_[VCHK_WINDOW_S] - vHist_[VCHK_WINDOW_S - 1]) < 0.01f) ? vSteady_ + 1 : 0;
      if (vSteady_ >= PCHK_REARM_S) { vFired_ = false; vSteady_ = 0; }
    }
    if (!vFired_ && vHist_[0] > VCHK_MIN_BAR && rate > VCHK_MIN && rate < VCHK_MAX && maxStep < 0.15f && down >= VCHK_WINDOW_S * 2 / 3) {
      vFired_ = true; vSteady_ = 0; vRate_ = rate;
      return true;
    }
    return false;
  }

  bool pumpCheck(const GuardIn& in) {
    if (!in.idle || !in.pchkOn || in.fault) { pN_ = 0; pQuiet_ = in.now; if (in.pumpBusy || in.calActive) pFired_ = false; return false; }
    if (in.now - pQuiet_ <= VCHK_QUIET_MS || in.now - pLast_ < 1000) return false;
    pLast_ = in.now;
    push(pHist_, pN_, PCHK_WINDOW_S, in.p);
    const int last = pN_ - 1;
    // "above PCHK_HIGH for 3 s" = the last 4 one-second samples, checked as soon as there are 4
    bool high = pN_ >= 4;
    for (int i = last - 3; high && i <= last; i++) high = pHist_[i] > PCHK_HIGH;
    if (pFired_ && pN_ >= 2) {   // wait until it stops rising
      pSteady_ = (fabsf(pHist_[last] - pHist_[last - 1]) < 0.01f) ? pSteady_ + 1 : 0;
      if (pSteady_ >= PCHK_REARM_S) { pFired_ = false; pSteady_ = 0; }
    }
    bool rising = false; float rise = 0;
    if (pN_ > PCHK_WINDOW_S) {
      int up = 0; float maxStep = 0;
      for (int i = 1; i <= PCHK_WINDOW_S; i++) { const float d = pHist_[i] - pHist_[i - 1]; if (d > 0.005f) up++; maxStep = max(maxStep, d); }
      rise = pHist_[PCHK_WINDOW_S] - pHist_[0];
      rising = rise > PCHK_RISE && up >= PCHK_UP && maxStep < 0.8f * rise;
    }
    if (!pFired_ && (high || rising)) {
      pFired_ = true; pSteady_ = 0; pRise_ = rise; pHigh_ = high;
      return true;
    }
    return false;
  }

  bool lowVolt(const GuardIn& in) {
    const bool low = in.voltOk && in.compressorOn && in.volts < LOW_V;
    if (!low) { lowT_ = 0; return false; }
    if (!lowT_) { lowT_ = in.now ? in.now : 1; return false; }
    if (in.now - lowT_ < LOW_V_MS) return false;
    lowT_ = 0;
    return true;
  }

  // one sample per second into a window of w+1 values, oldest first
  static void push(float* h, int& n, int w, float p) {
    if (n <= w) h[n++] = p; else { memmove(h, h + 1, w * sizeof(float)); h[w] = p; }
  }

  float vHist_[VCHK_WINDOW_S + 1] = {0}; int vN_ = 0; uint32_t vLast_ = 0, vQuiet_ = 0, vSteady_ = 0; bool vFired_ = false;
  float pHist_[PCHK_WINDOW_S + 1] = {0}; int pN_ = 0; uint32_t pLast_ = 0, pQuiet_ = 0, pSteady_ = 0; bool pFired_ = false;
  uint32_t lowT_ = 0;
  float vRate_ = 0, pRise_ = 0; bool pHigh_ = false;
};
