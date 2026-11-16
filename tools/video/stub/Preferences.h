#include <cstdint>
#pragma once
#include <map>
#include <string>
struct Preferences {
  static std::map<std::string,float>& f(){ static std::map<std::string,float> m; return m; }
  static std::map<std::string,bool>& b(){ static std::map<std::string,bool> m; return m; }
  void begin(const char*,bool){} void end(){}
  float getFloat(const char*k,float d){ auto it=f().find(k); return it==f().end()?d:it->second; }
  bool getBool(const char*k,bool d){ auto it=b().find(k); return it==b().end()?d:it->second; }
  void putFloat(const char*k,float v){ f()[k]=v; } void putBool(const char*k,bool v){ b()[k]=v; }

  uint8_t getUChar(const char*k,uint8_t d){ auto it=f().find(k); return it==f().end()?d:(uint8_t)it->second; }
  void putUChar(const char*k,uint8_t v){ f()[k]=v; }
};
