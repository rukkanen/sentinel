// SENT-LINK command handling (spec SENT-026, prompt 23 Stage 1).
//
// Answers a Pi `C` frame with the R/N payload the Pi's bridge actually parses
// (rosbottiNG dashboard/sentinel_bridge.py): the TARGET seq rides in the payload —
// R {"ack":<the C frame's seq>, ...result} / N {"nack":<seq>,"err":"..."}.
//
// Every command is IDEMPOTENT (SENT-026): the bridge retries with fresh seqs on timeout,
// so handling the same logical command twice must be harmless — and it is: ping/describe
// are pure reads, set_clock overwrites the same offset, sensors_on/off set a flag,
// flush re-reports, reboot re-requests.
//
// Portable + host-tested: no Arduino calls; reboot is a flag the firmware loop acts on.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace sentlink {

struct CmdCtx {
  const char* fw = "0.2.0";
  int proto = 1;
  // Clock discipline (SENT-062/106): Pi epoch at set_clock + our up_ms then. The Pi clock
  // is truth; we only remember the pair so buffered events can be rebased later.
  double   epoch_s = 0.0;
  uint32_t epoch_at_ms = 0;
  bool     clock_set = false;
  bool     sensors_on = true;
  bool     want_reboot = false;   // set by `reboot`; the loop restarts AFTER the R goes out
  uint32_t backlog_count = 0;     // curator backlog size (Stage 6 fills this; 0 until then)
};

// Handle one C-frame payload. Writes the R/N *payload* into out, sets is_ack, returns its
// length (0 = out too small, caller drops the response — bounded, never truncated JSON).
size_t handle_command(const char* payload, uint32_t cmd_seq, uint32_t now_ms,
                      CmdCtx& ctx, char* out, size_t cap, bool& is_ack);

}  // namespace sentlink
