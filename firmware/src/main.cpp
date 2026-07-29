// sentinel_module firmware entry (prompt 23 Stage 1: contract conformance + the receive path).
//
// What this stage speaks, and the Pi already understands (rosbottiNG sentinel_bridge.py):
//   → H every 1 s: {"up_ms","fw","proto"}   — the Pi's liveness + clock-rebase reference
//   → R/N answering every Pi C command      — ping/describe/set_clock/flush/sensors_on/off/reboot
//   ← C commands, R/N acks for our B frames — routed by the receiver below
// Sensor drivers (ultrasonic/radar/mic/IR) land as registry citizens in Stage 3+ — until
// then telemetry is honest by ABSENCE (no fake channels), and the dashboard shows the link
// itself going up: sentinel_health absent→up is this stage's whole acceptance test.
#include <Arduino.h>
#include <I2S.h>
#include <WiFi.h>
#define DECODE_NEC
#include <IRremote.hpp>

#include "channels.h"
#include "commands.h"
#include "pins.h"
#include "registry.h"
#include "sentlink.h"
#include "transport.h"
#include "wire.h"

using namespace sentlink;

static const char* FW_VERSION = "0.4.0";

// ---- sensor registry (Stage 3): a new sensor = one channel object + one add() ----
static sentsensors::Registry g_sensors;
static sentsensors::UltraChannel g_ultra;
static sentsensors::RadarChannel g_radar;   // .wired stays false until GPIO27 is proven live
static sentsensors::LoudChannel g_loud;     // INMP441 I2S, Tier-A: levels only, never audio
static sentsensors::IrChannel g_ir;
static sentsensors::DhtChannel g_dht;       // DHT11 GPIO16 — self-detecting (checksum),
static sentsensors::PirChannel g_pir;       //   silent until wired. PIR zones ship
static sentsensors::WifiChannel g_wifi;     //   .wired=false; WiFi survey needs no wiring.
static int g_pir_zone[4] = {-1, -1, -1, -1};
static const int PIR_PINS[4] = {pins::PIR_FRONT, pins::PIR_REAR, pins::PIR_LEFT, pins::PIR_RIGHT};
static bool g_i2s_ok = false;

static const uint32_t ULTRA_PERIOD_MS = 120;   // ~8 Hz ping
static const uint32_t TELEM_PERIOD_MS = 500;   // 2 Hz comprehensive T frame
static const uint32_t DHT_PERIOD_MS   = 5000;  // DHT11 max rate is ~1 Hz; 0.2 Hz is plenty
static const uint32_t WIFI_PERIOD_MS  = 60000; // passive survey each minute (listen-only)

static void serial_sink(const char* frame, size_t n, void*) {
  Serial.write((const uint8_t*)frame, n);
}

static Transport g_tp(serial_sink, nullptr);
static LineAssembler g_rx;
static CmdCtx g_ctx;

static uint32_t tHeartbeat = 0;
static uint32_t tBlink = 0, ledOffAt = 0;
static bool ledOn = false;
static uint32_t bad_crc_in = 0, malformed_in = 0;   // surfaced in telemetry later (SENT-107)

static void handle_frame(Frame& f, uint32_t now) {
  if (!f.crc_ok) {                       // well-formed but corrupt → NACK its seq (SENT-023)
    bad_crc_in++;
    char p[48];
    snprintf(p, sizeof(p), "{\"nack\":%lu,\"reason\":\"crc\"}", (unsigned long)f.seq);
    g_tp.send_nack(p);
    return;
  }
  switch (f.type) {
    case Type::Command: {
      char resp[MAX_PAYLOAD];
      bool is_ack = false;
      const size_t n = handle_command(f.payload, f.seq, now, g_ctx, resp, sizeof(resp), is_ack);
      if (n == 0) return;                             // couldn't build → drop, never truncate
      if (is_ack) g_tp.send_ack(resp);
      else        g_tp.send_nack(resp);
      if (g_ctx.want_reboot) {
        Serial.flush();                               // the R must reach the wire first
        delay(50);
        ESP.restart();
      }
      break;
    }
    case Type::Ack: {                                 // Pi acked one of our B frames
      uint32_t target = 0;
      if (json_uint(f.payload, "ack", target)) g_tp.on_ack(target);
      break;
    }
    case Type::Nack: {
      uint32_t target = 0;
      if (json_uint(f.payload, "nack", target)) g_tp.on_nack(target, now);
      break;
    }
    default:
      break;                                          // unexpected type from the Pi: ignore
  }
}

