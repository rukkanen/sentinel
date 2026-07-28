#include "commands.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "wire.h"

namespace sentlink {

// snprintf wrapper that maps overflow/error to 0 ("drop, never truncate" — SENT-121).
static size_t fmt(char* out, size_t cap, const char* f, ...) {
  va_list ap;
  va_start(ap, f);
  const int n = vsnprintf(out, cap, f, ap);
  va_end(ap);
  if (n <= 0 || (size_t)n >= cap) return 0;
  return (size_t)n;
}

size_t handle_command(const char* payload, uint32_t cmd_seq, uint32_t now_ms,
                      CmdCtx& ctx, char* out, size_t cap, bool& is_ack) {
  char cmd[24];
  if (!json_str(payload, "cmd", cmd, sizeof(cmd))) {
    is_ack = false;
    return fmt(out, cap, "{\"nack\":%lu,\"err\":\"no_cmd\"}", (unsigned long)cmd_seq);
  }

  is_ack = true;
  if (strcmp(cmd, "ping") == 0) {
    return fmt(out, cap, "{\"ack\":%lu,\"pong\":1}", (unsigned long)cmd_seq);
  }
  if (strcmp(cmd, "describe") == 0) {
    // The roster is the SENT-004 contract: what this firmware can grow to speak. Sensors
    // report honestly via telemetry/health; describe declares the *driver* roster.
    return fmt(out, cap,
               "{\"ack\":%lu,\"fw\":\"%s\",\"proto\":%d,"
               "\"sensors\":[\"loud\",\"radar\",\"ir\",\"range\"]}",
               (unsigned long)cmd_seq, ctx.fw, ctx.proto);
  }
  if (strcmp(cmd, "set_clock") == 0) {
    double epoch = 0.0;
    if (!json_double(payload, "epoch_s", epoch) || epoch <= 0.0) {
      is_ack = false;
      return fmt(out, cap, "{\"nack\":%lu,\"err\":\"bad_epoch\"}", (unsigned long)cmd_seq);
    }
    ctx.epoch_s = epoch;
    ctx.epoch_at_ms = now_ms;
    ctx.clock_set = true;
    return fmt(out, cap, "{\"ack\":%lu}", (unsigned long)cmd_seq);
  }
  if (strcmp(cmd, "flush") == 0) {
    return fmt(out, cap, "{\"ack\":%lu,\"backlog\":%lu}", (unsigned long)cmd_seq,
               (unsigned long)ctx.backlog_count);
  }
  if (strcmp(cmd, "sensors_on") == 0) {
    ctx.sensors_on = true;
    return fmt(out, cap, "{\"ack\":%lu,\"sensors\":1}", (unsigned long)cmd_seq);
  }
  if (strcmp(cmd, "sensors_off") == 0) {
    ctx.sensors_on = false;
    return fmt(out, cap, "{\"ack\":%lu,\"sensors\":0}", (unsigned long)cmd_seq);
  }
  if (strcmp(cmd, "reboot") == 0) {
    ctx.want_reboot = true;                  // the loop restarts AFTER this R is on the wire
    return fmt(out, cap, "{\"ack\":%lu,\"rebooting\":1}", (unsigned long)cmd_seq);
  }

  is_ack = false;
  return fmt(out, cap, "{\"nack\":%lu,\"err\":\"unknown_cmd\"}", (unsigned long)cmd_seq);
}

}  // namespace sentlink
