#include "wifi_logic.h"

namespace sentsensors {

void WifiLogic::begin(uint32_t) {
  collecting_n_ = 0;
}

void WifiLogic::add_ap(uint64_t bssid, int rssi) {
  if (collecting_n_ >= MAX_APS) return;            // bounded; strongest-first order from scan
  collecting_[collecting_n_].bssid = bssid;
  collecting_[collecting_n_].rssi = rssi;
  collecting_n_++;
}

int WifiLogic::best_rssi() const {
  int best = -127;
  for (int i = 0; i < n_; i++)
    if (cur_[i].rssi > best) best = cur_[i].rssi;
  return best;
}

void WifiLogic::commit(uint32_t) {
  // shift: cur -> prev, collecting -> cur
  for (int i = 0; i < n_; i++) prev_[i] = cur_[i];
  prev_n_ = n_;
  const bool had_prev = committed_;
  for (int i = 0; i < collecting_n_; i++) cur_[i] = collecting_[i];
  n_ = collecting_n_;
  committed_ = true;
  if (!had_prev) { have_prev_ = true; return; }    // first survey = baseline, never an event

  int appeared = 0, vanished = 0, best_prev = -127, best_now = -127;
  for (int i = 0; i < n_; i++) {
    bool known = false;
    for (int j = 0; j < prev_n_; j++)
      if (cur_[i].bssid == prev_[j].bssid) { known = true; break; }
    if (!known) appeared++;
    if (cur_[i].rssi > best_now) best_now = cur_[i].rssi;
  }
  for (int j = 0; j < prev_n_; j++) {
    bool still = false;
    for (int i = 0; i < n_; i++)
      if (prev_[j].bssid == cur_[i].bssid) { still = true; break; }
    if (!still) vanished++;
    if (prev_[j].rssi > best_prev) best_prev = prev_[j].rssi;
  }
  const int best_delta = (best_now > best_prev) ? best_now - best_prev : best_prev - best_now;
  // "Changed" = the set moved meaningfully, not one flaky beacon: ≥3 memberships flipped,
  // or the strongest signal jumped ≥15 dB (we moved, or the router did).
  if (appeared + vanished >= 3 || (prev_n_ > 0 && n_ > 0 && best_delta >= 15)) {
    pending_ = true;
    appeared_ = appeared;
    vanished_ = vanished;
  }
}

bool WifiLogic::take_change(int& appeared, int& vanished) {
  if (!pending_) return false;
  pending_ = false;
  appeared = appeared_;
  vanished = vanished_;
  return true;
}

}  // namespace sentsensors
