// sentinel_module firmware entry (Phase C — under construction).
// Uses the SENT-LINK transport over serial. Sensor drivers + the curator (analyse/store)
// land as the next host-tested units; the mic-envelope listening proven in ../rung2 folds
// into the curator. For now: boot banner + 1 Hz heartbeat through the real transport, so
// the ESP32 build exercises the whole comms stack (codec + transport).
#include <Arduino.h>

#include "transport.h"

using namespace sentlink;

static void serial_sink(const char* frame, size_t n, void*) {
  Serial.write((const uint8_t*)frame, n);
}

static Transport g_tp(serial_sink, nullptr);
static uint32_t tHeartbeat = 0;

void setup() {
  Serial.begin(115200);
  delay(50);
  g_tp.send_event("{\"boot\":\"sentinel_module fw v0\"}");
}

void loop() {
  const uint32_t now = millis();
  if (now - tHeartbeat >= 1000) {
    tHeartbeat = now;
    char p[48];
    snprintf(p, sizeof(p), "{\"up\":%lu}", (unsigned long)(now / 1000));
    g_tp.send_heartbeat(p);
  }
  g_tp.tick(now);   // drive reliable-Data retransmits (none yet, but the path is live)
}
