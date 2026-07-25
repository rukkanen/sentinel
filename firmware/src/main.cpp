// sentinel_module firmware entry (Phase C — under construction).
// For now this only exercises the SENT-LINK codec so the ESP32 build compiles the
// portable lib (real supervisor/sensor-driver/curator wiring lands as the next units,
// spec→RED→GREEN). The mic-envelope listening logic proven in ../rung2 folds in here
// once the curator unit exists.
#include <Arduino.h>

#include "sentlink.h"

static char frame[sentlink::MAX_FRAME];
static uint32_t seq = 0;

void setup() {
  Serial.begin(115200);
  delay(50);
  size_t n = sentlink::encode(frame, sizeof(frame), sentlink::Type::Debug, seq++,
                              "{\"boot\":\"sentinel_module fw v0\"}");
  if (n) Serial.write((const uint8_t*)frame, n);
}

void loop() {
  // Placeholder heartbeat via the codec — replaced by the supervisor task next unit.
  size_t n = sentlink::encode(frame, sizeof(frame), sentlink::Type::Heartbeat, seq++,
                              "{\"up\":0}");
  if (n) Serial.write((const uint8_t*)frame, n);
  delay(1000);
}
