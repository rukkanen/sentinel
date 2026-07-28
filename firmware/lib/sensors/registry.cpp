#include "registry.h"

#include <stdio.h>
#include <string.h>

namespace sentsensors {

bool Registry::add(Driver* d) {
  if (!d || n_ >= MAX_DRIVERS) return false;
  drivers_[n_++] = d;
  return true;
}

size_t Registry::build_telemetry(uint32_t up_ms, char* out, size_t cap) {
  if (cap < 24) return 0;
  int len = snprintf(out, cap, "{\"up_ms\":%lu", (unsigned long)up_ms);
  if (len <= 0 || (size_t)len >= cap) return 0;
  bool any = false;
  for (int i = 0; i < n_; i++) {
    char frag[96];
    const size_t fl = drivers_[i]->telemetry_frag(frag, sizeof(frag));
    if (!fl) continue;
    if ((size_t)len + 1 + fl + 2 >= cap) break;    // full frame stays bounded — drop the rest
    out[len++] = ',';
    memcpy(out + len, frag, fl);
    len += (int)fl;
    any = true;
  }
  if (!any) return 0;                              // nothing to say → no frame (honest silence)
  out[len++] = '}';
  out[len] = '\0';
  return (size_t)len;
}

size_t Registry::next_event(char* out, size_t cap, uint32_t now_ms) {
  for (int i = 0; i < n_; i++) {
    Driver* d = drivers_[rr_];
    rr_ = (rr_ + 1) % (n_ ? n_ : 1);
    const size_t n = d->next_event(out, cap, now_ms);
    if (n) return n;
  }
  return 0;
}

}  // namespace sentsensors
