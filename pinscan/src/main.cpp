// pinscan — find which GPIO the sensors are wired to. Streams digital (pulldown) +
// ADC1 levels ~7 Hz. Move (radar → a digital pin goes/stays HIGH) and make noise
// (mic → an ADC pin's level swings). Not the protocol — a bring-up diagnostic.
#include <Arduino.h>

// Safe general-purpose pins (avoid flash 6-11). 12 is a strapping pin → plain INPUT.
const int DIG[] = {4, 5, 12, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27};
const int NDIG = sizeof(DIG) / sizeof(DIG[0]);
const int ADC[] = {32, 33, 34, 35, 36, 39};  // ADC1 (always safe)
const int NADC = sizeof(ADC) / sizeof(ADC[0]);

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < NDIG; i++)
    pinMode(DIG[i], DIG[i] == 12 ? INPUT : INPUT_PULLDOWN);
  analogReadResolution(12);
  delay(50);
  Serial.println("# pinscan v1 — D|<pin=level..>|A|<pin=adc..>");
}

void loop() {
  String d, a;
  for (int i = 0; i < NDIG; i++) {
    d += String(DIG[i]) + "=" + digitalRead(DIG[i]);
    if (i < NDIG - 1) d += ",";
  }
  for (int i = 0; i < NADC; i++) {
    a += String(ADC[i]) + "=" + analogRead(ADC[i]);
    if (i < NADC - 1) a += ",";
  }
  Serial.print("D|"); Serial.print(d); Serial.print("|A|"); Serial.println(a);
  delay(140);
}
