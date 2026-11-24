// Settings that survive power-off and firmware updates (NVS, separate from the app partitions)
#pragma once
#include <Preferences.h>
#include <cstring>
#include <cmath>
#include "config.h"

struct Settings {
  uint8_t preset = 0;             // index into PRESETS (config.h); the target follows it
  float custom  = PRESETS[P_CUSTOM].bar; // bar, value of the CUSTOM preset
  float target  = PRESETS[0].bar; // bar
  bool  psi     = false;          // display units
  uint8_t rot   = 1;              // screen rotation: 1 wide (knob right), 0 portrait, 3 / 2 the same turned 180
  bool  valve   = true;           // menu VALVE: deflate valve fitted (g_valve follows it)
  bool  vcheck  = true;           // menu VOLT CHECK: read AUX1 through the 0-25 V module on G15
  bool  wdog    = true;
  bool  bt      = true;           // menu BLUETOOTH: on by default (owner, 24.11; bench: 0 ADC2 fails with a phone connected)
  bool  vchk2   = true;
  bool  pchk    = true;           // menu PUMP CHECK: watch for the compressor running with its output off           // menu VALVE CHECK: watch for a valve stuck open while idle           // menu WATCHDOG: 5 s main-loop watchdog
  bool  econfirm = true;          // menu ENGINE CONFIRM: third press before inflating when the engine is off or unknown
  float vtrim   = 1.0f;           // menu AUX1 TRIM: AUX1 volts x this, set once against a multimeter (module + ADC tolerance)

  // flow offset: dynamic sensor reading minus real tyre pressure
  bool  offAuto   = true;         // true: learned; false: offManual
  float offManual = DEFAULT_OFFSET;
  float offset    = DEFAULT_OFFSET; // learned scalar (fallback until c is known)
  float c         = 0;            // learned: offset = c * rise rate (s); 0 = not learned
  bool  omAuto    = true;         // menu OFFSET MAX: AUTO = from the learned offset, else omMan
  float omMan     = OFFMAX_DEFAULT;
  // highest AUTO offset the pump may subtract (config.h OFFMAX_*): AUTO follows the offset measured at the stops
  float offCap() const {
    if (!omAuto) return omMan;
    return learned ? constrain(OFFMAX_K * offset + OFFMAX_ADD, OFFMAX_LO, OFFMAX_HI) : OFFMAX_DEFAULT;
  }
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
  // hour meter: compressor (K1) and valve (K4) seconds, compressor seconds at the last filter service (menu HOURS)
  uint32_t cmpSec = 0, ventSec = 0, svcSec = 0;
  char wifiPass[9] = OTA_PASS;   // AP password for UPDATE (8 chars + NUL), editable in menu WIFI PASSWORD
  float svcHours() const { return (cmpSec - svcSec) / 3600.0f; }
  // CUSTOM can run on fewer wheels (spare, one wheel after a repair): wheels is saved, wk = active wheels / 4 is runtime only.
  // For such a run the 4-wheel learning is put aside (L4), the RAM values are scaled to that run, the run may correct them
  // for itself, and at the end L4 comes back unchanged: save() always writes L4 while wk != 1, so a 1-wheel run never
  // teaches the 4-wheel values and a power cut mid-run leaves nothing scaled behind.
  // Scaling: inflate is compressor-limited, rise rate ~ 1/n. Deflate goes through n valve cores in parallel and then the one
  // vent valve in series, so tau(n) / tau(4) = (n / 4) * S(4) / S(n), S(n) = series(n * core, valve) (config.h).
  uint8_t wheels = 4;             // CUSTOM wheel count, 1..4
  float wk = 1.0f;
  struct Learn { float offset, c, rate, kTop, bias, dev, tauD; bool learned, topLearned, learnedD; } L4{};
  Learn learnNow() const { return {offset, c, rate, kTop, bias, dev, tauD, learned, topLearned, learnedD}; }
  void learnSet(const Learn& l) {
    offset = l.offset; c = l.c; rate = l.rate; kTop = l.kTop; bias = l.bias; dev = l.dev; tauD = l.tauD;
    learned = l.learned; topLearned = l.topLearned; learnedD = l.learnedD;
  }
  void useWheels(uint8_t n) {
    const float k = constrain((int)n, 1, 4) / 4.0f;
    if (fabsf(k - wk) < 1e-4f) return;
    if (wk != 1.0f) learnSet(L4);                      // back to the 4-wheel values first
    wk = k;
    if (k == 1.0f) return;
    L4 = learnNow();
    rate /= k; kTop /= k; c *= k; tauD *= tauWheels(constrain((int)n, 1, 4));
  }
  float airL() const { return airVL(tyre) * wk; }
  float presetBar(int i) const { return i == P_CUSTOM ? custom : i == 0 ? HIGHWAY_BY_TYRE[tyre < N_TYRE_SIZES ? tyre : 0] : PRESETS[i].bar; }

