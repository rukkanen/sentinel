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

#include "channels.h"
#include "commands.h"
#include "pins.h"
#include "registry.h"
#include "sentlink.h"
#include "transport.h"
#include "wire.h"

using namespace sentlink;

static const char* FW_VERSION = "0.3.0";

// ---- sensor registry (Stage 3): a new sensor = one channel object + one add() ----
static sentsensors::Registry g_sensors;
static sentsensors::UltraChannel g_ultra;
static sentsensors::RadarChannel g_radar;   // .wired stays false until GPIO27 is proven live

static const uint32_t ULTRA_PERIOD_MS = 120;   // ~8 Hz ping
static const uint32_t TELEM_PERIOD_MS = 500;   // 2 Hz comprehensive T frame

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
  g_sensors.add(&g_ultra);
  g_sensors.add(&g_radar);
  Serial.begin(115200);
  delay(50);
  g_tp.send_event("{\"s\":\"boot\",\"sev\":0,\"sum\":\"sentinel_module fw 0.3.0 up\",\"up_ms\":0}");
}

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
