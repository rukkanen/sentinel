#include "wire.h"

#include <stdlib.h>
#include <string.h>

namespace sentlink {

bool LineAssembler::feed(char c) {
  if (ready_) reset();                       // previous line was consumed — start fresh
  if (c == '\n' || c == '\r') {
    if (overflowed_) {                       // the over-long line ends here: drop it whole
      overflows_++;
      reset();
      return false;
    }
    if (len_ == 0) return false;             // bare newline / CRLF tail — nothing to hand up
    buf_[len_] = '\0';
    ready_ = true;
    return true;
  }
  if (overflowed_) return false;             // keep discarding until the terminator
  if (len_ >= sizeof(buf_) - 1) {            // would overflow: poison this line (SENT-121)
    overflowed_ = true;
    return false;
  }
  buf_[len_++] = c;
  return false;
}

// Find `"key":` at top level and return a pointer just past the ':' (or nullptr).
static const char* find_value(const char* payload, const char* key) {
  if (!payload || !key) return nullptr;
  const size_t klen = strlen(key);
  const char* p = payload;
  while ((p = strstr(p, key)) != nullptr) {
    // must be a quoted key: "key" immediately followed by optional spaces and ':'
    if (p > payload && p[-1] == '"' && p[klen] == '"') {
      const char* q = p + klen + 1;
      while (*q == ' ') q++;
      if (*q == ':') {
        q++;
        while (*q == ' ') q++;
        return q;
      }
    }
    p += 1;
  }
  return nullptr;
}

bool json_uint(const char* payload, const char* key, uint32_t& out) {
  const char* v = find_value(payload, key);
  if (!v || *v < '0' || *v > '9') return false;
  char* end = nullptr;
  const unsigned long n = strtoul(v, &end, 10);
  if (end == v) return false;
  out = (uint32_t)n;
  return true;
}

bool json_double(const char* payload, const char* key, double& out) {
  const char* v = find_value(payload, key);
  if (!v) return false;
  char* end = nullptr;
  const double d = strtod(v, &end);
  if (end == v) return false;
  out = d;
  return true;
}

bool json_str(const char* payload, const char* key, char* out, size_t cap) {
  const char* v = find_value(payload, key);
  if (!v || *v != '"' || cap == 0) return false;
  v++;
  size_t i = 0;
  while (*v && *v != '"' && i < cap - 1) out[i++] = *v++;
  out[i] = '\0';
  return *v == '"';                          // an unterminated string is malformed → false
}

}  // namespace sentlink
