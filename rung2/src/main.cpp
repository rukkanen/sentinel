// SENTINEL rung-2 — the actually-wired sensor: the MIC, listened to properly (Tier A).
// The mic's digital threshold (hardware pot) is finicky (owner S68: "constantly sending,
// or nothing unless I hit it"), so we IGNORE the pot and use the ANALOG envelope on
// GPIO32 (found by the S68 pin scan — biased ~182..670, swings with sound): sample fast,
// take peak-to-peak = loudness, auto-baseline the ambient, and fire loud-events in
// SOFTWARE. This is exactly sentinel_module_spec Tier-A listening (SENT-045) and it
// makes the pot irrelevant. Frames are the SENT-LINK seed: <ver><type><seq>|json|crc16.
//   (Radar GPIO12 is v1-planned but NOT wired yet — S68 scan saw no digital activity.)
#include <Arduino.h>

static const int MIC = 32;         // mic analog envelope (AO) — verify; S68 scan → GPIO32
static const int LED = 2;
static const uint32_t WIN_MS   = 50;    // loudness integration window
static const uint32_t TELEM_MS = 500;   // telemetry cadence

static uint32_t seq = 0;

static uint16_t crc16_ccitt(const char *s, size_t n) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    crc ^= (uint16_t)(uint8_t)s[i] << 8;
    for (int b = 0; b < 8; b++)
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}
static void emit(char type, const String &json) {
  String head = String("1") + type + String(seq++) + "|" + json;
  char crc[8];
  snprintf(crc, sizeof(crc), "%04X", crc16_ccitt(head.c_str(), head.length()));
  Serial.print(head); Serial.print('|'); Serial.println(crc);
}

static float baseline = 0;     // EMA of ambient loudness
static bool loud = false;
static uint32_t loudCount = 0, tTelem = 0;

void setup() {
  pinMode(LED, OUTPUT);
  analogReadResolution(12);
  Serial.begin(115200);
  delay(50);
  emit('D', "{\"boot\":\"sentinel rung2 mic-listen v1\",\"mic_pin\":32}");
}

void loop() {
  // Integrate the envelope: peak-to-peak of fast samples over WIN_MS = loudness.
  int lo = 4095, hi = 0;
  const uint32_t t0 = millis();
  while (millis() - t0 < WIN_MS) {
    int v = analogRead(MIC);
    if (v < lo) lo = v;
    if (v > hi) hi = v;
  }
  const int level = hi - lo;

  if (baseline == 0) baseline = level;
  baseline = 0.98f * baseline + 0.02f * level;      // slow adapt to ambient
  const int thresh = (int)baseline + 120;           // software margin (Q3 tuning)
  const bool nowLoud = level > thresh;
  const uint32_t now = millis();

  if (nowLoud && !loud) {
    loud = true; loudCount++; digitalWrite(LED, 1);
    emit('E', String("{\"s\":\"sound\",\"loud\":1,\"lvl\":") + level +
                  ",\"base\":" + (int)baseline + "}");
  } else if (!nowLoud && loud) {
    loud = false; digitalWrite(LED, 0);
    emit('E', String("{\"s\":\"sound\",\"loud\":0,\"lvl\":") + level + "}");
  }

  if (now - tTelem >= TELEM_MS) {
    tTelem = now;
    emit('T', String("{\"lvl\":") + level + ",\"base\":" + (int)baseline +
                    ",\"th\":" + thresh + ",\"lc\":" + loudCount +
                    ",\"up\":" + (now / 1000) + "}");
  }
}
