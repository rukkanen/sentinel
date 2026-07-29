// HC-SR501 PIR zones LOGIC (prompt 24 Stage B1 / Q-D0b). Up to four directional PIRs,
// each on its own pin (33/35/36/39) — identity IS the value: the zone label gives the
// place model a coarse motion BEARING. Same debounce/episode discipline as the radar
// (one event per motion episode); a zone stays fully silent until its `wired` flag is
// proven true (floating input-only pins fake motion).
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "radar_logic.h"

namespace sentsensors {

class PirZoneSet {
 public:
  static const int MAX_ZONES = 4;

  struct Zone {
    RadarLogic logic;      // debounce + one-event-per-episode, identical semantics
    const char* label;
    bool wired;
    bool pending;          // an episode started, event not yet drained
  };

  // Returns the zone index, or -1 when full. `label` must outlive the set (string literal).
  int add_zone(const char* label, bool wired);
  void feed(int zone, bool level, uint32_t now_ms);
  // Writes "front+rear" style list of currently-active wired zones. Returns count active.
  int active_labels(char* out, size_t cap) const;
  // One pending zone event per call: {"s":"pir",...,"sum":"motion: <label> (PIR)"}.
  size_t next_event(char* out, size_t cap, uint32_t now_ms);
  int zones() const { return n_; }
  const Zone& zone(int i) const { return z_[i]; }

 private:
  Zone z_[MAX_ZONES];
  int n_ = 0;
};

}  // namespace sentsensors