void setup() {
  g_ctx.fw = FW_VERSION;                 // keep describe's fw in lockstep with the heartbeat's
  pinMode(pins::LED, OUTPUT);
  pinMode(pins::ULTRA_TRIG, OUTPUT);
  digitalWrite(pins::ULTRA_TRIG, LOW);
  pinMode(pins::ULTRA_ECHO, INPUT);
  pinMode(pins::RADAR_OUT, INPUT);
  pinMode(pins::DHT_DATA, INPUT_PULLUP);
  for (int i = 0; i < 4; i++) pinMode(PIR_PINS[i], INPUT);
  g_sensors.add(&g_ultra);
  g_sensors.add(&g_radar);
  g_sensors.add(&g_loud);
  g_sensors.add(&g_ir);
  g_sensors.add(&g_dht);
  g_sensors.add(&g_pir);
  g_sensors.add(&g_wifi);
  // PIR zones: labels are the motion BEARING (Q-D0b). Flip wired=true as each is soldered.
  g_pir_zone[0] = g_pir.add_zone("front", false);
  g_pir_zone[1] = g_pir.add_zone("rear", false);
  g_pir_zone[2] = g_pir.add_zone("left", false);
  g_pir_zone[3] = g_pir.add_zone("right", false);
  // WiFi survey: STA mode, never associates, passive scans only (listens to beacons).
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  // INMP441 over I2S: 16 kHz, 32-bit slots (the mic delivers 24-bit left-justified).
  I2S.setAllPins(pins::MIC_I2S_SCK, pins::MIC_I2S_WS, pins::MIC_I2S_SD, -1, pins::MIC_I2S_SD);
  g_i2s_ok = I2S.begin(I2S_PHILIPS_MODE, 16000, 32);
  IrReceiver.begin(pins::IR_RX, DISABLE_LED_FEEDBACK);
  Serial.begin(115200);
  delay(50);
  char boot_ev[110];
  snprintf(boot_ev, sizeof(boot_ev),
           "{\"s\":\"boot\",\"sev\":0,\"sum\":\"sentinel_module fw %s up\",\"up_ms\":0}", FW_VERSION);
  g_tp.send_event(boot_ev);
}

static void dht_read(uint32_t now);

