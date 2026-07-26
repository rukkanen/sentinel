// SENT-LINK transport — the delivery layer over the codec (sentinel_module_spec SENT-024/025).
//
// Two delivery styles, one link (owner S68 — "move data to a high degree AND an interrupt-style
// event notifier"):
//   • send_event/telemetry/heartbeat → emitted IMMEDIATELY, fire-fast, NOT retransmitted
//     (the low-latency notifier — a sensor ISR sets a flag, the loop fires the event now).
//   • send_data → RELIABLE: queued in a bounded outbox, retransmitted on ACK-timeout or NACK,
//     given up after max_retries (the caller learns of loss). This is the bulk/important channel.
//
// Host-testable by construction: no real I/O and no real clock. Output goes through a `Sink`
// callback (tests capture it) and time is injected via `now_ms` (tests control it). So the whole
// retransmit/timeout/ack state machine is unit-tested with zero hardware (spec SENT-160).
#pragma once
#include "sentlink.h"

namespace sentlink {

typedef void (*Sink)(const char* frame, size_t len, void* ctx);

struct TransportCfg {
  uint32_t ack_timeout_ms = 200;   // no ACK within this → retransmit
  uint8_t  max_retries    = 4;     // give up after this many (re)sends
};

class Transport {
 public:
  static const int OUTBOX = 8;     // max in-flight reliable frames (bounded — SENT-121)

  Transport(Sink sink, void* ctx, TransportCfg cfg = TransportCfg{});

  // Fire-fast, not tracked. Returns the seq used.
  uint32_t send_event(const char* payload);
  uint32_t send_telemetry(const char* payload);
  uint32_t send_heartbeat(const char* payload);

  // Reliable. Emits once now and tracks for ACK. Returns seq, or 0 if the outbox is full.
  uint32_t send_data(const char* payload, uint32_t now_ms);

  void on_ack(uint32_t seq);                 // clears the acked frame from the outbox
  void on_nack(uint32_t seq, uint32_t now_ms);  // immediate retransmit of that frame

  // Drive timeouts/retransmits — call often. Returns the number of frames that were GIVEN UP
  // (exceeded max_retries) on this tick; those are removed from the outbox.
  int tick(uint32_t now_ms);

  bool outbox_empty() const { return count_ == 0; }
  int  outbox_count() const { return count_; }

 private:
  struct Slot {
    bool     used = false;
    uint32_t seq = 0;
    uint32_t sent_ms = 0;
    uint8_t  tries = 0;
    size_t   len = 0;
    char     frame[MAX_FRAME];
  };
  uint32_t emit(Type type, const char* payload);   // encode + push to sink; returns seq
  void     resend(Slot& s, uint32_t now_ms);

  Sink sink_;
  void* ctx_;
  TransportCfg cfg_;
  uint32_t seq_ = 1;          // 0 is reserved as "none"
  Slot outbox_[OUTBOX];
  int count_ = 0;
};

}  // namespace sentlink
