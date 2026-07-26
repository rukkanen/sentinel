#include "transport.h"

namespace sentlink {

Transport::Transport(Sink sink, void* ctx, TransportCfg cfg)
    : sink_(sink), ctx_(ctx), cfg_(cfg) {}

uint32_t Transport::emit(Type type, const char* payload) {
  char buf[MAX_FRAME];
  const uint32_t seq = seq_++;
  const size_t n = encode(buf, sizeof(buf), type, seq, payload);
  if (!n) return 0;
  if (sink_) sink_(buf, n, ctx_);
  return seq;
}

uint32_t Transport::send_event(const char* p)     { return emit(Type::Event, p); }
uint32_t Transport::send_telemetry(const char* p) { return emit(Type::Telemetry, p); }
uint32_t Transport::send_heartbeat(const char* p) { return emit(Type::Heartbeat, p); }

uint32_t Transport::send_data(const char* payload, uint32_t now_ms) {
  int idx = -1;
  for (int i = 0; i < OUTBOX; i++)
    if (!outbox_[i].used) { idx = i; break; }
  if (idx < 0) return 0;                        // outbox full — refuse, don't overrun

  Slot& s = outbox_[idx];
  const uint32_t seq = seq_++;
  const size_t n = encode(s.frame, sizeof(s.frame), Type::Data, seq, payload);
  if (!n) return 0;                             // payload too long — slot stays free

  s.used = true; s.seq = seq; s.sent_ms = now_ms; s.tries = 1; s.len = n;
  count_++;
  if (sink_) sink_(s.frame, n, ctx_);
  return seq;
}

void Transport::resend(Slot& s, uint32_t now_ms) {
  if (sink_) sink_(s.frame, s.len, ctx_);
  s.sent_ms = now_ms;
  s.tries++;
}

void Transport::on_ack(uint32_t seq) {
  for (int i = 0; i < OUTBOX; i++)
    if (outbox_[i].used && outbox_[i].seq == seq) {
      outbox_[i].used = false; count_--; return;
    }
}

void Transport::on_nack(uint32_t seq, uint32_t now_ms) {
  for (int i = 0; i < OUTBOX; i++)
    if (outbox_[i].used && outbox_[i].seq == seq) { resend(outbox_[i], now_ms); return; }
}

int Transport::tick(uint32_t now_ms) {
  int gaveup = 0;
  for (int i = 0; i < OUTBOX; i++) {
    Slot& s = outbox_[i];
    if (!s.used) continue;
    if (now_ms - s.sent_ms < cfg_.ack_timeout_ms) continue;   // still within ACK window
    if (s.tries >= cfg_.max_retries) { s.used = false; count_--; gaveup++; }  // give up + report
    else resend(s, now_ms);
  }
  return gaveup;
}

}  // namespace sentlink
