// WiFi survey LOGIC (prompt 24 Stage B1 / Q-D4). The WROOM's idle radio LISTENS to
// beacons (passive scan — transmits nothing) every ~60 s; this module keeps the last
// survey, streams a compact fragment ("wap" count + best RSSI) and raises ONE event when
// the radio environment genuinely changes (APs appeared/vanished or the strongest signal
// moved a lot) — that is the place-fingerprint signal, and at night an AP vanishing is
// itself worth a line in the inbox. Portable; the shim feeds (bssid, rssi) pairs.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace sentsensors {

class WifiLogic {
 public:
  static const int MAX_APS = 16;

  void begin(uint32_t now_ms);                   // start collecting a new survey
  void add_ap(uint64_t bssid, int rssi);         // repeat per visible AP
  void commit(uint32_t now_ms);                  // survey done: diff vs previous, maybe flag
  bool has_survey() const { return committed_; }
  int ap_count() const { return n_; }
  int best_rssi() const;
  bool change_pending() const { return pending_; }
  // Consumes the pending change flag; fills counts of appeared/vanished for the event.
  bool take_change(int& appeared, int& vanished);

 private:
  struct Ap { uint64_t bssid; int rssi; };
  Ap cur_[MAX_APS];
  Ap prev_[MAX_APS];
  int n_ = 0, prev_n_ = 0;
  int collecting_n_ = 0;
  Ap collecting_[MAX_APS];
  bool committed_ = false, pending_ = false, have_prev_ = false;
  int appeared_ = 0, vanished_ = 0;
};

}  // namespace sentsensors
