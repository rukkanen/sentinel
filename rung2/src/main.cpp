// SENTINEL sensor-check (rung 2, updated S68) — verify ALL THREE sensors + wiring live.
//   mic A0  -> GPIO32  : analog envelope -> software loudness + "loud" events (Tier A)
//   radar   -> GPIO27  : RCWL-0516 motion, HIGH on movement (debounced)
//   IR      -> GPIO4   : VS1838B NEC decode -> button-code events
//   LED     -> GPIO2   : heartbeat, one short flash every 5 s
// Frames are the SENT-LINK seed: <ver><type><seq>|<json>|<crc16>. Read it with
//   ~/.venvs/sentinel/bin/python tools/read_rung2.py <by-id>
// This is a bring-up check, not the real firmware (that's firmware/, host-tested).
#include <Arduino.h>
#define DECODE_NEC
#include <IRremote.hpp>

static const int MIC = 32, RADAR = 27, IR = 4, LED = 2;
static const uint32_t WIN_MS = 50, TELEM_MS = 500, DEBOUNCE_MS = 25;

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

static float baseline = 0;
static bool loud = false, lastRadar = false;
static uint32_t loudCount = 0, motionCount = 0, tTelem = 0, tRadar = 0;
static uint32_t tBlink = 0, ledOffAt = 0; static bool ledOn = false;

void setup() {
  pinMode(RADAR, INPUT);
  pinMode(LED, OUTPUT);
  analogReadResolution(12);
  Serial.begin(115200);
  delay(50);
  IrReceiver.begin(IR, DISABLE_LED_FEEDBACK);
  emit('D', "{\"boot\":\"sentinel sensor-check v1\",\"mic\":32,\"radar\":27,\"ir\":4}");
}

void loop() {
  const uint32_t now = millis();

  // --- mic loudness (peak-to-peak over a window; software threshold) ---
  int lo = 4095, hi = 0; uint32_t t0 = millis();
  while (millis() - t0 < WIN_MS) { int v = analogRead(MIC); if (v < lo) lo = v; if (v > hi) hi = v; }
  const int level = hi - lo;
  if (baseline == 0) baseline = level;
  baseline = 0.98f * baseline + 0.02f * level;
  const int thresh = (int)baseline + 120;
  const bool nowLoud = level > thresh;
  if (nowLoud && !loud) { loud = true; loudCount++;
    emit('E', String("{\"s\":\"sound\",\"loud\":1,\"lvl\":") + level + "}"); }
  else if (!nowLoud && loud) { loud = false;
    emit('E', String("{\"s\":\"sound\",\"loud\":0,\"lvl\":") + level + "}"); }

  // --- radar motion ---
  const bool r = digitalRead(RADAR);
  if (r != lastRadar && now - tRadar >= DEBOUNCE_MS) {
    lastRadar = r; tRadar = now; if (r) motionCount++;
    emit('E', String("{\"s\":\"radar\",\"motion\":") + (r ? 1 : 0) + "}");
  }

  // --- IR remote (NEC) ---
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.protocol != UNKNOWN &&
        !(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)) {
      emit('E', String("{\"s\":\"ir\",\"cmd\":") + IrReceiver.decodedIRData.command +
                     ",\"addr\":" + IrReceiver.decodedIRData.address + "}");
    }
    IrReceiver.resume();
  }

  // --- telemetry ---
  if (now - tTelem >= TELEM_MS) {
    tTelem = now;
    emit('T', String("{\"lvl\":") + level + ",\"base\":" + (int)baseline +
                    ",\"radar\":" + (r ? 1 : 0) + ",\"lc\":" + loudCount +
                    ",\"mc\":" + motionCount + ",\"up\":" + (now / 1000) + "}");
  }

  // --- heartbeat LED: short flash every 5 s ---
  if (now - tBlink >= 5000) { tBlink = now; digitalWrite(LED, HIGH); ledOn = true; ledOffAt = now + 60; }
  if (ledOn && (int32_t)(now - ledOffAt) >= 0) { digitalWrite(LED, LOW); ledOn = false; }
}
