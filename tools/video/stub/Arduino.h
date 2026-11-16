#pragma once
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <algorithm>
using std::min; using std::max;
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define ADC_11db 3
#define IRAM_ATTR
inline uint32_t g_ms = 0;
inline int g_pins[64] = {0};
inline uint32_t millis(){ return g_ms; }
inline void digitalWrite(int p,int v){ g_pins[p]=v; }
inline int digitalRead(int p){ return g_pins[p]; }
inline void pinMode(int,int){}
inline void analogReadResolution(int){}
inline void analogSetPinAttenuation(int,int){}
inline int analogReadMilliVolts(int){ return 0; }
inline long random(long a,long b){ return a + std::rand()%(b-a); }
template<class T,class L,class H> T constrain(T x,L l,H h){ return x<l?l:(x>h?h:x); }
