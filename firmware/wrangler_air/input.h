// Rotary encoder (G41/G40) and its push button (G42), no external libraries
#pragma once
#include <Arduino.h>
#include "config.h"

namespace input {

volatile int32_t encCount = 0;
volatile uint8_t encPrev  = 0;

void IRAM_ATTR encIsr() {
  static const int8_t table[16] = {0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};
  const uint8_t s = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  encCount += table[(encPrev << 2) | s];
  encPrev = s;
}

void begin() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_BTN,   INPUT_PULLUP);
  encPrev = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  attachInterrupt(PIN_ENC_A, encIsr, CHANGE);
  attachInterrupt(PIN_ENC_B, encIsr, CHANGE);
}

// detents since last call (4 transitions per detent)
int takeSteps() {
  static int32_t last = 0;
  noInterrupts(); const int32_t c = encCount; interrupts();
  const int32_t d = (c - last) / 4;
  last += d * 4;
  return (int)d;
}

enum class Btn { NONE, SHORT, MID, LONG };   // MID: released between 0.5 and 2.0 s

// SHORT: released after 30..500 ms. MID: released between 0.5 and 2.0 s, an aborted hold; screens ignore it,
// only a running pump treats it as stop.
// LONG: fires at 2.0 s while still held (menu, CUSTOM cancel, LEARN back, OTA exit, cal abort).
constexpr uint32_t BTN_SHORT_MAX_MS = 500, BTN_LONG_MS = 2000;
bool     btnDown = false, btnLongSent = false;
uint32_t btnT = 0;

Btn button() {
  const bool pressed = digitalRead(PIN_BTN) == LOW;
  const uint32_t now = millis();
  if (pressed && !btnDown)  { btnDown = true; btnLongSent = false; btnT = now; }
  if (pressed && btnDown && !btnLongSent && now - btnT >= BTN_LONG_MS) { btnLongSent = true; return Btn::LONG; }
  if (!pressed && btnDown)  {
    btnDown = false;
    if (!btnLongSent && now - btnT > 30) return now - btnT < BTN_SHORT_MAX_MS ? Btn::SHORT : Btn::MID;
  }
  return Btn::NONE;
}
// how long the button has been held so far, 0 when it is up or the LONG has already fired
uint32_t heldMs() { return (btnDown && !btnLongSent) ? millis() - btnT : 0; }

// ---------- one input path for the knob and, later, the phone (review item 4) ----------
// Every source puts {steps, button} events into one FreeRTOS queue: the knob from loop() (poll), a BLE task from its
// own thread (send). loop() takes them with take(): steps are summed, at most one button per pass, the rest waits.
// So the BLE code never touches UI state from another thread, and a phone STOP is the same event as a knob press.
enum class Src : uint8_t { KNOB, PHONE, USB };
struct Ev { int16_t steps; Btn btn; Src src; };
inline QueueHandle_t queue = nullptr;
inline void queueBegin() { if (!queue) queue = xQueueCreate(32, sizeof(Ev)); }
// thread-safe: may be called from any task (BLE, USB); false = queue full or not started
inline bool send(int steps, Btn b, Src src) {
  if (!queue || (!steps && b == Btn::NONE)) return false;
  const Ev e = {(int16_t)steps, b, src};
  return xQueueSend(queue, &e, 0) == pdTRUE;
}
// knob -> queue, then everything queued -> one pass worth of input
inline void take(int& steps, Btn& b, Src* src = nullptr) {
  send(takeSteps(), button(), Src::KNOB);
  steps = 0; b = Btn::NONE;
  Ev e;
  while (queue && xQueueReceive(queue, &e, 0) == pdTRUE) {
    steps += e.steps;
    if (e.btn != Btn::NONE) { b = e.btn; if (src) *src = e.src; break; }   // the next button waits for the next pass
  }
}

}  // namespace input
