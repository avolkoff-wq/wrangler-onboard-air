// Pressure sensor: 0.5-4.5 V on G1 through a 10k/20k divider, or (SENSOR_DIRECT) 0-3.3 V straight on G13
#pragma once
#include <Arduino.h>
#include "config.h"
#include <driver/gpio.h>

class Sensor {
 public:
  void begin() {
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_SENSOR, ADC_11db);
#if SENSOR_DIRECT && !BENCH_SIM
    analogReadMilliVolts(PIN_SENSOR);                   // attach the pin to the ADC first...
    gpio_pulldown_en((gpio_num_t)PIN_SENSOR);           // ...then pull it down: a broken signal wire reads 0 V, not noise
    gpio_pullup_dis((gpio_num_t)PIN_SENSOR);
#endif
  }
  // call every loop: keeps a short running average (~50 ms)
  void update() {
#if BENCH_SIM
    vG1_ = simTyre();
#else
    float mv = 0;
    for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(PIN_SENSOR);
    vG1_ = (mv / 8.0f) / 1000.0f;
#endif
    filt_ = (filt_ < 0) ? vG1_ : filt_ + 0.2f * (vG1_ - filt_);
  }
  float vG1() const { return filt_; }
  bool  fault() const { return filt_ < G1_MIN_V || filt_ > G1_MAX_V; }
  // raw bar from the datasheet line, before calibration
  float rawBar() const { return (filt_ * DIVIDER - SENSOR_V0) / SENSOR_SPAN * SENSOR_FS; }
  float bar(float a, float b) const { return a * rawBar() + b; }

 private:
#if BENCH_SIM
 public:
  // Virtual 4-tyre system. Flow ramps up/down with the compressor (spin-up),
  // falls as the tyres fill, and the sensor reads tyres + SIM_C * rise rate.
  static inline float simP = SIM_START_BAR;  // real tyre pressure, bar (harness may set it)
  static inline float simFlow = 0;           // 0..1 compressor flow
 private:
  float simTyre() {
    static uint32_t last = millis();
    const uint32_t now = millis();
    const float dt = (now - last) / 1000.0f; last = now;
    const bool on = digitalRead(PIN_K1) == HIGH;
    simFlow += ((on ? 1.0f : 0.0f) - simFlow) * min(1.0f, dt / SIM_SPINUP_S);
    const float rise = SIM_RATE * simFlow * max(0.0f, 1.0f - simP / SIM_PMAX);
    simP += rise * dt;
    const float noise = SIM_NOISE * ((float)random(-1000, 1000) / 1000.0f);
    const float reading = simP + SIM_C * rise + noise;
    return (reading / SENSOR_FS * SENSOR_SPAN + SENSOR_V0) / DIVIDER;
  }
#endif
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
