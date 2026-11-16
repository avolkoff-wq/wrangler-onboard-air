// Settings that survive power-off and firmware updates (NVS, separate from the app partitions)
#pragma once
#include <Preferences.h>
#include "config.h"

struct Settings {
  float target  = 2.2f;           // bar
  bool  psi     = false;          // display units

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
  float calA    = 1.0f;           // calibrated = calA * raw + calB
  float calB    = 0.0f;

  void load() {
    Preferences p; p.begin("air", true);
    target       = p.getFloat("target", target);
    psi          = p.getBool ("psi", psi);
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
    calA         = p.getFloat("calA", calA);
    calB         = p.getFloat("calB", calB);
    p.end();
    if (target > TARGET_MAX) target = TARGET_MAX;
    if (target < 0) target = 0;
  }
  void save() const {
    Preferences p; p.begin("air", false);
    p.putFloat("target", target);
    p.putBool ("psi", psi);
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
    p.putFloat("calA", calA);
    p.putFloat("calB", calB);
    p.end();
  }
  void resetLearning() {
    offset = DEFAULT_OFFSET; c = 0; rate = DEFAULT_RATE; kTop = DEFAULT_RATE * 0.6f;
    bias = 0; dev = 0.05f; settleLearn = SETTLE_MS / 1000.0f; learned = false; save();
  }
};
