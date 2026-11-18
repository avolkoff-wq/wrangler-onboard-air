// Settings that survive power-off and firmware updates (NVS, separate from the app partitions)
#pragma once
#include <Preferences.h>
#include "config.h"

struct Settings {
  uint8_t preset = 0;             // index into PRESETS (config.h); the target follows it
  float custom  = PRESETS[P_CUSTOM].bar; // bar, value of the CUSTOM preset
  float target  = PRESETS[0].bar; // bar
  bool  psi     = false;          // display units
  uint8_t rot   = 1;              // screen rotation: 1 wide (knob right), 0 portrait, 3 / 2 the same turned 180

  // flow offset: dynamic sensor reading minus real tyre pressure
  bool  offAuto   = true;         // true: learned; false: offManual
  float offManual = DEFAULT_OFFSET;
  float offset    = DEFAULT_OFFSET; // learned scalar (fallback until c is known)
  float c         = 0;            // learned: offset = c * rise rate (s); 0 = not learned
  // settling pause before a static measurement
  bool  settleAuto   = true;      // true: wait until pressure is steady
  float settleManual = SETTLE_MS / 1000.0f;  // s
  float settleLearn  = SETTLE_MS / 1000.0f;  // s, last auto result, for display

  float rate    = DEFAULT_RATE;   // learned main-run rate, bar/s
  float kTop    = DEFAULT_RATE * 0.6f; // learned top-up gain incl. spin-up, bar/s
  float bias    = 0;              // learned miss at stop: real - estimate, bar
  float dev     = 0.05f;          // learned spread of that miss, bar
  bool  learned = false;          // false -> next auto run starts with the learning pulse
  bool  topLearned = false;       // false -> kTop is estimated from the main-run rate
  float calA    = 1.0f;           // calibrated = calA * raw + calB
  float calB    = 0.0f;
  // deflate (schematic C): second, separate learning. p_abs(t) = p_abs0 * exp(-t / tauD) while the valve is open
  float tauD    = TAU_D_DEFAULT;  // s, start value from the air model (config.h)
  bool  learnedD = false;         // false -> the first vent only goes 60 % of the way
  uint32_t bootN = 0;             // power-up counter, first column of the log
  uint8_t tyre  = 0;              // index into TYRES (config.h)
  float airL() const { return airVL(tyre); }
  float presetBar(int i) const { return i == P_CUSTOM ? custom : PRESETS[i].bar; }

  void load() {
    Preferences p; p.begin("air", true);
    preset       = p.getUChar("preset", preset); if (preset >= N_PRESETS) preset = 0;
    custom       = constrain(p.getFloat("custom", custom), TARGET_MIN, TARGET_MAX);
    target       = presetBar(preset);
    psi          = p.getBool ("psi", psi);
    rot          = p.getUChar("rot", rot) & 3;
    offAuto      = p.getBool ("offAuto", offAuto);
    offManual    = p.getFloat("offMan", offManual);
    offset       = p.getFloat("offset", offset);
    c            = p.getFloat("c", c);
    settleAuto   = p.getBool ("setAuto", settleAuto);
    settleManual = p.getFloat("setMan", settleManual);
    settleLearn  = p.getFloat("setLrn", settleLearn);
    rate         = p.getFloat("rate", rate);
    kTop         = p.getFloat("kTop", kTop);
    bias         = p.getFloat("bias", bias);
    dev          = p.getFloat("dev", dev);
    learned      = p.getBool ("learned", learned);
    topLearned   = p.getBool ("topL", topLearned);
    calA         = p.getFloat("calA", calA);
    calB         = p.getFloat("calB", calB);
    tauD         = p.getFloat("tauD", tauD);
    learnedD     = p.getBool ("lrnD", learnedD);
    bootN        = p.getUInt ("boot", bootN);
    tyre         = p.getUChar("tyre", tyre); if (tyre >= N_TYRE_SIZES) tyre = 0;
    p.end();
    if (target > TARGET_MAX) target = TARGET_MAX;
    if (target < TARGET_MIN) target = TARGET_MIN;
  }
  void save() const {
    Preferences p; p.begin("air", false);
    p.putUChar("preset", preset);
    p.putFloat("custom", custom);
    p.putFloat("target", target);
    p.putBool ("psi", psi);
    p.putUChar("rot", rot);
    p.putBool ("offAuto", offAuto);
    p.putFloat("offMan", offManual);
    p.putFloat("offset", offset);
    p.putFloat("c", c);
    p.putBool ("setAuto", settleAuto);
    p.putFloat("setMan", settleManual);
    p.putFloat("setLrn", settleLearn);
    p.putFloat("rate", rate);
    p.putFloat("kTop", kTop);
    p.putFloat("bias", bias);
    p.putFloat("dev", dev);
    p.putBool ("learned", learned);
    p.putBool ("topL", topLearned);
    p.putFloat("calA", calA);
    p.putFloat("calB", calB);
    p.putFloat("tauD", tauD);
    p.putBool ("lrnD", learnedD);
    p.putUInt ("boot", bootN);
    p.putUChar("tyre", tyre);
    p.end();
  }
  // other tyre size: everything learned is scaled by the air volume, so the learning carries over
  // (rise rates ~ 1/V, deflate time constant ~ V, flow offset per rise rate ~ V; the offset itself is a flow effect and stays)
  void setTyre(uint8_t i) {
    if (i >= N_TYRE_SIZES || i == tyre) return;
    const float k = airVL(i) / airVL(tyre);
    rate /= k; kTop /= k; tauD *= k; c *= k; tyre = i; save();
  }
  void resetLearning() {
    offset = DEFAULT_OFFSET; c = 0; rate = inflRate(1.5f, airL()); kTop = rate * 0.6f;
    bias = 0; dev = 0.05f; settleLearn = SETTLE_MS / 1000.0f; learned = false; topLearned = false;
    tauD = tauModel(airL()); learnedD = false; save();
  }
};
