// What the screen shows, as one plain struct (review item 4). loop() fills it once per pass (publishUi in the .ino);
// USB `io` / `ui` print it, and the planned BLE notify sends it, so the phone shows the same screen as the DinMeter.
// Readers in other tasks (BLE) take a copy under a short critical section and never touch the UI globals.
#pragma once
#include <Arduino.h>

struct UiSnapshot {
  uint32_t seq = 0;        // +1 per pass, the phone can drop stale notifies
  uint8_t  screen = 0;     // Screen enum value (.ino)
  uint8_t  menuPos = 0;
  uint8_t  preset = 0;
  uint8_t  state = 0;      // PState
  uint8_t  err = 0;        // PErr
  uint8_t  armStage = 0;   // 0 not armed, 1 armed, 2 engine confirmed
  bool     k1 = false, valve = false, noAir = false, inBand = false, offTarget = false, fake = false, demo = false;
  float    bar = 0;        // calibrated line pressure
  float    est = 0;        // tyre estimate while pumping
  float    target = 0;
  float    staticBar = 0;  // last static measurement
  float    aux1 = 0;       // AUX1 volts (0 = no module)
  uint32_t runMs = 0, phaseMs = 0;
  uint32_t adcFails = 0;
};

inline UiSnapshot g_ui;
inline portMUX_TYPE g_uiMux = portMUX_INITIALIZER_UNLOCKED;
inline void uiPublish(const UiSnapshot& s) { portENTER_CRITICAL(&g_uiMux); g_ui = s; portEXIT_CRITICAL(&g_uiMux); }
inline UiSnapshot uiRead() { portENTER_CRITICAL(&g_uiMux); const UiSnapshot s = g_ui; portEXIT_CRITICAL(&g_uiMux); return s; }
