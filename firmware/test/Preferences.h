#pragma once
#include <cstdint>
struct Preferences{ void begin(const char*,bool){} void end(){}
 float getFloat(const char*,float d){return d;} bool getBool(const char*,bool d){return d;} uint8_t getUChar(const char*,uint8_t d){return d;} uint32_t getUInt(const char*,uint32_t d){return d;}
 void putFloat(const char*,float){} void putBool(const char*,bool){} void putUChar(const char*,uint8_t){} void putUInt(const char*,uint32_t){} };
