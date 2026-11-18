#define SENSOR_DIRECT 1
#define DEFLATE 1
#define BENCH_SIM 1
#include "config.h"
uint32_t g_ms=0; int g_pins[64]={0};
inline void analogReadResolution(int){} inline void analogSetPinAttenuation(int,int){} 
#define ADC_11db 0
#include "sensor.h"
#include "storage.h"
#include "pump.h"
int main(int argc,char**argv){
  Settings st; Sensor s; Pump p; p.begin(&s,&st);
  float starts[]={2.4,2.4,2.4,1.2,0.9,2.3}; float targets[]={1.2,0.8,1.2,2.4,2.4,2.25};
  for(int r=0;r<6;r++){
    Sensor::simP=starts[r]; st.target=targets[r]; s.update();
    printf("==== run %d: tyres %.2f -> target %.2f  (tauD %.1f learned %d)\n",r,starts[r],targets[r],st.tauD,st.learnedD);
    p.start(); uint32_t t0=g_ms;
    while(p.busy() && g_ms-t0<20*60*1000){ g_ms+=20; s.update(); p.update(); }
    printf("  -> state %d err %d final static %.3f real %.3f  time %.0fs starts %d vents %d\n",(int)p.state(),(int)p.error(),p.staticBar(),Sensor::simP,(g_ms-t0)/1000.0,p.starts(),p.vents());
    p.clearResult(); g_ms+=5000;
  }
}
