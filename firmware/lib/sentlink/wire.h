// SENT-LINK wire glue — the receive-side helpers (portable, host-tested; spec SENT-026/107).
//
// Framework-free like the rest of lib/sentlink: no Arduino, no I/O, everything bounded.
//   • LineAssembler — turns a byte stream into complete lines with a HARD cap (MAX_FRAME);
//     an over-long line is dropped whole and counted, never grown (SENT-121).
//   • json_uint / json_double / json_str — minimal tolerant key extraction from the ≤200 B
//     compact-JSON payloads. NOT a JSON parser by design (prompt 23 Stage 1: "no JSON
//     library — bounded key extraction matches the frames"); keys must be top-level and
//     unique, which the SENT-LINK payload contract guarantees.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "sentlink.h"

namespace sentlink {

class LineAssembler {
 public:
  // Feed one byte. Returns true when a COMPLETE line is ready — read it via line()/len(),
  // then call reset() (or just keep feeding: the next byte resets automatically).
  bool feed(char c);
  const char* line() const { return buf_; }
  size_t len() const { return len_; }
  void reset() { len_ = 0; ready_ = false; overflowed_ = false; }
  uint32_t overflows() const { return overflows_; }
  // The buffer is mutable on purpose: decode() null-terminates the payload in place.
  char* mutable_line() { return buf_; }

 private:
  char buf_[MAX_FRAME];
  size_t len_ = 0;
  bool ready_ = false;
  bool overflowed_ = false;
  uint32_t overflows_ = 0;
};

// Extract `"key":<uint>` from a compact-JSON payload. Returns false if absent/malformed.
bool json_uint(const char* payload, const char* key, uint32_t& out);
// Extract `"key":<number>` (integer or decimal) — for set_clock's epoch_s.
bool json_double(const char* payload, const char* key, double& out);
// Extract `"key":"value"` into out (NUL-terminated, truncated to cap-1). False if absent.
bool json_str(const char* payload, const char* key, char* out, size_t cap);

}  // namespace sentlink
