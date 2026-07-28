// ultratest — DISPOSABLE scaffold: does the just-wired ultrasonic give numbers?
// Wiring under test (prompt 23 §6 P3): VCC→3V3 · GND→GND · TRIG→GPIO26 · ECHO→GPIO34.
// Prints one line per ping at ~5 Hz:  RANGE cm=57.3 us=3323   (or RANGE timeout)
// Keeps the HB heartbeat so wroom_check.py still recognises the board as healthy.
#include <Arduino.h>

constexpr int TRIG = 26;
constexpr int ECHO = 34;   // input-only pin — perfect for an input signal
constexpr int LED  = 2;

void setup() {
  Serial.begin(115200);
  pinMode(TRIG, OUTPUT);
  digitalWrite(TRIG, LOW);
  pinMode(ECHO, INPUT);
  pinMode(LED, OUTPUT);
  delay(50);
  Serial.println("ultratest: TRIG=26 ECHO=34, 5 Hz");
}

void loop() {
  static uint32_t hb = 0, tHb = 0, nPing = 0;

  // ping
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  // HC-SR04 family: echo HIGH duration = round-trip time. 30 ms timeout ≈ >5 m / no echo.
  const uint32_t us = pulseIn(ECHO, HIGH, 30000);
  nPing++;
  if (us == 0) {
    Serial.println("RANGE timeout");
  } else {
    Serial.printf("RANGE cm=%.1f us=%lu\n", us / 58.0, (unsigned long)us);
    digitalWrite(LED, HIGH);   // blip on a real echo
  }
  delay(30);
  digitalWrite(LED, LOW);

  // heartbeat every 2 s so the health checker still trends uptime — plus an ECHO
  // wire diagnostic: sample the idle level 100×. A connected module holds ECHO solid
  // LOW between pings; a floating (disconnected) GPIO34 reads noisy/mixed since the
  // input-only pins have no internal pulls.
  const uint32_t now = millis();
  if (now - tHb >= 2000) {
    tHb = now;
    int highs = 0;
    for (int i = 0; i < 100; i++) { highs += digitalRead(ECHO); delayMicroseconds(50); }
    const char* idle = (highs == 0) ? "LOW(connected)"
                     : (highs == 100) ? "HIGH(stuck?!)" : "FLOATING(wire off?)";
    // Continuity check for the LOOPBACK bench test (TRIG jumpered straight to ECHO):
    // hold TRIG high/low slowly and see if ECHO follows. With a module attached this
    // stays follow=0 (modules don't couple TRIG→ECHO statically) — that's expected.
    digitalWrite(TRIG, HIGH); delay(2); const int fH = digitalRead(ECHO);
    digitalWrite(TRIG, LOW);  delay(2); const int fL = digitalRead(ECHO);
    const int follow = (fH == 1 && fL == 0);
    Serial.printf("HB %lu up=%lus pings=%lu echo_idle=%s(%d/100) trig_follow=%d\n",
                  (unsigned long)hb++, (unsigned long)(now / 1000), (unsigned long)nPing,
                  idle, highs, follow);
  }
  delay(170);   // ~5 Hz total
}
