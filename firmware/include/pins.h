// sentinel_module pin map — LOCKED, see the wiring guide + ir_remote_map.md.
// Verify each on hardware as its sensor comes online (D-S1 "pins are seeds to verify").
//   mic (INMP441 I2S MEMS): SCK→GPIO14 · WS→GPIO25 · SD→GPIO32 · L/R→GND   (S71: replaced the
//     finicky analog KY-037; digital audio → real loudness AND future on-device classification,
//     hub-independent = audio_spec Tier C)
//   radar OUT → GPIO27   (RCWL-0516 motion, HIGH on movement)
//   IR signal → GPIO4    (VS1838B receiver — remote buttons; §1b control surface)
//   heartbeat → GPIO2    (on-board LED)
#pragma once

namespace pins {
// INMP441 I2S microphone (the ESP32 is I2S master: it drives SCK + WS, the mic returns SD).
constexpr int MIC_I2S_SCK = 14;   // BCLK  (bit clock, ESP32 → mic)
constexpr int MIC_I2S_WS  = 25;   // LRCLK (word select, ESP32 → mic)
constexpr int MIC_I2S_SD  = 32;   // DATA  (audio, mic → ESP32)
// L/R tied to GND on the board = left channel.

constexpr int RADAR_OUT = 27;
constexpr int IR_RX     = 4;
constexpr int LED       = 2;

// Ultrasonic ranger (HC-SR04, bot's TIP at 8 cm height, VCC 3V3 — proven live S81d).
constexpr int ULTRA_TRIG = 26;   // output: 10 µs ping
constexpr int ULTRA_ECHO = 34;   // input-only pin — echo pulse width (no 5 V here without a divider!)

// DHT11 temp+humidity (prompt 24 B1, module in owner's hand — wire DATA here, VCC 3V3).
constexpr int DHT_DATA = 16;

// HC-SR501 PIR zones (prompt 24 Q-D0b): one pin per direction, identity = motion bearing.
// VCC 5 V (onboard regulator; OUT is 3V3 logic). 35/36/39 are input-only — perfect here.
constexpr int PIR_FRONT = 33;
constexpr int PIR_REAR  = 35;
constexpr int PIR_LEFT  = 36;    // "VP"
constexpr int PIR_RIGHT = 39;    // "VN"
}  // namespace pins
