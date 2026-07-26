// sentinel_module pin map — LOCKED 2026-07-26 (S68), see the wiring guide.
// Verify each on hardware as its sensor comes online (D-S1 "pins are seeds to verify").
//   mic A0    → GPIO32   (analog loudness envelope — confirmed live, rung 2)
//   radar OUT → GPIO27   (RCWL-0516 motion, HIGH on movement; off GPIO12 to dodge the strapping trap)
//   IR signal → GPIO4    (VS1838B receiver — remote buttons: explore / map / STOP …)
//   heartbeat → GPIO2    (on-board LED)
#pragma once

namespace pins {
constexpr int MIC_A0    = 32;
constexpr int RADAR_OUT = 27;
constexpr int IR_RX     = 4;
constexpr int LED       = 2;
}  // namespace pins
