// SENTINEL rung-1 demo — prove the whole flash -> boot -> serial -> command loop.
// Deliberately minimal: no sensors, no protocol framing (rung 2 adds the framed
// event), no sleep. Blinks the onboard LED, prints a banner line the Pi can read,
// echoes any newline-terminated command back.

#include <Arduino.h>

static const int LED_PIN = 2; // onboard LED, same pin the legacy sketch used
static const uint32_t BLINK_MS = 500;
static const uint32_t BANNER_MS = 2000;
static const size_t RX_MAX = 120;

static uint32_t lastBlink = 0;
static uint32_t lastBanner = 0;
static uint32_t bannerSeq = 0;
static String rx;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  rx.reserve(RX_MAX);
  Serial.println("SENTINEL demo v1 boot");
}

void loop() {
  const uint32_t now = millis();

  if (now - lastBlink >= BLINK_MS) {
    lastBlink = now;
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }

  if (now - lastBanner >= BANNER_MS) {
    lastBanner = now;
    Serial.printf("SENTINEL demo v1 uptime=%lus seq=%lu\n",
                  (unsigned long)(now / 1000UL), (unsigned long)bannerSeq++);
  }

  // Non-blocking line assembly: readStringUntil() would stall the loop up to its
  // 1 s timeout on a partial line.
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      rx.trim();
      if (rx.length() > 0) {
        Serial.printf("echo:%s\n", rx.c_str());
      }
      rx = "";
    } else if (rx.length() < RX_MAX) {
      rx += c;
    }
  }
}
