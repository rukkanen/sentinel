#include "radar_logic.h"

#include <stdio.h>

namespace sentsensors {

void RadarLogic::feed(bool level, uint32_t now_ms) {
  if (level != raw_) {
    raw_ = level;
    raw_since_ = now_ms;
  }
  if (now_ms - raw_since_ >= cfg_.debounce_ms && stable_ != raw_) {
    stable_ = raw_;
    if (stable_) {
      last_high_ms_ = now_ms;
      if (!in_episode_) {              // a NEW motion episode → exactly one event
        in_episode_ = true;
        episodes_++;
        pending_ = true;
      }
    }
  }
  if (stable_) last_high_ms_ = now_ms;
  if (in_episode_ && !stable_ && now_ms - last_high_ms_ >= cfg_.episode_gap_ms)
    in_episode_ = false;               // quiet long enough — the next HIGH is a fresh event
}

size_t RadarLogic::next_event(char* out, size_t cap, uint32_t now_ms) {
  if (!pending_) return 0;
  pending_ = false;
  const int n = snprintf(out, cap,
      "{\"s\":\"radar\",\"sev\":1,\"sum\":\"motion detected (microwave)\",\"up_ms\":%lu}",
      (unsigned long)now_ms);
  if (n <= 0 || (size_t)n >= cap) return 0;
  return (size_t)n;
}

}  // namespace sentsensors
