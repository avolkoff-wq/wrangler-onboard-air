// Event log on the flash (LittleFS), downloadable over Wi-Fi in the UPDATE mode: http://192.168.4.1/log
// One CSV line per event: boot number, seconds since power-up, event, data as key=value pairs separated by ';'.
// Events: BOOT, RUN (start), STATIC (static measurement), STOP (end of a pumping / venting phase with the numbers
// at the stop), LEARN_INF / LEARN_DEF (what was measured and how the learned values moved: old>new),
// CAL_POINT / CAL_DONE / CAL_FAIL (gauge calibration), END (result of a run).
// At LOG_MAX_BYTES the file moves to log_old.csv, so the last two files are always there.
#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"

namespace wlog {
inline bool ok = false;
inline uint32_t boot = 0;

inline void begin(uint32_t bootNo) {
  ok = LittleFS.begin(true);   // formats the partition on the very first start
  boot = bootNo;
}

inline void rotate() {
  File f = LittleFS.open("/log.csv", "r");
  const size_t s = f ? f.size() : 0;
  if (f) f.close();
  if (s > LOG_MAX_BYTES) { LittleFS.remove("/log_old.csv"); LittleFS.rename("/log.csv", "/log_old.csv"); }
}

inline void ev(const char* event, const String& data) {
#if BENCH_USB
  Serial.printf("%lu,%.1f,%s,%s\n", (unsigned long)boot, millis() / 1000.0f, event, data.c_str());   // bench: every line also on USB
#endif
  if (!ok) return;
  rotate();
  const bool fresh = !LittleFS.exists("/log.csv");
  File f = LittleFS.open("/log.csv", "a");
  if (!f) return;
  if (fresh) f.println("boot,t_s,event,data");
  f.printf("%lu,%.1f,%s,%s\n", (unsigned long)boot, millis() / 1000.0f, event, data.c_str());
  f.close();
}

inline size_t size() {
  size_t s = 0;
  for (const char* p : {"/log_old.csv", "/log.csv"}) { File f = LittleFS.open(p, "r"); if (f) { s += f.size(); f.close(); } }
  return s;
}

inline void clear() { LittleFS.remove("/log.csv"); LittleFS.remove("/log_old.csv"); }

// "key=value" helpers
inline String kv(const char* k, float v, int d = 3) { return String(k) + "=" + String(v, d) + ";"; }
inline String kv(const char* k, const String& v)     { return String(k) + "=" + v + ";"; }
inline String mv(const char* k, float a, float b, int d = 3) { return String(k) + "=" + String(a, d) + ">" + String(b, d) + ";"; }
}  // namespace wlog
