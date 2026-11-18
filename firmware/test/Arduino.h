#pragma once
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
using std::min; using std::max;
extern uint32_t g_ms; inline uint32_t millis(){return g_ms;}
extern int g_pins[64]; inline void digitalWrite(int p,int v){g_pins[p]=v;} inline int digitalRead(int p){return g_pins[p];}
#define HIGH 1
#define LOW 0
inline long random(long a,long b){return a+rand()%(b-a);}
template<class T> T constrain(T v,T a,T b){return v<a?a:v>b?b:v;}
struct String : std::string { String(){} String(const char*s):std::string(s){} String(const std::string&s):std::string(s){}
 String(float v,int d){char b[32];snprintf(b,32,"%.*f",d,v);assign(b);} String(int v){assign(std::to_string(v));}
 String operator+(const String&o)const{return String(std::string(*this)+std::string(o));} String operator+(const char*o)const{return String(std::string(*this)+o);} };
inline String operator+(const char*a,const String&b){return String(std::string(a)+std::string(b));}
