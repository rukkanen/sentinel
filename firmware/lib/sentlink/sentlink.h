// SENT-LINK v1 — the sentinel_module ↔ Pi communication framework (portable core).
//
// Framework-free C++ (no Arduino, no ESP-IDF) so it host-compiles + unit-tests in the
// PlatformIO `native` env (sentinel_module_spec SENT-160). The ESP32 firmware and the Pi
// bridge both speak this. Wire format (spec SENT-021):
//
//     <ver><type><seq>|<payload>|<crc16>\n
//
//   ver      one char, protocol version ('1')
//   type     one char, frame class (Type below) — the typed prefix, spec SENT-022
//   seq      decimal uint32, monotonic, wraps; gap-detectable (spec SENT-023)
//   payload  compact JSON (no '|' bytes)
//   crc16    CRC-16/CCITT-FALSE over "<ver><type><seq>|<payload>", 4 upper-hex (spec SENT-023)
//
// Two delivery styles ride the ONE frame format (owner S68 — "move data well AND an
// interrupt-style event notifier"): EVENT frames are fire-fast/low-latency (the notifier);
// DATA frames are the reliable bulk channel (acked + retransmitted by the transport layer,
// built on top of this codec — a separate unit). This header is just the codec; it does no
// I/O, so it is trivially testable and reusable on both ends.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace sentlink {

constexpr char VERSION = '1';
constexpr size_t CRC_HEX = 4;
// Longest frame we ever build/parse (bounds every buffer — spec SENT-121 "bounded everywhere").
constexpr size_t MAX_PAYLOAD = 200;
constexpr size_t MAX_FRAME = 1 + 1 + 10 + 1 + MAX_PAYLOAD + 1 + CRC_HEX + 1 + 1;  // +\n +NUL

enum class Type : char {
  Event     = 'E',  // interrupt-style notifier — low latency, best-effort + acked
  Telemetry = 'T',  // periodic comprehensive frame (curator rolling stats)
  Heartbeat = 'H',  // liveness + rolling health
  Ack       = 'R',  // response / positive ack
  Nack      = 'N',  // negative ack (bad crc / seq gap) — carries the offending seq
  Command   = 'C',  // Pi → MCU
  Data      = 'B',  // reliable bulk channel (buffered-event flush, larger transfers)
  Debug     = 'D',  // human-readable diagnostics
  Invalid   = '?',
};

// CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflect, xorout 0).
// Cross-validated on real hardware S68 (rung-2 frames matched the Pi's independent impl).
uint16_t crc16_ccitt(const char* data, size_t len);

// Encode a complete frame (incl. trailing '\n') into `buf`. Returns bytes written
// (excluding the NUL terminator), or 0 if it would overflow `cap` or payload is too long.
size_t encode(char* buf, size_t cap, Type type, uint32_t seq, const char* payload);

// A decoded, validated frame. `payload` points INTO the mutated input line.
struct Frame {
  char     ver = 0;
  Type     type = Type::Invalid;
  uint32_t seq = 0;
  const char* payload = nullptr;
  size_t   payload_len = 0;
  bool     crc_ok = false;
};

// Parse one line (a single frame, WITHOUT the trailing '\n'; trailing '\n'/'\r' tolerated).
// Mutates `line` in place (null-terminates the payload). Returns true iff the frame is
// structurally well-formed AND the CRC matches (out.crc_ok mirrors the CRC result). A
// structurally-broken line returns false; a well-formed line with a bad CRC returns true
// with crc_ok=false so the caller can NACK the seq (spec SENT-023).
bool decode(char* line, size_t len, Frame& out);

// Map a raw type char to Type (Invalid if unknown).
Type type_of(char c);

}  // namespace sentlink
