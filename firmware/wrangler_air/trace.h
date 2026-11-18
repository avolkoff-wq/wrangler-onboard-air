// Per-run pressure trace: one small CSV per run (also LEARN tests), kept in RAM while the run goes and written
// to LittleFS /runs once at the end (one flash write per run). The last TRACE_KEEP runs are kept.
// The Wi-Fi page lists them and draws each one as a graph in the browser (TRACE_GRAPH_JS, plain SVG, offline).
//
// File:  # key=value;... header (preset, target, p0, p, result, boot, t0, tyre, starts, vents, dur, test, truncated)
//        t,line,est,ph,k1,valve        every TRACE_DT_MS: line = calibrated sensor reading, est = estimate while pumping
//        t,STATIC,p                    static measurement result (marker row)
// Phase codes: S settle, E measure, C calibration pulse / LEARN INFLATE, M main pump, T top-up, V vent
#pragma once
#include <Arduino.h>
#include "config.h"

constexpr uint32_t TRACE_DT_MS  = 500;    // one row every 0.5 s
constexpr size_t   TRACE_MAX    = 8000;   // bytes per run; longer runs stop recording ("truncated=1")
constexpr int      TRACE_KEEP   = 20;     // newest runs kept on the flash

class Trace {
 public:
  void begin(uint32_t nowMs) { body_ = ""; t0_ = nowMs; lastMs_ = 0; first_ = true; truncated_ = false; active_ = true; }
  bool active() const { return active_; }
  // one sample; call every loop, it keeps its own 0.5 s pace
  void sample(uint32_t nowMs, float line, float est, char ph, bool k1, bool valve) {
    if (!active_ || (!first_ && nowMs - lastMs_ < TRACE_DT_MS)) return;
    first_ = false; lastMs_ = nowMs;
    const char pc[2] = {ph, 0};
    add(String((nowMs - t0_) / 1000.0f, 1) + "," + String(line, 3) + "," + String(est, 3) + "," + String(pc) + "," +
        (k1 ? "1" : "0") + "," + (valve ? "1" : "0") + "\n");
  }
  void marker(uint32_t nowMs, float p) {
    if (active_) add(String((nowMs - t0_) / 1000.0f, 1) + ",STATIC," + String(p, 3) + "\n");
  }
  // header + body; the caller writes it to a file
  String finish(const String& header) {
    active_ = false;
    return "# " + header + "truncated=" + (truncated_ ? "1" : "0") + ";\nt,line,est,ph,k1,valve\n" + body_;
  }
  float seconds(uint32_t nowMs) const { return (nowMs - t0_) / 1000.0f; }

 private:
  void add(const String& row) {
    if (truncated_) return;
    if (body_.length() + row.length() > TRACE_MAX) { truncated_ = true; return; }
    body_ += row;
  }
  String body_;
  uint32_t t0_ = 0, lastMs_ = 0;
  bool first_ = true, truncated_ = false, active_ = false;
};

