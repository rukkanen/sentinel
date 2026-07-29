// DHT11 temp+humidity LOGIC (prompt 24 Stage B1). Portable; the Arduino shim in src/
// bit-bangs the wire and hands the 5 raw bytes here. Checksum + plausibility gate every
// frame; three consecutive failures (or a 30 s silence) → honest no-reading, never a
// stale lie. Sensor: DHT11 on GPIO16, coarse by design (±2 °C / ±5 %RH) — good enough
// for the place model's "is it fan season" question.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace sentsensors {

class DhtLogic {
 public:
  struct Cfg {
    uint32_t stale_ms;   // no accepted frame for this long → no reading
    int max_fails;       // consecutive bad reads that kill trust in the last value
    Cfg() : stale_ms(30000), max_fails(3) {}
  };
  DhtLogic() {}
  explicit DhtLogic(const Cfg& cfg) : cfg_(cfg) {}

  // Pure frame decode: 5 bytes {rh_int, rh_dec, t_int, t_dec, checksum}. False on bad
  // checksum OR values outside what a DHT11 can physically say (0..50 °C, 5..95 %RH).
  static bool decode(const uint8_t b[5], float& temp_c, float& rh);

  void feed_frame(const uint8_t b[5], uint32_t now_ms);   // decode + accept/refuse
  void feed_fail(uint32_t now_ms);                        // a read that never decoded
  bool has_reading(uint32_t now_ms) const;
  float temp_c() const { return temp_; }
  float rh() const { return rh_; }

 private:
  Cfg cfg_;
  float temp_ = 0, rh_ = 0;
  uint32_t last_ok_ms_ = 0;
  int fails_ = 0;
  bool have_ = false;
};

}  // namespace sentsensors
