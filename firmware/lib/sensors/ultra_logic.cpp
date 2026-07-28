#include "ultra_logic.h"

#include <stdio.h>

namespace sentsensors {

static float med3(float a, float b, float c) {
  if (a > b) { float t = a; a = b; b = t; }
  if (b > c) { float t = b; b = c; c = t; }
  if (a > b) { float t = a; a = b; b = t; }
  return b;
}

void UltraLogic::feed(uint32_t echo_us, uint32_t now_ms) {
  (void)now_ms;
  if (echo_us == 0 || echo_us > cfg_.max_valid_us) {
    timeouts_ += (echo_us == 0);
    // A few misses don't kill the reading (soft targets drop echoes); a run of them does.
    if (++misses_ >= 5) { valid_ = false; close_streak_ = 0; }
    return;
  }
  misses_ = 0;
  const float cm = echo_us / 58.0f;
  win_[win_i_] = cm;
  win_i_ = (win_i_ + 1) % 3;
  if (win_n_ < 3) win_n_++;
  cm_ = (win_n_ < 3) ? cm : med3(win_[0], win_[1], win_[2]);
  valid_ = true;

  // Close-obstacle state machine (hysteresis + debounce).
  if (!in_close_) {
    if (cm_ < cfg_.close_cm) {
      if (++close_streak_ >= cfg_.debounce_n) {
        in_close_ = true;
        close_streak_ = 0;
        pending_ = 1;
        pending_cm_ = cm_;
      }
    } else {
      close_streak_ = 0;
    }
  } else if (cm_ > cfg_.clear_cm) {
    in_close_ = false;
    pending_ = 2;
    pending_cm_ = cm_;
  }
}

size_t UltraLogic::next_event(char* out, size_t cap, uint32_t now_ms) {
  if (!pending_) return 0;
  const int kind = pending_;
  pending_ = 0;
  int n;
  if (kind == 1) {
    n = snprintf(out, cap,
                 "{\"s\":\"range\",\"sev\":1,\"sum\":\"obstacle at %.0f cm (tip, 8 cm high)\","
                 "\"up_ms\":%lu,\"cm\":%.1f}",
                 (double)pending_cm_, (unsigned long)now_ms, (double)pending_cm_);
  } else {
    n = snprintf(out, cap,
                 "{\"s\":\"range\",\"sev\":0,\"sum\":\"obstacle cleared (%.0f cm)\","
                 "\"up_ms\":%lu,\"cm\":%.1f}",
                 (double)pending_cm_, (unsigned long)now_ms, (double)pending_cm_);
  }
  if (n <= 0 || (size_t)n >= cap) return 0;
  return (size_t)n;
}

}  // namespace sentsensors