// Graph page script: draw(csvText, psi) fills <svg id=g> and <div id=h>. Plain JS + SVG, no libraries, works offline.
// Used by the Wi-Fi page (/run?id=...) and by the desk test for docs/screen/run_graph_example.html.
static const char TRACE_GRAPH_HTML_HEAD[] = R"HTML(<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'>
<title>Wrangler Air run</title>
<style>body{font:14px Arial,sans-serif;margin:10px;background:#fff;color:#111}#h{margin:4px 0 8px;color:#333}
svg{width:100%;max-width:900px;height:auto;border:1px solid #ccc;background:#fff}.k{display:inline-block;width:12px;height:10px;margin:0 4px 0 12px}</style>
<div id=h></div><svg id=g viewBox='0 0 900 420'></svg>
<div><span class=k style=background:#ffd9a8></span>compressor <span class=k style=background:#cfe6fb></span>valve open
<span class=k style='border-top:2px dashed #c00;height:0'></span>target <span class=k style='background:#111;border-radius:6px;width:8px;height:8px'></span>static measurement
<span class=k style='background:#999;height:2px'></span>estimate while pumping</div>
<script>
function draw(txt,psi){
 var L=txt.trim().split('\n'),H={},R=[],M=[];
 L[0].replace(/^#\s*/,'').split(';').forEach(function(kv){var i=kv.indexOf('=');if(i>0)H[kv.slice(0,i)]=kv.slice(i+1);});
 for(var i=2;i<L.length;i++){var c=L[i].split(',');if(c[1]=='STATIC')M.push([+c[0],+c[2]]);else if(c.length>=6)R.push([+c[0],+c[1],+c[2],c[3],c[4]=='1',c[5]=='1']);}
 var k=psi?14.5038:1,u=psi?'psi':'bar',tg=+H.target,f=function(v){return (v*k).toFixed(psi?0:2)};
 document.getElementById('h').innerHTML='<b>'+(H.preset||'')+' '+f(tg)+' '+u+'</b> &nbsp; '+(H.test&&H.test!='-'?'LEARN '+H.test+' &nbsp; ':'')+
  'p0 '+f(+H.p0)+' &rarr; '+f(+H.p)+' '+u+' &nbsp; '+H.result+' &nbsp; '+(+H.dur).toFixed(0)+' s &nbsp; starts '+H.starts+' vents '+H.vents+
  ' &nbsp; tyre '+H.tyre+(H.truncated=='1'?' &nbsp; <b>truncated</b>':'');
 var W=900,Hh=420,l=56,r=12,t=12,b=40,tmax=Math.max(1,R.length?R[R.length-1][0]:1,M.length?M[M.length-1][0]:1),
  vals=R.map(function(x){return x[1]}).concat(M.map(function(x){return x[1]}),[tg,+H.p0]),
  lo=Math.max(0,Math.min.apply(null,vals)-0.2),hi=Math.max.apply(null,vals)+0.2,
  X=function(s){return l+(W-l-r)*s/tmax},Y=function(v){return t+(Hh-t-b)*(1-(v-lo)/(hi-lo))},s='';
 // bands where the compressor ran / the valve was open
 function bands(ix,col){var st=null;for(var i=0;i<R.length;i++){var on=R[i][ix];if(on&&st===null)st=R[i][0];
  if((!on||i==R.length-1)&&st!==null){var e=on?R[i][0]:R[i][0];s+='<rect x='+X(st)+' y='+t+' width='+Math.max(1,X(e)-X(st))+' height='+(Hh-t-b)+' fill='+col+' />';st=null;}}}
 bands(4,'#ffd9a8');bands(5,'#cfe6fb');
 // grid and axes
 var step=(hi-lo)*k>4?1:(hi-lo)*k>1.5?0.5:0.2;if(psi)step=(hi-lo)*k>40?10:5;
 for(var g=Math.ceil(lo*k/step)*step;g<=hi*k;g+=step){var y=Y(g/k);s+='<line x1='+l+' x2='+(W-r)+' y1='+y+' y2='+y+' stroke=#eee />'+
  '<text x='+(l-6)+' y='+(y+4)+' text-anchor=end font-size=12>'+(psi?g.toFixed(0):g.toFixed(1))+'</text>';}
 var ts=tmax>300?60:tmax>120?30:tmax>40?10:5;
 for(var g=0;g<=tmax;g+=ts){var x=X(g);s+='<line x1='+x+' x2='+x+' y1='+t+' y2='+(Hh-b)+' stroke=#f2f2f2 />'+
  '<text x='+x+' y='+(Hh-b+16)+' text-anchor=middle font-size=12>'+g+'</text>';}
 s+='<text x='+((l+W-r)/2)+' y='+(Hh-6)+' text-anchor=middle font-size=12>time, s</text>'+
  '<text x=14 y='+((t+Hh-b)/2)+' transform="rotate(-90 14 '+((t+Hh-b)/2)+')" text-anchor=middle font-size=12>'+u+'</text>'+
  '<rect x='+l+' y='+t+' width='+(W-l-r)+' height='+(Hh-t-b)+' fill=none stroke=#999 />';
 // target, estimate, line reading, static points
 s+='<line x1='+l+' x2='+(W-r)+' y1='+Y(tg)+' y2='+Y(tg)+' stroke=#c00 stroke-dasharray="6 4" stroke-width=1.5 />';
 var pe='',pl='';R.forEach(function(x){if(x[4])pe+=(pe?' L':'M')+X(x[0]).toFixed(1)+' '+Y(x[2]).toFixed(1);else if(pe)pe+=' M'+X(x[0]).toFixed(1)+' '+Y(x[2]).toFixed(1);
  pl+=(pl?' L':'M')+X(x[0]).toFixed(1)+' '+Y(x[1]).toFixed(1);});
 if(pe)s+='<path d="'+pe+'" fill=none stroke=#999 stroke-width=1.5 />';
 s+='<path d="'+pl+'" fill=none stroke=#1a4fa0 stroke-width=2 />';
 M.forEach(function(m){s+='<circle cx='+X(m[0])+' cy='+Y(m[1])+' r=4.5 fill=#111><title>'+f(m[1])+' '+u+' at '+m[0]+' s</title></circle>';});
 document.getElementById('g').innerHTML=s;
}
</script>
)HTML";
