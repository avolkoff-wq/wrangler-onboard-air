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

enum class Btn { NONE, SHORT, LONG };

Btn button() {
  static bool down = false, longSent = false;
  static uint32_t t = 0;
  const bool pressed = digitalRead(PIN_BTN) == LOW;
  const uint32_t now = millis();
  if (pressed && !down)  { down = true; longSent = false; t = now; }
  if (pressed && down && !longSent && now - t > 1000) { longSent = true; return Btn::LONG; }
  if (!pressed && down)  { down = false; if (!longSent && now - t > 30) return Btn::SHORT; }
  return Btn::NONE;
}

}  // namespace input
