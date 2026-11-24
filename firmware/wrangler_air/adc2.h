// ADC2 reads with an error code: G13 pressure sensor, G15 AUX1 module.
// analogReadMilliVolts() returns 0 on any driver error (for example while the radio holds ADC2), which is
// indistinguishable from a real 0 V: at 2.5 bar one failed read in 8 showed 2.19 bar and the pump over-inflated.
// Here every sample reports success; failed samples are dropped by the caller and counted in adc2::fails.
#pragma once
#include "config.h"
#if __has_include("esp_adc/adc_oneshot.h")
#define ADC2_REAL 1
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#endif

namespace adc2 {
inline uint32_t fails = 0;   // failed samples since the last reset (RUN start); logged in END, shown in `io` / `status`

#ifdef ADC2_REAL

inline adc_oneshot_unit_handle_t unit = nullptr;
inline adc_cali_handle_t cali[2] = {nullptr, nullptr};   // per pin: [0] PIN_SENSOR, [1] PIN_VBAT

inline int slot(int pin) { return pin == PIN_SENSOR ? 0 : pin == PIN_VBAT ? 1 : -1; }

// 12 dB attenuation (0-3.1 V), as the old analogSetPinAttenuation(ADC_11db). false = ADC not usable
inline bool begin() {
  if (unit) return true;
  adc_oneshot_unit_init_cfg_t u = {};
  u.unit_id = ADC_UNIT_2;
  u.ulp_mode = ADC_ULP_MODE_DISABLE;
  if (adc_oneshot_new_unit(&u, &unit) != ESP_OK) { unit = nullptr; return false; }
  bool ok = true;
  for (int pin : {PIN_SENSOR, PIN_VBAT}) {
    if (pin < 0) continue;
    adc_unit_t un; adc_channel_t ch;
    if (adc_oneshot_io_to_channel(pin, &un, &ch) != ESP_OK || un != ADC_UNIT_2) { ok = false; continue; }
    adc_oneshot_chan_cfg_t c = {};
    c.atten = ADC_ATTEN_DB_12;
    c.bitwidth = ADC_BITWIDTH_DEFAULT;
    if (adc_oneshot_config_channel(unit, ch, &c) != ESP_OK) { ok = false; continue; }
    adc_cali_curve_fitting_config_t k = {};
    k.unit_id = ADC_UNIT_2; k.chan = ch; k.atten = ADC_ATTEN_DB_12; k.bitwidth = ADC_BITWIDTH_DEFAULT;
    if (adc_cali_create_scheme_curve_fitting(&k, &cali[slot(pin)]) != ESP_OK) ok = false;
  }
  return ok;
}

// one sample in mV; false = driver error (radio busy, timeout), the sample must not be used
inline bool readMv(int pin, int& mv) {
  const int s = slot(pin);
  adc_unit_t un; adc_channel_t ch; int raw = 0;
  if (!unit || s < 0 || !cali[s] || adc_oneshot_io_to_channel(pin, &un, &ch) != ESP_OK ||
      adc_oneshot_read(unit, ch, &raw) != ESP_OK || adc_cali_raw_to_voltage(cali[s], raw, &mv) != ESP_OK) {
    fails++;
    return false;
  }
  return true;
}

#else   // desk test (test/run.sh): a settable pin voltage and a failure pattern
inline int stubMv[64] = {0};
inline int stubFailOf8 = 0;   // fail this many samples out of every 8
inline uint32_t stubN = 0;
inline bool begin() { return true; }
inline bool readMv(int pin, int& mv) {
  if ((int)(stubN++ % 8) < stubFailOf8) { fails++; return false; }
  mv = stubMv[pin & 63];
  return true;
}
#endif
}
