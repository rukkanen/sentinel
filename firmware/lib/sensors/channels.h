// Registry-facing channel drivers (portable). The Arduino shims in src/ feed raw signals
// in; these produce the telemetry fragments + events the registry drains. One class per
// sensor keeps "new sensor = one file + one register()" literally true (SENT-004 / P3).
#pragma once
#include <stdio.h>

#include "radar_logic.h"
#include "registry.h"
#include "ultra_logic.h"

namespace sentsensors {

class UltraChannel : public Driver {
 public:
  UltraLogic logic;
  const char* key() const override { return "range"; }
  size_t telemetry_frag(char* out, size_t cap) override {
    if (!logic.has_range()) return 0;              // no echo → silent, never fake zeros
    const int n = snprintf(out, cap, "\"range_cm\":%.1f", (double)logic.range_cm());
    return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
  }
  size_t next_event(char* out, size_t cap, uint32_t now_ms) override {
    return logic.next_event(out, cap, now_ms);
  }
};

class RadarChannel : public Driver {
 public:
  RadarLogic logic;
  bool wired = false;                              // stays silent until the pin is proven
  const char* key() const override { return "radar"; }
  size_t telemetry_frag(char* out, size_t cap) override {
    if (!wired) return 0;
    const int n = snprintf(out, cap, "\"radar\":%d", logic.motion() ? 1 : 0);
    return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
  }
  size_t next_event(char* out, size_t cap, uint32_t now_ms) override {
    if (!wired) return 0;
    return logic.next_event(out, cap, now_ms);
  }
};

}  // namespace sentsensors
