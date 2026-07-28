// Ultrasonic ranger LOGIC (HC-SR04 at the bot's tip, 8 cm high — prompt 23 P3, Stage 3).
// Portable: the Arduino shim does the actual ping and feeds echo times here.
//
//   feed(echo_us, now_ms)  echo_us==0 means timeout/no echo
//   → median-of-3 smoothing (single-sample spikes are normal on soft/angled targets)
//   → close-obstacle state machine with hysteresis + debounce:
//       ENTER  < close_cm for `debounce_n` consecutive readings → one "obstacle" event
//       EXIT   > clear_cm (hysteresis band keeps a wobbling target from spamming)
//   Advisory only — this NEVER touches motion/ESTOP (SENT-120); lidar+contact stay safety.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace sentsensors {

class UltraLogic {
 public:
  struct Cfg {
    float close_cm = 15.0f;     // enter threshold
    float clear_cm = 25.0f;     // exit threshold (hysteresis)
    int   debounce_n = 3;       // consecutive close readings before the event
    uint32_t max_valid_us = 25000;  // > this = treat as no-echo (~4.3 m)
  };

  UltraLogic() {}
  explicit UltraLogic(const Cfg& cfg) : cfg_(cfg) {}

  void feed(uint32_t echo_us, uint32_t now_ms);

  bool has_range() const { return valid_; }
  float range_cm() const { return cm_; }
  uint32_t timeouts() const { return timeouts_; }

  // One-shot event drain: fills payload fields for an E frame, or returns 0.
  size_t next_event(char* out, size_t cap, uint32_t now_ms);

 private:
  Cfg cfg_;
  float win_[3] = {0, 0, 0};
  int win_n_ = 0, win_i_ = 0;
  float cm_ = 0;
  bool valid_ = false;
  uint32_t timeouts_ = 0, misses_ = 0;
  int close_streak_ = 0;
  bool in_close_ = false;
  // pending event: 0=none 1=obstacle 2=cleared
  int pending_ = 0;
  float pending_cm_ = 0;
};

}  // namespace sentsensors
