// Firmware update over Wi-Fi: DinMeter opens its own access point,
// the phone connects and uploads the .bin at http://192.168.4.1
// The new image goes to the other OTA partition; settings (NVS) are kept.
#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include "config.h"
#include "log.h"

class Ota {
 public:
  void start() {
    if (running_) return;
    digitalWrite(PIN_K1, LOW); if (PIN_K4 >= 0) digitalWrite(PIN_K4, LOW);   // outputs off for the whole update
    WiFi.mode(WIFI_AP);
    WiFi.softAP(OTA_SSID, OTA_PASS);
    server_.on("/", HTTP_GET, [this]() {
      server_.send(200, "text/html",
        "<meta name=viewport content='width=device-width'>"
        "<h3>Wrangler Air " FW_VERSION "</h3>"
        "<p><a href=/log>Download log (CSV, " + String(wlog::size() / 1024) + " kB)</a></p>"
        "<form method=POST action=/clear onsubmit=\"return confirm('Clear the log?')\"><input type=submit value='Clear log'></form><hr>"
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
  WebServer server_{80};
  bool running_ = false;
  size_t progress_ = 0;
};
