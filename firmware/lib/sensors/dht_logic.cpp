#include "dht_logic.h"

namespace sentsensors {

bool DhtLogic::decode(const uint8_t b[5], float& temp_c, float& rh) {
  const uint8_t sum = (uint8_t)(b[0] + b[1] + b[2] + b[3]);
  if (sum != b[4]) return false;
  const float t = (float)b[2] + (float)b[3] * 0.1f;
  const float h = (float)b[0] + (float)b[1] * 0.1f;
  // DHT11 physical envelope (datasheet 0..50 °C / 20..90 %RH, with margin): anything
  // outside is a mis-sampled wire that happened to checksum, not weather.
  if (t < -5.0f || t > 55.0f) return false;
  if (h < 5.0f || h > 95.0f) return false;
  temp_c = t;
  rh = h;
  return true;
}

void DhtLogic::feed_frame(const uint8_t b[5], uint32_t now_ms) {
  float t, h;
  if (!decode(b, t, h)) {
    feed_fail(now_ms);
    return;
  }
  temp_ = t;
  rh_ = h;
  last_ok_ms_ = now_ms;
  fails_ = 0;
  have_ = true;
}

void DhtLogic::feed_fail(uint32_t) {
  if (fails_ < 1000) fails_++;
}

bool DhtLogic::has_reading(uint32_t now_ms) const {
  if (!have_) return false;
  if (fails_ >= cfg_.max_fails) return false;
  if (now_ms - last_ok_ms_ > cfg_.stale_ms) return false;
  return true;
}

}  // namespace sentsensors
