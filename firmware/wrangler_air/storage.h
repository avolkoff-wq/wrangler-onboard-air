// Settings that survive power-off and firmware updates (NVS, separate from the app partitions)
#pragma once
#include <Preferences.h>
#include "config.h"

struct Settings {
  float target  = 2.2f;           // bar
  bool  psi     = false;          // display units
  float offset  = DEFAULT_OFFSET; // learned dynamic offset, bar
  float rate    = DEFAULT_RATE;   // learned inflation rate, bar/s
  bool  learned = false;          // false -> next run starts with the calibration pulse
  float calA    = 1.0f;           // calibrated = calA * raw + calB
  float calB    = 0.0f;

  void load() {
    Preferences p; p.begin("air", true);
    target  = p.getFloat("target", target);
    psi     = p.getBool ("psi", psi);
    offset  = p.getFloat("offset", offset);
    rate    = p.getFloat("rate", rate);
    learned = p.getBool ("learned", learned);
    calA    = p.getFloat("calA", calA);
    calB    = p.getFloat("calB", calB);
    p.end();
    if (target > TARGET_MAX) target = TARGET_MAX;
    if (target < 0) target = 0;
  }
  void save() const {
    Preferences p; p.begin("air", false);
    p.putFloat("target", target);
    p.putBool ("psi", psi);
    p.putFloat("offset", offset);
    p.putFloat("rate", rate);
    p.putBool ("learned", learned);
    p.putFloat("calA", calA);
    p.putFloat("calB", calB);
    p.end();
  }
  void resetLearning() { offset = DEFAULT_OFFSET; rate = DEFAULT_RATE; learned = false; save(); }
};