  void load() {
    Preferences p; p.begin("air", true);
    preset       = p.getUChar("preset", preset); if (preset >= N_PRESETS) preset = 0;
    custom       = p.getFloat("custom", custom);   // clamped in sanitize(), once g_valve is loaded
    psi          = p.getBool ("psi", psi);
    rot          = p.getUChar("rot", rot) & 3;
    valve        = p.getBool ("valve", valve); g_valve = valve;
    vcheck       = p.getBool ("vchk", vcheck);
    econfirm     = p.getBool ("econf", econfirm);
    wdog         = p.getBool ("wdog", wdog);
    bt           = p.getBool ("bt", bt);
    vchk2        = p.getBool ("vstuck", vchk2);
    pchk         = p.getBool ("pstuck", pchk);
    offAuto      = p.getBool ("offAuto", offAuto);
    offManual    = p.getFloat("offMan", offManual);
    offset       = p.getFloat("offset", offset);
    c            = p.getFloat("c", c);
    omAuto       = p.getBool ("omAuto", omAuto);
    omMan        = p.getFloat("omMan", omMan);
    vtrim        = p.getFloat("vtrim", vtrim);
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
    wheels       = constrain((int)p.getUChar("wheels", wheels), 1, 4);
    tyre         = p.getUChar("tyre", tyre); if (tyre >= N_TYRE_SIZES) tyre = 0;
    cmpSec       = p.getUInt ("cmpSec", cmpSec);
    ventSec      = p.getUInt ("ventSec", ventSec);
    svcSec       = p.getUInt ("svcSec", svcSec); if (svcSec > cmpSec) svcSec = cmpSec;
    {
      const String wp = p.getString("wifiPass", OTA_PASS);
      strncpy(wifiPass, wp.c_str(), sizeof wifiPass);
      wifiPass[sizeof wifiPass - 1] = 0;
      if (strlen(wifiPass) < 8) strncpy(wifiPass, OTA_PASS, sizeof wifiPass);
    }
    p.end();
    sanitize();
  }
  // Every float from NVS goes through here: a NaN or absurd value would otherwise live forever (calA = NaN makes every
  // pressure NaN and every "> cap" / "< dead" comparison false, so the safety stops would never fire).
  // Out of range = back to the default, or clamped where the edge is meaningful.
  static float fin(float v, float lo, float hi, float def) { return std::isfinite(v) && v >= lo && v <= hi ? v : def; }
  void sanitize() {
    // custom after g_valve is known: TARGET_MIN depends on the VALVE setting
    custom       = std::isfinite(custom) ? constrain(custom, (float)TARGET_MIN, TARGET_MAX) : PRESETS[P_CUSTOM].bar;
    target       = presetBar(preset);
    if (!std::isfinite(target)) target = PRESETS[0].bar;
    target       = constrain(target, (float)TARGET_MIN, TARGET_MAX);
    offManual    = fin(offManual, 0.05f, 1.5f, DEFAULT_OFFSET);           // menu range
    offset       = fin(offset, 0.0f, 2.0f, DEFAULT_OFFSET);               // learnMain accepts < 2.0
    c            = fin(c, 0.0f, 240.0f, 0.0f);
    vtrim        = fin(vtrim, VTRIM_MIN, VTRIM_MAX, 1.0f);
        omMan        = fin(omMan, OFFMAX_LO, OFFMAX_HI, OFFMAX_DEFAULT);       // menu range                            // learned 0.5..60, x4 room for tyre / wheel scaling
    settleManual = fin(settleManual, 0.5f, 10.0f, SETTLE_MS / 1000.0f);  // menu range
    settleLearn  = fin(settleLearn, 0.0f, 10.0f, SETTLE_MS / 1000.0f);
    rate         = fin(rate, 0.0001f, 2.0f, DEFAULT_RATE);
    kTop         = fin(kTop, 0.0001f, 2.0f, DEFAULT_RATE * 0.6f);
    bias         = fin(bias, -1.0f, 1.0f, 0.0f);
    dev          = fin(dev, 0.0f, 1.0f, 0.05f);
    tauD         = fin(tauD, 5.0f, 3000.0f, TAU_D_DEFAULT);               // the learning accepts 5..3000 s
    if (!(std::isfinite(calA) && calA >= 0.7f && calA <= 1.3f && std::isfinite(calB) && fabsf(calB) <= 0.5f)) {
      calA = 1.0f; calB = 0.0f;                                           // calibration.h accepts only this box
    }
  }
  void save() const {
    Preferences p; p.begin("air", false);
    const Learn w = wk != 1.0f ? L4 : learnNow();
    p.putUChar("preset", preset);
    p.putFloat("custom", custom);
    p.putFloat("target", target);
    p.putBool ("psi", psi);
    p.putUChar("rot", rot);
    p.putBool ("valve", valve);
    p.putBool ("vchk", vcheck);
    p.putBool ("econf", econfirm);
    p.putBool ("wdog", wdog);
    p.putBool ("bt", bt);
    p.putBool ("vstuck", vchk2);
    p.putBool ("pstuck", pchk);
    p.putBool ("offAuto", offAuto);
    p.putFloat("offMan", offManual);
    p.putFloat("offset", w.offset);
    p.putFloat("c", w.c);
    p.putBool ("omAuto", omAuto);
    p.putFloat("omMan", omMan);
    p.putFloat("vtrim", vtrim);
    p.putBool ("setAuto", settleAuto);
    p.putFloat("setMan", settleManual);
    p.putFloat("setLrn", settleLearn);
    p.putFloat("rate", w.rate);      // always the 4-wheel values (see useWheels)
    p.putFloat("kTop", w.kTop);
    p.putFloat("bias", w.bias);
    p.putFloat("dev", w.dev);
    p.putBool ("learned", w.learned);
    p.putBool ("topL", w.topLearned);
    p.putFloat("calA", calA);
    p.putFloat("calB", calB);
    p.putFloat("tauD", w.tauD);
    p.putBool ("lrnD", w.learnedD);
    p.putUInt ("boot", bootN);
    p.putUChar("tyre", tyre);
    p.putUChar("wheels", wheels);
    p.putUInt ("cmpSec", cmpSec);
    p.putUInt ("ventSec", ventSec);
    p.putUInt ("svcSec", svcSec);
    p.putString("wifiPass", wifiPass);
    p.end();
  }
  // other tyre size: everything learned is scaled by the air volume, so the learning carries over
  // (rise rates ~ 1/V, deflate time constant ~ V, flow offset per rise rate ~ V; the offset itself is a flow effect and stays)
  void setTyre(uint8_t i) {
    if (i >= N_TYRE_SIZES || i == tyre || wk != 1.0f) return;
    const float k = airVL(i) / airVL(tyre);
    rate /= k; kTop /= k; tauD *= k; c *= k; tyre = i;
    if (preset == 0) target = presetBar(0);   // HIGHWAY follows the tyre size
    save();
  }
  void resetLearning() {
    wk = 1.0f;
    offset = DEFAULT_OFFSET; c = 0; rate = inflRate(1.5f, airL()); kTop = rate * 0.6f;
    bias = 0; dev = 0.05f; settleLearn = SETTLE_MS / 1000.0f; learned = false; topLearned = false;
    tauD = tauModel(airL()); learnedD = false; save();
  }
};
