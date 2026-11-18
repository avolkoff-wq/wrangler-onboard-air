#pragma once
#include <Arduino.h>
namespace wlog { inline bool verbose=true;
inline void ev(const char* e,const String& d){ if(verbose) printf("%7.1f %s %s\n", millis()/1000.0, e, d.c_str()); }
inline String kv(const char*k,float v,int d=3){return String(k)+"="+String(v,d)+";";}
inline String kv(const char*k,const String&v){return String(k)+"="+v+";";}
inline String mv(const char*k,float a,float b,int d=3){return String(k)+"="+String(a,d)+">"+String(b,d)+";";} }
