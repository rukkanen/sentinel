// irscan v2 — DISPOSABLE bring-up scaffold: learn the IR remote's button codes, cleanly.
// Plain NEC decode on GPIO4. Prints ONE line per real button; ambient-IR noise is COUNTED,
// not spammed (v1's DECODE_DISTANCE_WIDTH fallback turned every stray pulse into junk).
// Heartbeat LED GPIO2 every 5 s.
#include <Arduino.h>
#define DECODE_NEC
#include <IRremote.hpp>

static const int IR = 4, LED = 2;
static uint32_t tRep = 0, tBlink = 0, ledOffAt = 0;
static bool ledOn = false;
static uint32_t codeCount = 0, unknownCount = 0;

void setup() {
  pinMode(LED, OUTPUT);
  Serial.begin(115200);
  delay(50);
  IrReceiver.begin(IR, DISABLE_LED_FEEDBACK);
  Serial.println("irscan v2 — press each button once, slowly. (ambient noise is summarised, not spammed)");
}

void loop() {
  if (IrReceiver.decode()) {
    const auto &d = IrReceiver.decodedIRData;
    if (!(d.flags & IRDATA_FLAGS_IS_REPEAT)) {
      if (d.protocol == UNKNOWN) {
        unknownCount++;
      } else {
        codeCount++;
        Serial.print("IR  cmd=0x");
        if (d.command < 0x10) Serial.print('0');
        Serial.print(d.command, HEX);
        Serial.print("  proto=");
        Serial.print(getProtocolString(d.protocol));
        Serial.print("  (#");
        Serial.print(codeCount);
        Serial.println(")");
      }
    }
    IrReceiver.resume();
  }

  const uint32_t now = millis();
  if (now - tRep >= 3000) {
    tRep = now;
    if (unknownCount > 0) {
      Serial.print(".. ambient noise: ");
      Serial.print(unknownCount);
      Serial.print(" bursts/3s, IR-idle=");
      Serial.println(digitalRead(IR));  // a healthy VS1838B idles HIGH(1)
      unknownCount = 0;
    }
  }
  if (now - tBlink >= 5000) { tBlink = now; digitalWrite(LED, HIGH); ledOn = true; ledOffAt = now + 60; }
  if (ledOn && (int32_t)(now - ledOffAt) >= 0) { digitalWrite(LED, LOW); ledOn = false; }
}
