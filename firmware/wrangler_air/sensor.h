// Pressure sensor 0-3.3 V straight on G13; in DEMO a virtual 4-tyre system answers the outputs instead
#pragma once
#include <Arduino.h>
#include "config.h"
#include <driver/gpio.h>

class Sensor {
 public:
  void begin() {
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_SENSOR, ADC_11db);
    analogReadMilliVolts(PIN_SENSOR);                   // attach the pin to the ADC first...
    gpio_pulldown_en((gpio_num_t)PIN_SENSOR);           // ...then pull it down: a broken signal wire reads 0 V, not noise
    gpio_pullup_dis((gpio_num_t)PIN_SENSOR);
  }
  // call every loop: keeps a short running average (~50 ms)
  void update() {
    if (BENCH_SIM) vG1_ = simTyre();
    else {
      float mv = 0;
      for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(PIN_SENSOR);
      vG1_ = (mv / 8.0f) / 1000.0f;
    }
    filt_ = (filt_ < 0) ? vG1_ : filt_ + 0.2f * (vG1_ - filt_);
  }
  float vG1() const { return filt_; }
  void  resetFilter() { filt_ = -1; }                   // after DEMO on / off
  static void simReset() { simP = SIM_START_BAR; simFlow = 0; simLast = 0; }
  bool  fault() const { return filt_ < G1_MIN_V || filt_ > G1_MAX_V; }
  // raw bar from the datasheet line, before calibration
  float rawBar() const { return (filt_ * DIVIDER - SENSOR_V0) / SENSOR_SPAN * SENSOR_FS; }
  float bar(float a, float b) const { return a * rawBar() + b; }

 public:
  // Virtual 4-tyre system. Flow ramps up/down with the compressor (spin-up),
  // falls as the tyres fill, and the sensor reads tyres + SIM_C * rise rate.
  static inline float simP = SIM_START_BAR;  // real tyre pressure, bar (harness may set it)
  static inline float simFlow = 0;           // 0..1 compressor flow
  static inline uint32_t simLast = 0;
 private:
  float simTyre() {
    const uint32_t now = millis();
    if (!simLast) simLast = now;
    const float dt = (now - simLast) / 1000.0f; simLast = now;
    const bool on = digitalRead(PIN_K1) == HIGH;
    simFlow += ((on ? 1.0f : 0.0f) - simFlow) * min(1.0f, dt / SIM_SPINUP_S);
    const float rise = SIM_RATE * simFlow * max(0.0f, 1.0f - simP / SIM_PMAX);
    simP += rise * dt;
    // deflate valve (K4): choked outflow, absolute pressure falls exponentially; the line reads low while it flows
    const bool vent = PIN_K4 >= 0 && digitalRead(PIN_K4) == HIGH;
    const float fall = vent ? (simP + P_ATM) / SIM_TAU_D : 0;
    simP = max(0.0f, simP - fall * dt);
    const float noise = SIM_NOISE * ((float)random(-1000, 1000) / 1000.0f);
    const float reading = max(0.0f, simP + SIM_C * rise - SIM_C_D * fall) + noise;
    return (reading / SENSOR_FS * SENSOR_SPAN + SENSOR_V0) / DIVIDER;
  }
 private:
  float vG1_ = 0, filt_ = -1;
};

// Averages bar readings over a window (static measurement with the compressor off)
class Averager {
 public:
  void start() { sum_ = 0; n_ = 0; }
  void add(float v) { sum_ += v; n_++; }
  float value() const { return n_ ? sum_ / n_ : 0; }
 private:
  float sum_ = 0; int n_ = 0;
};

// AUX1 voltage through the 0-25 V module on G15. Tells the engine state before the compressor starts.
class Supply {
 public:
  void begin() { if (PIN_VBAT >= 0) analogSetPinAttenuation(PIN_VBAT, ADC_11db); }
  void update() {
    if (PIN_VBAT < 0) return;
    float mv = 0;
    for (int i = 0; i < 4; i++) mv += analogReadMilliVolts(PIN_VBAT);
    const float v = mv / 4.0f / 1000.0f * VBAT_DIV;
    filt_ = (filt_ < 0) ? v : filt_ + 0.05f * (v - filt_);    // slow: ~0.5 s, ignores cranking dips and relay clicks
  }
  bool  fitted() const { return PIN_VBAT >= 0 && filt_ >= VBAT_MIN_V; }
  float volts() const { return fitted() ? filt_ : 0; }
  bool  engineOn() const { return fitted() && filt_ >= ENGINE_ON_V; }
  // line shown before inflating: "" engine runs, "ENGINE OFF" measured off, "ENGINE ON?" no module
  const char* engineText() const { return !fitted() ? "ENGINE ON?" : engineOn() ? "" : "ENGINE OFF"; }

 private:
  float filt_ = -1;
};

enum class Eng { RUN, OFF, NOMOD, NOCHECK };   // engine state before inflating (wrangler_air.ino engine())
