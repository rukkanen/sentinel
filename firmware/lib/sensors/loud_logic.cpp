#include "loud_logic.h"

#include <stdio.h>

namespace sentsensors {

void LoudLogic::feed_db(float db, uint32_t now_ms) {
  (void)now_ms;
  last_ = db;
  ever_ = true;
  if (n_ == 0) { wmin_ = wmax_ = db; wsum_ = 0; }
  if (db < wmin_) wmin_ = db;
  if (db > wmax_) wmax_ = db;
  wsum_ += db;
  n_++;

  const float enter = baseline_ + cfg_.enter_margin_db;
  const float exit_ = baseline_ + cfg_.exit_margin_db;
  if (!loud_) {
    if (db >= enter) {
      if (++streak_ >= cfg_.debounce_n) {
        loud_ = true;
        streak_ = 0;
        pending_ = 1;
        pending_db_ = db;
      }
    } else {
      streak_ = 0;
      // learn the quiet — ONLY the quiet (a loud hour must not become "normal")
      baseline_ += cfg_.baseline_alpha * (db - baseline_);
    }
  } else if (db <= exit_) {
    loud_ = false;
    pending_ = 2;
    pending_db_ = db;
  }
}

size_t LoudLogic::telemetry_frag(char* out, size_t cap) {
  if (n_ == 0) return 0;
  const int n = snprintf(out, cap, "\"loud_db\":%.1f,\"loud_base\":%.1f%s",
                         (double)wmax_, (double)baseline_, loud_ ? ",\"loud\":1" : "");
  n_ = 0;
  return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
}

size_t LoudLogic::next_event(char* out, size_t cap, uint32_t now_ms) {
  if (!pending_) return 0;
  const int kind = pending_;
  pending_ = 0;
  int n;
  if (kind == 1) {
    n = snprintf(out, cap,
                 "{\"s\":\"loud\",\"sev\":2,\"sum\":\"loud sound: %.0f dB (%.0f over normal)\","
                 "\"up_ms\":%lu,\"db\":%.1f,\"base\":%.1f}",
                 (double)pending_db_, (double)(pending_db_ - baseline_),
                 (unsigned long)now_ms, (double)pending_db_, (double)baseline_);
  } else {
    n = snprintf(out, cap,
                 "{\"s\":\"loud\",\"sev\":0,\"sum\":\"sound back to normal (%.0f dB)\","
                 "\"up_ms\":%lu,\"db\":%.1f}",
                 (double)pending_db_, (unsigned long)now_ms, (double)pending_db_);
  }
  return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
}

}  // namespace sentsensors
