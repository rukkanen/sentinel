// Registry-facing channel drivers (portable). The Arduino shims in src/ feed raw signals
// in; these produce the telemetry fragments + events the registry drains. One class per
// sensor keeps "new sensor = one file + one register()" literally true (SENT-004 / P3).
#pragma once
#include <stdio.h>
#include <string.h>

#include "dht_logic.h"
#include "loud_logic.h"
#include "pir_logic.h"
#include "radar_logic.h"
#include "registry.h"
#include "ultra_logic.h"
#include "wifi_logic.h"

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

class LoudChannel : public Driver {
 public:
  LoudLogic logic;
  bool muted = false;      // owner privacy switch (IR 🔇) — the shim ALSO stops sampling
  const char* key() const override { return "loud"; }
  size_t telemetry_frag(char* out, size_t cap) override {
    if (muted) {
      const int n = snprintf(out, cap, "\"mic_muted\":1");
      return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
    }
    return logic.telemetry_frag(out, cap);
  }
  size_t next_event(char* out, size_t cap, uint32_t now_ms) override {
    if (muted) return 0;
    return logic.next_event(out, cap, now_ms);
  }
};

class DhtChannel : public Driver {
 public:
  DhtLogic logic;
  uint32_t now_ms = 0;                             // the shim stamps this before each frag
  const char* key() const override { return "temp"; }
  size_t telemetry_frag(char* out, size_t cap) override {
    if (!logic.has_reading(now_ms)) return 0;      // absent/failed sensor → honest silence
    const int n = snprintf(out, cap, "\"temp_c\":%.1f,\"rh\":%.0f",
                           (double)logic.temp_c(), (double)logic.rh());
    return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
  }
  size_t next_event(char*, size_t, uint32_t) override { return 0; }  // baselines live on the Pi
};

class PirChannel : public Driver {
 public:
  const char* key() const override { return "pir"; }
  int add_zone(const char* label, bool wired) { return zones_.add_zone(label, wired); }
  void feed(int zone, bool level, uint32_t now_ms) { zones_.feed(zone, level, now_ms); }
  size_t telemetry_frag(char* out, size_t cap) override {
    char labels[96];
    if (zones_.active_labels(labels, sizeof(labels)) == 0) return 0;   // all quiet → silent
    const int n = snprintf(out, cap, "\"pir\":\"%s\"", labels);
    return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
  }
  size_t next_event(char* out, size_t cap, uint32_t now_ms) override {
    return zones_.next_event(out, cap, now_ms);
  }
  int zones() const { return zones_.zones(); }

 private:
  PirZoneSet zones_;
};

class WifiChannel : public Driver {
 public:
  WifiLogic logic;
  const char* key() const override { return "wifi"; }
  size_t telemetry_frag(char* out, size_t cap) override {
    if (!logic.has_survey() || logic.ap_count() == 0) return 0;
    const int n = snprintf(out, cap, "\"wap\":%d,\"wrssi\":%d",
                           logic.ap_count(), logic.best_rssi());
    return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
  }
  size_t next_event(char* out, size_t cap, uint32_t now_ms) override {
    int appeared = 0, vanished = 0;
    if (!logic.take_change(appeared, vanished)) return 0;
    const int n = snprintf(out, cap,
        "{\"s\":\"wifi\",\"sev\":1,\"sum\":\"radio environment changed: +%d/-%d APs\","
        "\"up_ms\":%lu}", appeared, vanished, (unsigned long)now_ms);
    return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
  }
};

class IrChannel : public Driver {
 public:
  const char* key() const override { return "ir"; }
  void press(uint8_t cmd, const char* name, uint32_t now_ms) {
    snprintf(last_, sizeof(last_), "%s", name);
    last_cmd_ = cmd;
    last_ms_ = now_ms;
    fresh_ = true;
    pending_ = true;
  }
  size_t telemetry_frag(char* out, size_t cap) override {
    if (!fresh_) return 0;                      // only carry a button while it's recent
    const int n = snprintf(out, cap, "\"ir\":\"%s\"", last_);
    return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
  }
  size_t next_event(char* out, size_t cap, uint32_t now_ms) override {
    if (!pending_) return 0;
    pending_ = false;
    const int n = snprintf(out, cap,
        "{\"s\":\"ir\",\"sev\":0,\"sum\":\"remote: %s\",\"up_ms\":%lu,\"cmd\":%u}",
        last_, (unsigned long)now_ms, (unsigned)last_cmd_);
    return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
  }
  void age(uint32_t now_ms) { if (fresh_ && now_ms - last_ms_ > 5000) fresh_ = false; }

 private:
  char last_[16] = "";
  uint8_t last_cmd_ = 0;
  uint32_t last_ms_ = 0;
  bool fresh_ = false, pending_ = false;
};

}  // namespace sentsensors
