// Firmware update over Wi-Fi: DinMeter opens its own access point,
// the phone connects and uploads the .bin at http://192.168.4.1
// The new image goes to the other OTA partition; settings (NVS) are kept.
#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <LittleFS.h>
#include <algorithm>
#include "config.h"
#include "log.h"
#include "trace.h"

class Ota {
 public:
  void start(const bool* psi = nullptr) {
    if (running_) return;
    psi_ = psi;
    digitalWrite(PIN_K1, LOW); if (PIN_K4 >= 0) digitalWrite(PIN_K4, LOW);   // outputs off for the whole update
    WiFi.mode(WIFI_AP);
    WiFi.softAP(OTA_SSID, OTA_PASS);
    server_.on("/", HTTP_GET, [this]() {
      server_.send(200, "text/html",
        "<meta name=viewport content='width=device-width'>"
        "<h3>Wrangler Air " FW_VERSION "</h3>"
        "<p><a href=/log>Download log (CSV, " + String(wlog::size() / 1024) + " kB)</a></p>"
        "<form method=POST action=/clear onsubmit=\"return confirm('Clear the log?')\"><input type=submit value='Clear log'></form>"
        "<form method=POST action=/clearruns onsubmit=\"return confirm('Clear all run graphs?')\"><input type=submit value='Clear runs'></form><hr>"
        "<h4>Runs (newest first)</h4>" + runList() +
        "<p style=color:#666>Graphs are recorded during each run and shown here afterwards. No live graph: "
        "the pressure input (ADC2) can not be read while Wi-Fi is on.</p><hr>"
        "<form method=POST action=/update enctype=multipart/form-data>"
        "<input type=file name=fw accept=.bin><br><br><input type=submit value=Update></form>");
    });
    // both files in one download: the older part first
    server_.on("/log", HTTP_GET, [this]() {
      server_.sendHeader("Content-Disposition", "attachment; filename=wrangler_air_log.csv");
      server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
      server_.send(200, "text/csv", "");
      bool header = false;
      for (const char* p : {"/log_old.csv", "/log.csv"}) {
        File f = LittleFS.open(p, "r");
        if (!f) continue;
        String first = f.readStringUntil('\n');               // the header line goes out once
        if (!header) { server_.sendContent(first + "\n"); header = true; }
        uint8_t buf[512];
        while (f.available()) { const size_t n = f.read(buf, sizeof buf); server_.sendContent((const char*)buf, n); }
        f.close();
      }
      server_.sendContent("");
    });
    // run graph page: the browser fetches the CSV and draws it (TRACE_GRAPH_HTML_HEAD)
    server_.on("/run", HTTP_GET, [this]() {
      const String id = server_.arg("id");
      if (!validId(id)) { server_.send(404, "text/plain", "no such run"); return; }
      server_.send(200, "text/html", String(TRACE_GRAPH_HTML_HEAD) +
        "<p><a href='/runfile?id=" + id + "&dl=1'>download " + id + "</a> &nbsp; <a href=/>back</a></p>"
        "<p style=color:#666>No live graph: the pressure input can not be read while Wi-Fi is on.</p>"
        "<script>fetch('/runfile?id=" + id + "').then(function(r){return r.text()}).then(function(t){draw(t," + (psi_ && *psi_ ? "true" : "false") + ")});</script>");
    });
    server_.on("/runfile", HTTP_GET, [this]() {
      const String id = server_.arg("id");
      File f = validId(id) ? LittleFS.open("/runs/" + id, "r") : File();
      if (!f) { server_.send(404, "text/plain", "no such run"); return; }
      if (server_.hasArg("dl")) server_.sendHeader("Content-Disposition", "attachment; filename=" + id);
      server_.streamFile(f, "text/csv");
      f.close();
    });
    server_.on("/clearruns", HTTP_POST, [this]() {
      File d = LittleFS.open("/runs");
      String names[64]; int n = 0;
      if (d) { for (File f = d.openNextFile(); f && n < 64; f = d.openNextFile()) { names[n++] = String(f.name()); f.close(); } d.close(); }
      for (int i = 0; i < n; i++) LittleFS.remove("/runs/" + names[i].substring(names[i].lastIndexOf('/') + 1));
      wlog::ev("LOG", "runs_cleared=1;");
      server_.sendHeader("Location", "/"); server_.send(303);
    });
    server_.on("/clear", HTTP_POST, [this]() {
      wlog::clear(); wlog::ev("LOG", "cleared=1;");
      server_.sendHeader("Location", "/"); server_.send(303);
    });
    server_.on("/update", HTTP_POST,
      [this]() {
        const bool ok = !Update.hasError();
        server_.send(200, "text/plain", ok ? "OK, rebooting" : "Update FAILED");
        if (ok) { delay(500); ESP.restart(); }
      },
      [this]() {
        HTTPUpload& u = server_.upload();
        if (u.status == UPLOAD_FILE_START)      { Update.begin(UPDATE_SIZE_UNKNOWN); progress_ = 0; }
        else if (u.status == UPLOAD_FILE_WRITE) { Update.write(u.buf, u.currentSize); progress_ += u.currentSize; }
        else if (u.status == UPLOAD_FILE_END)   { Update.end(true); }
      });
    server_.begin();
    running_ = true;
  }
  void stop() {
    if (!running_) return;
    server_.stop(); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF);
    running_ = false;
  }
  void update() { if (running_) server_.handleClient(); }
  bool running() const { return running_; }
  size_t progress() const { return progress_; }

 private:
  // file names are r<boot>_<n>.csv: nothing else is served
  static bool validId(const String& id) {
    if (id.length() < 6 || id[0] != 'r' || !id.endsWith(".csv")) return false;
    for (size_t i = 1; i + 4 < id.length(); i++) if (!isdigit((unsigned char)id[i]) && id[i] != '_') return false;
    return true;
  }
  static String hv(const String& h, const char* k) {   // value from the "# key=value;..." header
    const String key = String(k) + "=";
    int i = h.startsWith(key) ? 0 : h.indexOf(";" + key);
    if (i < 0) { i = h.indexOf(" " + key); if (i < 0) return ""; }
    if (h[i] == ';' || h[i] == ' ') i++;
    const int e = h.indexOf(';', i);
    return h.substring(i + key.length(), e < 0 ? h.length() : e);
  }
  String runList() {
    String names[64]; int n = 0;
    File d = LittleFS.open("/runs");
    if (d) { for (File f = d.openNextFile(); f && n < 64; f = d.openNextFile()) { String nm = f.name(); names[n++] = nm.substring(nm.lastIndexOf('/') + 1); f.close(); } d.close(); }
    if (!n) return "<p>No runs yet.</p>";
    std::sort(names, names + n, [](const String& a, const String& b) { return a > b; });
    const bool psi = psi_ && *psi_;
    auto u = [&](const String& v) { return psi ? String(v.toFloat() * 14.5038f, 0) : String(v.toFloat(), 2); };
    String out = "<table border=1 cellpadding=4 style='border-collapse:collapse;font-size:13px'>"
                 "<tr><th>run</th><th>preset</th><th>target</th><th>p0 &rarr; final</th><th>result</th><th>s</th><th>starts</th><th>vents</th></tr>";
    for (int i = 0; i < n; i++) {
      File f = LittleFS.open("/runs/" + names[i], "r");
      if (!f) continue;
      const String h = f.readStringUntil('\n'); f.close();
      const String test = hv(h, "test");
      out += "<tr><td><a href='/run?id=" + names[i] + "'>" + names[i] + "</a></td><td>" + hv(h, "preset") + (test != "-" && test.length() ? " (LEARN " + test + ")" : "") +
             "</td><td>" + u(hv(h, "target")) + "</td><td>" + u(hv(h, "p0")) + " &rarr; " + u(hv(h, "p")) + "</td><td>" + hv(h, "result") +
             "</td><td>" + String(hv(h, "dur").toFloat(), 0) + "</td><td>" + hv(h, "starts") + "</td><td>" + hv(h, "vents") + "</td></tr>";
    }
    return out + "</table><p>" + String(psi ? "psi" : "bar") + "</p>";
  }

  const bool* psi_ = nullptr;
  WebServer server_{80};
  bool running_ = false;
  size_t progress_ = 0;
};
