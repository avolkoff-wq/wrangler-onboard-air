// Bluetooth LE link for the phone app (decisions.md 2026-11-24, variant B): the DinMeter sends the screen as data
// (UiSnapshot, ui_state.h) and takes the same inputs as the knob through input::send(). All UI state, menus and
// protections stay in the firmware; a lost link changes nothing in a run.
//   service 7a1f0001-5c0e-4b6a-9d2f-57a1e5a1c0de
//   STATE   7a1f0002-...  read + notify, open: any phone may watch. 36 bytes, little endian (pack() below); bytes 34-35 p0 (0.7.2)
//   INPUT   7a1f0003-...  write, needs an authenticated encrypted link (a bonded phone): 1 byte,
//                         1 = left, 2 = right, 3 = press, 4 = medium press, 5 = hold, 6 = back (one level up),
//                         9 = send the preset icon again
//                         SCREEN also carries 'M' blobs: the current menu level as a list (0.7.3)
//   SCREEN  7a1f0004-...  notify, open: the display list of the current screen (dlist.h), sent in chunks
//                         [frame id, chunk index, chunk count] + up to MTU - 6 bytes; the phone joins them by frame id
// Pairing: passkey entry, the DinMeter is display-only. Menu BLUETOOTH > PAIR shows a random 6-digit PIN for 60 s.
// Outside that window the passkey is random and never shown, so no new phone can bond. Up to 3 bonds (NimBLE default).
// NimBLE callbacks run in the BLE host task: they only set flags, read the snapshot under its mutex and write the input
// queue. Logging and NimBLE calls from the menu happen in loop() (update()).
#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "ui_state.h"
#include "input.h"
#include "log.h"
#include "dlist.h"

namespace ble {
constexpr char SVC_UUID[]   = "7a1f0001-5c0e-4b6a-9d2f-57a1e5a1c0de";
constexpr char STATE_UUID[] = "7a1f0002-5c0e-4b6a-9d2f-57a1e5a1c0de";
constexpr char INPUT_UUID[] = "7a1f0003-5c0e-4b6a-9d2f-57a1e5a1c0de";
constexpr char SCREEN_UUID[] = "7a1f0004-5c0e-4b6a-9d2f-57a1e5a1c0de";
constexpr uint32_t SCREEN_MS = 150, SCREEN_IDLE_MS = 2000;   // a changed screen at most every 150 ms, an unchanged one every 2 s
constexpr uint32_t PAIR_MS = 60000, NOTIFY_MS = 200, NOTIFY_IDLE_MS = 1000;
constexpr uint8_t PROTO = 1;

inline bool on = false;
inline NimBLEServer* server = nullptr;
inline NimBLECharacteristic* stateCh = nullptr;
inline NimBLECharacteristic* screenCh = nullptr;
inline volatile bool screenNow = false;   // a phone connected: send the screen at once
inline size_t (*menuSource)(uint8_t*, size_t) = nullptr;   // the menu as a list for the phone (ui_screens.h menuBlob)
inline volatile bool iconReset = false;   // the phone asked for the icons again (INPUT 9), or a new phone
inline uint32_t pin = 0, pairUntil = 0;
// set in the host task, logged in loop()
inline volatile bool evConn = false, evDisc = false, evPaired = false, evDenied = false;
inline volatile uint32_t inputs = 0;      // accepted phone inputs, for `status`
inline volatile int discReason = 0, authInfo = 0;   // for the log: NimBLE disconnect reason; enc | auth << 1 | bonded << 2 | key size << 4

inline bool pairing() { return pairUntil && (int32_t)(pairUntil - millis()) > 0; }
inline uint32_t pairLeftMs() { return pairing() ? pairUntil - millis() : 0; }
inline int connected() { return on && server ? server->getConnectedCount() : 0; }
inline int bonds() { return on ? NimBLEDevice::getNumBonds() : 0; }

// STATE packet, 42 bytes (36 until 0.7.3; 36-37 CUSTOM value mbar, 38 step, 39 wheels; 40 sound counter, 41 sound id)
inline size_t pack(const UiSnapshot& u, uint8_t* b) {
  auto p16 = [&](int i, int32_t v) { v = constrain(v, -32768, 32767); b[i] = v & 0xFF; b[i + 1] = (v >> 8) & 0xFF; };
  auto p32 = [&](int i, uint32_t v) { for (int k = 0; k < 4; k++) b[i + k] = (v >> (8 * k)) & 0xFF; };
  b[0] = PROTO; b[1] = u.screen; b[2] = u.menuPos; b[3] = u.preset; b[4] = u.state; b[5] = u.err; b[6] = u.armStage;
  b[7] = (u.k1 ? 1 : 0) | (u.valve ? 2 : 0) | (u.noAir ? 4 : 0) | (u.inBand ? 8 : 0) | (u.offTarget ? 16 : 0) | (u.fake ? 32 : 0) | (u.demo ? 64 : 0);
  p16(8, lroundf(u.bar * 1000)); p16(10, lroundf(u.est * 1000)); p16(12, lroundf(u.target * 1000));
  p16(14, lroundf(u.staticBar * 1000)); p16(16, lroundf(u.aux1 * 100));
  p32(18, u.runMs); p32(22, u.phaseMs); p32(26, u.seq); p32(30, u.adcFails);
  p16(34, lroundf(u.p0 * 1000));   // start pressure of the run, mbar (0.7.2)
  p16(36, lroundf(u.customEdit * 1000)); b[38] = u.customStep; b[39] = u.customWheels;   // CUSTOM editor (0.7.4)
  b[40] = g_sndSeq; b[41] = g_sndId;   // the last sound (sound.h mark), for the phone to play too (0.7.6)
  return 42;
}

class ServerCb : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override { evConn = true; screenNow = true; iconReset = true; }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override { discReason = reason; evDisc = true; }
  uint32_t onPassKeyDisplay() override { return pairing() ? pin : (esp_random() % 1000000); }   // no window: nobody knows it
  void onAuthenticationComplete(NimBLEConnInfo& ci) override {
    authInfo = (ci.isEncrypted() ? 1 : 0) | (ci.isAuthenticated() ? 2 : 0) | (ci.isBonded() ? 4 : 0) | (ci.getSecKeySize() << 4);
    if (ci.isEncrypted() && ci.isAuthenticated()) { if (pairing()) { evPaired = true; pairUntil = 0; } }
    else evDenied = true;
  }
};
class InputCb : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& ci) override {
    if (!ci.isEncrypted() || !ci.isAuthenticated()) return;          // the stack already refuses, this is the belt
    const std::string v = c->getValue();
    if (v.size() != 1) return;
    bool ok = false;
    switch ((uint8_t)v[0]) {
      case 1: ok = input::send(-1, input::Btn::NONE, input::Src::PHONE); break;
      case 2: ok = input::send(1, input::Btn::NONE, input::Src::PHONE); break;
      case 3: ok = input::send(0, input::Btn::SHORT, input::Src::PHONE); break;
      case 4: ok = input::send(0, input::Btn::MID, input::Src::PHONE); break;
      case 5: ok = input::send(0, input::Btn::LONG, input::Src::PHONE); break;
      case 6: ok = input::send(0, input::Btn::BACK, input::Src::PHONE); break;   // the phone back key: one level up
      case 9: iconReset = true; ok = true; break;                                  // the phone lost an icon: send again
    }
    if (ok) inputs = inputs + 1;
  }
};
inline ServerCb serverCb; inline InputCb inputCb;

