// Tier-A loudness LOGIC (SENT-045, prompt 23 Stage 4). Portable; the Arduino shim reads
// the INMP441 over I²S and feeds dB values here. LEVELS ONLY — never audio content
// (D-S2/D-A3); the raw samples die in the shim's buffer.
//
//   feed_db(db, now)  → rolling stats for T frames (min/max/avg since last drain)
//                     → adaptive baseline: slow EMA that only learns when it's QUIET,
//                       so a loud hour doesn't teach the bot that loud is normal
//                     → loud-event state machine: enter at baseline+enter_margin
//                       (debounced), exit at baseline+exit_margin (hysteresis)
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace sentsensors {

class LoudLogic {
 public:
  struct Cfg {
    float enter_margin_db = 12.0f;
    float exit_margin_db  = 6.0f;
    int   debounce_n      = 3;      // consecutive loud windows before the event
    float baseline_alpha  = 0.02f;  // slow EMA (learns quiet, ~50 windows)
    float baseline_init   = 35.0f;  // sane ambient seed until it learns
  };
  LoudLogic() {}
  explicit LoudLogic(const Cfg& c) : cfg_(c), baseline_(c.baseline_init) {}

  void feed_db(float db, uint32_t now_ms);

  bool  has_data() const { return n_ > 0 || ever_; }
  float last_db() const { return last_; }
  float baseline_db() const { return baseline_; }
  bool  loud() const { return loud_; }

  // Drain rolling stats for one T frame: writes `"loud_db":x,"loud_base":y[,"loud":1]`
  // fragment; resets the window. 0 if no samples arrived since the last drain.
  size_t telemetry_frag(char* out, size_t cap);
  // One-shot loud_start/loud_end event payloads (E schema). 0 = none pending.
  size_t next_event(char* out, size_t cap, uint32_t now_ms);

 private:
  Cfg cfg_{};
  float baseline_ = 35.0f;
  float last_ = 0, wmin_ = 0, wmax_ = 0, wsum_ = 0;
  int n_ = 0;
  bool ever_ = false;
  int streak_ = 0;
  bool loud_ = false;
  int pending_ = 0;          // 1=start 2=end
  float pending_db_ = 0;
};

}  // namespace sentsensors
