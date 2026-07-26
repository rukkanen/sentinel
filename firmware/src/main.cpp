// sentinel_module firmware entry (Phase C — under construction).
// Runs the real SENT-LINK transport over serial + the heartbeat LED. Sensor drivers (mic
// loudness / radar / IR) and the curator (analyse + store) land as the next host-tested
// units; pins are locked in include/pins.h and shown in the wiring guide.
#include <Arduino.h>

#include "pins.h"
#include "transport.h"

using namespace sentlink;

static void serial_sink(const char* frame, size_t n, void*) {
  Serial.write((const uint8_t*)frame, n);
}

static Transport g_tp(serial_sink, nullptr);
static uint32_t tHeartbeat = 0;   // SENT-LINK liveness frame
static uint32_t tBlink = 0;       // on-board LED "I'm alive" pulse
static uint32_t ledOffAt = 0;
static bool ledOn = false;

void setup() {
  pinMode(pins::LED, OUTPUT);
  Serial.begin(115200);
  delay(50);
  g_tp.send_event("{\"boot\":\"sentinel_module fw v0\"}");
}

void loop() {
  const uint32_t now = millis();

  // Heartbeat LED: a short flash once every 5 s (was an annoying 1 Hz — owner S68).
  if (now - tBlink >= 5000) {
    tBlink = now;
    digitalWrite(pins::LED, HIGH);
    ledOn = true;
    ledOffAt = now + 60;
  }
  if (ledOn && (int32_t)(now - ledOffAt) >= 0) {
    digitalWrite(pins::LED, LOW);
    ledOn = false;
  }

  // SENT-LINK heartbeat frame (protocol liveness) — 1 Hz.
  if (now - tHeartbeat >= 1000) {
    tHeartbeat = now;
    char p[48];
    snprintf(p, sizeof(p), "{\"up\":%lu}", (unsigned long)(now / 1000));
    g_tp.send_heartbeat(p);
  }

  g_tp.tick(now);   // drive reliable-Data retransmits (none yet, but the path is live)
}