inline void begin() {
  if (on) return;
  NimBLEDevice::init("WranglerAir");
  NimBLEDevice::setSecurityAuth(true, true, true);                    // bond, MITM (passkey), secure connections
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
  server = NimBLEDevice::createServer();
  server->setCallbacks(&serverCb, false);
  server->advertiseOnDisconnect(true);
  NimBLEService* svc = server->createService(SVC_UUID);
  stateCh = svc->createCharacteristic(STATE_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  NimBLECharacteristic* in = svc->createCharacteristic(INPUT_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);
  in->setCallbacks(&inputCb);
  screenCh = svc->createCharacteristic(SCREEN_UUID, NIMBLE_PROPERTY::NOTIFY);
  svc->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SVC_UUID); adv->setName("WranglerAir"); adv->enableScanResponse(true);
  adv->start();
  on = true;
  wlog::ev("BT", wlog::kv("on", String(1)) + wlog::kv("bonds", String(NimBLEDevice::getNumBonds())));
}
inline void end() {
  if (!on) return;
  pairUntil = 0; on = false; stateCh = nullptr; screenCh = nullptr; server = nullptr; dl::on = false;
  NimBLEDevice::deinit(true);
  wlog::ev("BT", wlog::kv("on", String(0)));
}
// menu PAIR: false = three phones are bonded already (FORGET ALL first)
inline bool pairStart() {
  if (!on || NimBLEDevice::getNumBonds() >= 3) return false;
  pin = 100000 + esp_random() % 900000; pairUntil = millis() + PAIR_MS; if (!pairUntil) pairUntil = 1;
  wlog::ev("BT_PAIR", "window=60;");
  return true;
}
inline void pairStop() { pairUntil = 0; }
inline void forgetAll() {
  if (!on) return;
  NimBLEDevice::deleteAllBonds();
  for (auto h : server->getPeerDevices()) server->disconnect(h);
  wlog::ev("BT_FORGET", "all=1;");
}
// every loop pass: log what the host task flagged, notify the screen (on a change at most every NOTIFY_MS, else 1 / s)
// outgoing blob (frame 'D' or icon 'I') in chunks [id, index, count] + data that fit the smallest MTU. Sent a few
// chunks per loop pass; a chunk the stack can not take (no buffers) is tried again on the next pass, so nothing is lost.
inline uint8_t out[3400]; inline size_t outN = 0, outPos = 0; inline uint8_t outId = 0, outIdx = 0, outCount = 0; inline size_t outRoom = 0;
inline bool outBusy() { return outN && outIdx < outCount; }
inline void queueBlob(const uint8_t* buf, size_t n) {
  if (!n || n > sizeof out) return;
  memcpy(out, buf, n); outN = n; outId++; if (out[0] == 'D' && n > 2) out[2] = outId;
  uint16_t mtu = 517;
  for (auto h : server->getPeerDevices()) mtu = min(mtu, server->getPeerMTU(h));
  outRoom = mtu > 30 ? mtu - 3 - 3 : 17;
  outCount = (n + outRoom - 1) / outRoom; outIdx = 0;
}
inline void pumpOut() {
  static uint8_t chunk[520];
  for (int k = 0; k < 4 && outBusy(); k++) {
    const size_t off = outIdx * outRoom, len = min(outRoom, outN - off);
    chunk[0] = outId; chunk[1] = outIdx; chunk[2] = outCount; memcpy(chunk + 3, out + off, len);
    if (!screenCh->notify(chunk, len + 3)) return;   // try this chunk again next pass
    outIdx++;
  }
}
// the newest display-list frame; before it the preset icon once per connection and icon (the phone caches it by key)
inline void sendScreen() {
  static uint32_t last = 0, lastFrame = 0; static uint8_t prev[1200]; static size_t prevN = 0;
  static uint16_t sentKeys[8] = {0}; static uint8_t sentAt = 0;
  if (!screenCh || !connected()) { outN = 0; return; }
  pumpOut();
  if (outBusy() || dl::frames == lastFrame) return;
  static uint8_t mPrev[1400]; static size_t mPrevN = 0;
  if (iconReset) { memset(sentKeys, 0, sizeof sentKeys); iconReset = false; mPrevN = 0; }   // once, not while screenNow waits
  if (menuSource) {   // the menu level as a list, when it changed (cursor, values, level)
    static uint8_t mb[1400];
    const size_t mn = menuSource(mb, sizeof mb);
    if (!mn) mPrevN = 0;
    else if (mn != mPrevN || memcmp(mb, mPrev, mn)) { memcpy(mPrev, mb, mn); mPrevN = mn; queueBlob(mb, mn); pumpOut(); return; }
  }
  const uint16_t key = dl::icKey;
  bool known = !key;
  for (uint16_t k : sentKeys) if (k == key) known = true;
  if (!known) {   // the icon first, the frame on a later pass
    static uint8_t ib[3300];
    queueBlob(ib, dl::icEncode(ib, sizeof ib));
    sentKeys[sentAt++ % 8] = key;
    pumpOut();
    return;
  }
  const uint32_t now = millis();
  static uint8_t buf[1200];
  const size_t n = dl::encode(buf, sizeof buf, 0);
  const bool changed = n != prevN || memcmp(buf + 3, prev + 3, n > 3 ? n - 3 : 0) != 0;   // byte 2 is the frame id
  if (!screenNow && now - last < (changed ? SCREEN_MS : SCREEN_IDLE_MS)) return;
  screenNow = false; last = now; lastFrame = dl::frames;
  memcpy(prev, buf, n); prevN = n;
  queueBlob(buf, n); pumpOut();
}
inline void update(const UiSnapshot& u) {
  dl::on = dl::force || (on && connected() > 0);
  if (!on) return;
  sendScreen();
  if (evConn)   { evConn = false;   wlog::ev("BT_CONNECT", wlog::kv("n", String(connected()))); }
  if (evDisc)   { evDisc = false;   wlog::ev("BT_DISCONNECT", wlog::kv("n", String(connected())) + wlog::kv("reason", String((int)discReason, HEX))); }
  if (evPaired) { evPaired = false; wlog::ev("BT_PAIRED", wlog::kv("bonds", String(NimBLEDevice::getNumBonds())) + wlog::kv("auth", String((int)authInfo, HEX))); }
  if (evDenied) { evDenied = false; wlog::ev("BT_DENIED", wlog::kv("auth", String((int)authInfo, HEX))); }
  if (pairUntil && !pairing()) pairUntil = 0;
  static uint32_t last = 0; static uint8_t prev[42] = {0};
  uint8_t b[42]; pack(u, b);
  const uint32_t now = millis();
  // seq (26..29) and the phase / run clocks change every pass: compare the rest
  const bool changed = memcmp(b, prev, 18) != 0 || memcmp(b + 36, prev + 36, 4) != 0;
  const bool snd = b[40] != prev[40];   // a new sound goes out at once, so the phone plays it in time
  if (!stateCh || (!snd && now - last < (changed ? NOTIFY_MS : NOTIFY_IDLE_MS))) return;
  last = now; memcpy(prev, b, 42);
  stateCh->setValue(b, 42);
  if (connected()) stateCh->notify();
}
}  // namespace ble
