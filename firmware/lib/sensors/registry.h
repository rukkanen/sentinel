// Sensor-driver registry (prompt 23 Stage 3 / spec SENT-004, D-S8, owner P3).
//
// THE extension point: a new sensor = one driver + one register() call — zero rewrites,
// zero Pi changes (the Pi's Sensors view is roster-driven). Portable, host-tested:
// drivers here hold LOGIC only; the Arduino shims in src/ feed them raw signals.
//
// Contract per driver:
//   • key()            roster name ("range", "radar", "loud", "ir", "power"…)
//   • telemetry_frag() appends `"k":v[,"k2":v2…]` JSON fragments (no braces) — or 0 = silent
//     (an absent/idle sensor is silent, never a fake zero — prompt 18's honesty rule)
//   • next_event()     drains one pending event payload (full E-schema JSON) — or 0 = none
// All bounded: fixed driver slots, fixed event queues inside drivers (SENT-121).
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace sentsensors {

class Driver {
 public:
  virtual const char* key() const = 0;
  virtual size_t telemetry_frag(char* out, size_t cap) = 0;
  virtual size_t next_event(char* out, size_t cap, uint32_t now_ms) = 0;
  virtual ~Driver() {}
};

class Registry {
 public:
  static const int MAX_DRIVERS = 8;

  // False when full (bounded — never silently grows).
  bool add(Driver* d);
  int count() const { return n_; }

  // Build one T payload: {"up_ms":<now>,<frag>,<frag>…}. Drivers with nothing to say are
  // skipped. Returns len, or 0 if nothing fit / no driver spoke (caller skips the frame).
  size_t build_telemetry(uint32_t up_ms, char* out, size_t cap);

  // Drain ONE pending event from the round-robin of drivers (fair, bounded per call).
  size_t next_event(char* out, size_t cap, uint32_t now_ms);

 private:
  Driver* drivers_[MAX_DRIVERS] = {nullptr};
  int n_ = 0;
  int rr_ = 0;   // round-robin cursor for event draining
};

}  // namespace sentsensors