static void poll_sensors(uint32_t now) {
  static uint32_t tPing = 0;
  if (now - tPing >= ULTRA_PERIOD_MS) {
    tPing = now;
    digitalWrite(pins::ULTRA_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(pins::ULTRA_TRIG, LOW);
    // Blocks ≤25 ms worst-case; acceptable at this stage (RX buffer holds ~256 B ≈ 22 ms
    // and the Pi speaks rarely). The ISR-timestamp upgrade is Stage 7 hardening.
    const uint32_t us = pulseIn(pins::ULTRA_ECHO, HIGH, 25000);
    g_ultra.logic.feed(us, now);
  }
  if (g_radar.wired) g_radar.logic.feed(digitalRead(pins::RADAR_OUT) == HIGH, now);

  // Tier-A loudness: drain whatever I2S has buffered, RMS -> dB(rel), feed the logic.
  // Raw samples never leave this function (D-S2: levels, never audio content).
  if (g_i2s_ok && !g_loud.muted) {
    static int32_t buf[256];
    static uint32_t tWin = 0;
    static double acc = 0; static uint32_t nAcc = 0;
    static float dc = 0;                              // running DC offset (INMP441 has one;
    int bytes = I2S.available() ? I2S.read(buf, sizeof(buf)) : 0;   // without removal RMS is a lie)
    if (bytes > 0) {
      const int ns = bytes / 4;
      for (int i = 0; i < ns; i += 2) {   // LEFT slot only (L/R->GND); right slot is junk
        const float raw = (float)(buf[i] >> 8);       // 24-bit sample
        dc += 0.0005f * (raw - dc);                   // slow high-pass
        const float v = raw - dc;
        acc += (double)v * v;
        nAcc++;
      }
    }
    if (now - tWin >= 100 && nAcc > 64) {             // ~10 windows/s
      tWin = now;
      const float rms = sqrtf((float)(acc / nAcc));
      acc = 0; nAcc = 0;
      // dB relative scale: 0 = silence floor, ~95 = full-scale 24-bit
      const float db = rms > 1 ? 20.0f * log10f(rms / 8388608.0f) + 95.0f : 0.0f;
      g_loud.logic.feed_db(db < 0 ? 0 : db, now);
    }
  }

  // IR remote (Stage 5): decoded NEC press -> named button + local actions.
  if (IrReceiver.decode()) {
    const auto& d = IrReceiver.decodedIRData;
    if (d.protocol == NEC && !(d.flags & IRDATA_FLAGS_IS_REPEAT)) {
      const uint8_t cmd = d.command;
      const char* name =
        cmd == 0x45 ? "power" : cmd == 0x46 ? "mode"  : cmd == 0x47 ? "mute"  :
        cmd == 0x44 ? "play"  : cmd == 0x40 ? "prev"  : cmd == 0x43 ? "next"  :
        cmd == 0x07 ? "eq"    : cmd == 0x15 ? "vol-"  : cmd == 0x09 ? "vol+"  :
        cmd == 0x19 ? "cycle" : cmd == 0x0D ? "usd"   : "btn";
      g_ir.press(cmd, name, now);
      if (cmd == 0x47) {                              // hardware-true mic mute toggle
        g_loud.muted = !g_loud.muted;
        char e[120];
        snprintf(e, sizeof(e),
                 "{\"s\":\"mic\",\"sev\":0,\"sum\":\"microphone %s (IR remote)\",\"up_ms\":%lu}",
                 g_loud.muted ? "MUTED" : "unmuted", (unsigned long)now);
        g_tp.send_event(e);
      } else if (cmd == 0x44) {                       // AV-snap request (Pi decides + acts)
        char e[130];
        snprintf(e, sizeof(e),
                 "{\"s\":\"av_snap_req\",\"sev\":1,\"sum\":\"AV snap requested (remote)\",\"up_ms\":%lu}",
                 (unsigned long)now);
        g_tp.send_event(e);
      }
    }
    IrReceiver.resume();
  }
  g_ir.age(now);

  // PIR zones (B1): plain digital reads; feed() itself ignores unwired zones.
  for (int i = 0; i < 4; i++)
    if (g_pir_zone[i] >= 0)
      g_pir.feed(g_pir_zone[i], digitalRead(PIR_PINS[i]) == HIGH, now);

  // DHT11 (B1): bit-banged single-wire read every 5 s. Interrupts are off ~4 ms during
  // the 40-bit burst (I2S is DMA-fed and the UART FIFO holds ~50 B at 115200 — both fine).
  static uint32_t tDht = 0;
  if (now - tDht >= DHT_PERIOD_MS) {
    tDht = now;
    dht_read(now);
  }
  g_dht.now_ms = now;

  // WiFi survey (B1): async passive scan each minute; harvest when the radio is done.
  static uint32_t tWifi = 0;
  static bool scanning = false;
  if (!scanning && now - tWifi >= WIFI_PERIOD_MS) {
    tWifi = now;
    WiFi.scanNetworks(true /*async*/, false /*hidden*/, true /*passive: listen only*/, 300);
    scanning = true;
  }
  if (scanning) {
    const int16_t nf = WiFi.scanComplete();
    if (nf >= 0) {
      g_wifi.logic.begin(now);
      for (int16_t i = 0; i < nf; i++) {
        const uint8_t* b = WiFi.BSSID(i);
        uint64_t id = 0;
        if (b) for (int k = 0; k < 6; k++) id = (id << 8) | b[k];
        g_wifi.logic.add_ap(id, WiFi.RSSI(i));
      }
      g_wifi.logic.commit(now);
      WiFi.scanDelete();
      scanning = false;
    } else if (nf == WIFI_SCAN_FAILED) {
      scanning = false;                                // try again next minute
    }
  }
}

// DHT11 single-wire read: host start pulse, then 40 bits timed by high-pulse width.
// All timing raw; the portable DhtLogic judges checksum + plausibility + freshness.
static void dht_read(uint32_t now) {
  uint8_t data[5] = {0, 0, 0, 0, 0};
  pinMode(pins::DHT_DATA, OUTPUT);
  digitalWrite(pins::DHT_DATA, LOW);
  delay(20);                                           // ≥18 ms start signal
  noInterrupts();
  pinMode(pins::DHT_DATA, INPUT_PULLUP);
  delayMicroseconds(40);
  // sensor response: ~80 µs LOW then ~80 µs HIGH, then 40 bits
  auto wait_level = [](int level, uint32_t timeout_us) -> bool {
    const uint32_t t0 = micros();
    while (digitalRead(pins::DHT_DATA) != level)
      if (micros() - t0 > timeout_us) return false;
    return true;
  };
  bool ok = wait_level(LOW, 90) && wait_level(HIGH, 100) && wait_level(LOW, 100);
  if (ok) {
    for (int i = 0; i < 40 && ok; i++) {
      if (!wait_level(HIGH, 70)) { ok = false; break; }   // 50 µs low preamble per bit
      const uint32_t t0 = micros();
      if (!wait_level(LOW, 90)) { ok = false; break; }    // high width: ~27 µs=0, ~70 µs=1
      if (micros() - t0 > 45) data[i / 8] |= (uint8_t)(0x80 >> (i % 8));
    }
  }
  interrupts();
  if (ok) g_dht.logic.feed_frame(data, now);             // decode judges checksum/plausibility
  else    g_dht.logic.feed_fail(now);                    // absent sensor times out in ~0.3 ms
}

void loop() {
  const uint32_t now = millis();

  // Receive path: bytes → lines → frames → routed. Bounded at every step (SENT-107/121).
  while (Serial.available() > 0) {
    if (g_rx.feed((char)Serial.read())) {
      Frame f;
      if (decode(g_rx.mutable_line(), g_rx.len(), f)) handle_frame(f, now);
      else malformed_in++;
    }
  }

  // Heartbeat LED: one short flash every 5 s (owner S68).
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

  // SENT-LINK heartbeat — 1 Hz, the exact shape the Pi rebases clocks from (SENT-106).
  if (now - tHeartbeat >= 1000) {
    tHeartbeat = now;
    char p[80];
    snprintf(p, sizeof(p), "{\"up_ms\":%lu,\"fw\":\"%s\",\"proto\":1}",
             (unsigned long)now, FW_VERSION);
    g_tp.send_heartbeat(p);
  }

  // Sensors: poll, stream T at 2 Hz, fire events as they surface (Stage 3).
  poll_sensors(now);
  static uint32_t tTelem = 0;
  if (now - tTelem >= TELEM_PERIOD_MS) {
    tTelem = now;
    char t[MAX_PAYLOAD];
    if (g_sensors.build_telemetry(now, t, sizeof(t))) g_tp.send_telemetry(t);
  }
  char ev[MAX_PAYLOAD];
  if (g_sensors.next_event(ev, sizeof(ev), now)) g_tp.send_event(ev);

  g_tp.tick(now);   // reliable-B retransmits (the backlog rides this from Stage 6)
}
