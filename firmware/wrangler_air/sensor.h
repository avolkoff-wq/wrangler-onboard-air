// Pressure sensor on G1: 0.5-4.5 V ratiometric through a 10k/20k divider
#pragma once
#include <Arduino.h>
#include "config.h"

class Sensor {
 public:
  void begin() {
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_SENSOR, ADC_11db);
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
  // Virtual 4-tyre system: while K1 is on, tyres rise and the sensor reads
  // tyres + a flow offset that shrinks as they fill. Returns the voltage at G1.
  float simTyre() {
    static float p = SIM_START_BAR;
    static uint32_t last = millis();
    const uint32_t now = millis();
    const float dt = (now - last) / 1000.0f; last = now;
    const bool on = digitalRead(PIN_K1) == HIGH;
    if (on) p += SIM_RATE * dt;
    const float reading = p + (on ? max(0.15f, SIM_OFFSET - 0.06f * p) : 0.0f);
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
