#include "pir_logic.h"

#include <stdio.h>
#include <string.h>

namespace sentsensors {

int PirZoneSet::add_zone(const char* label, bool wired) {
  if (n_ >= MAX_ZONES || !label) return -1;
  RadarLogic::Cfg cfg;
  cfg.debounce_ms = 120;      // the HC-SR501 already holds its output ~2.5 s; this only
  cfg.episode_gap_ms = 8000;  // rejects sub-poll glitches. Long gap = one event per visit.
  z_[n_].logic = RadarLogic(cfg);
  z_[n_].label = label;
  z_[n_].wired = wired;
  z_[n_].pending = false;
  return n_++;
}

void PirZoneSet::feed(int zone, bool level, uint32_t now_ms) {
  if (zone < 0 || zone >= n_ || !z_[zone].wired) return;
  const uint32_t before = z_[zone].logic.episodes();
  z_[zone].logic.feed(level, now_ms);
  if (z_[zone].logic.episodes() > before) z_[zone].pending = true;
}

int PirZoneSet::active_labels(char* out, size_t cap) const {
  if (!out || !cap) return 0;
  out[0] = '\0';
  int active = 0;
  size_t used = 0;
  for (int i = 0; i < n_; i++) {
    if (!z_[i].wired || !z_[i].logic.motion()) continue;
    const int w = snprintf(out + used, cap - used, "%s%s", active ? "+" : "", z_[i].label);
    if (w <= 0 || used + (size_t)w >= cap) break;   // truncated tail is dropped whole
    used += (size_t)w;
    active++;
  }
  return active;
}

size_t PirZoneSet::next_event(char* out, size_t cap, uint32_t now_ms) {
  for (int i = 0; i < n_; i++) {
    if (!z_[i].pending) continue;
    z_[i].pending = false;
    const int n = snprintf(out, cap,
        "{\"s\":\"pir\",\"sev\":1,\"sum\":\"motion: %s (PIR)\",\"up_ms\":%lu,\"zone\":\"%s\"}",
        z_[i].label, (unsigned long)now_ms, z_[i].label);
    if (n <= 0 || (size_t)n >= cap) return 0;
    return (size_t)n;
  }
  return 0;
}

}  // namespace sentsensors
