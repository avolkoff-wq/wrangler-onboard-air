#pragma once
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
struct Preferences{ void begin(const char*,bool){} void end(){}
 float getFloat(const char*,float d){return d;} bool getBool(const char*,bool d){return d;} uint8_t getUChar(const char*,uint8_t d){return d;} uint32_t getUInt(const char*,uint32_t d){return d;} String getString(const char*,const char* d){return String(d);}
 // byte rows (0.8.0 preset table) are kept in memory, so the tests can save, load and spoil them
 static std::map<std::string, std::vector<uint8_t>>& bytes() { static std::map<std::string, std::vector<uint8_t>> m; return m; }
 size_t putBytes(const char* k, const void* v, size_t n) { bytes()[k].assign((const uint8_t*)v, (const uint8_t*)v + n); return n; }
 size_t getBytesLength(const char* k) { auto it = bytes().find(k); return it == bytes().end() ? 0 : it->second.size(); }
 size_t getBytes(const char* k, void* v, size_t n) { auto it = bytes().find(k); if (it == bytes().end()) return 0; const size_t m = it->second.size() < n ? it->second.size() : n; memcpy(v, it->second.data(), m); return m; }
 void putFloat(const char*,float){} void putBool(const char*,bool){} void putUChar(const char*,uint8_t){} void putUInt(const char*,uint32_t){} void putString(const char*,const char*){} };
