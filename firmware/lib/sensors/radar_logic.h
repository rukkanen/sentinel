// RCWL-0516 microwave radar LOGIC (prompt 23 Stage 3b). Portable; the shim feeds pin levels.
// HIGH = movement. Debounced both ways; one "motion" event per rising episode (SENT-041).
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace sentsensors {

class RadarLogic {
 public:
  struct Cfg {
    uint32_t debounce_ms = 150;    // level must hold this long to count
    uint32_t episode_gap_ms = 5000;  // quiet this long ends an episode (next HIGH = new event)
  };
  RadarLogic() {}
  explicit RadarLogic(const Cfg& cfg) : cfg_(cfg) {}

  void feed(bool level, uint32_t now_ms);
  bool motion() const { return stable_; }
  uint32_t episodes() const { return episodes_; }
  size_t next_event(char* out, size_t cap, uint32_t now_ms);

 private:
  Cfg cfg_;
  bool raw_ = false, stable_ = false, pending_ = false;
  uint32_t raw_since_ = 0, last_high_ms_ = 0, episodes_ = 0;
  bool in_episode_ = false;
};

}  // namespace sentsensors
