#include "sentlink.h"

namespace sentlink {

uint16_t crc16_ccitt(const char* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)(uint8_t)data[i] << 8;
    for (int b = 0; b < 8; b++)
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

Type type_of(char c) {
  switch (c) {
    case 'E': return Type::Event;
    case 'T': return Type::Telemetry;
    case 'H': return Type::Heartbeat;
    case 'R': return Type::Ack;
    case 'N': return Type::Nack;
    case 'C': return Type::Command;
    case 'B': return Type::Data;
    case 'D': return Type::Debug;
    default:  return Type::Invalid;
  }
}

static size_t utoa10(uint32_t v, char* out) {
  char tmp[10];
  size_t n = 0;
  do { tmp[n++] = char('0' + v % 10); v /= 10; } while (v);
  for (size_t i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
  return n;
}

static bool hex4(uint16_t v, char* out) {
  static const char* H = "0123456789ABCDEF";
  out[0] = H[(v >> 12) & 0xF];
  out[1] = H[(v >> 8) & 0xF];
  out[2] = H[(v >> 4) & 0xF];
  out[3] = H[v & 0xF];
  return true;
}

size_t encode(char* buf, size_t cap, Type type, uint32_t seq, const char* payload) {
  if (!buf || !payload) return 0;
  size_t plen = 0;
  while (payload[plen]) {
    if (payload[plen] == '|' || payload[plen] == '\n') return 0;  // illegal in payload
    plen++;
  }
  if (plen > MAX_PAYLOAD) return 0;

  // Build the head "<ver><type><seq>|<payload>" first (CRC is computed over exactly this).
  size_t n = 0;
  auto put = [&](char c) { if (n < cap) buf[n] = c; n++; };
  put(VERSION);
  put((char)type);
  { char d[10]; size_t k = utoa10(seq, d); for (size_t i = 0; i < k; i++) put(d[i]); }
  put('|');
  for (size_t i = 0; i < plen; i++) put(payload[i]);
  if (n > cap) return 0;                       // head already overflowed
  const size_t head_len = n;

  const uint16_t crc = crc16_ccitt(buf, head_len);
  put('|');
  { char h[4]; hex4(crc, h); for (int i = 0; i < 4; i++) put(h[i]); }
  put('\n');
  if (n >= cap) return 0;                       // need room for NUL
  buf[n] = '\0';
  return n;
}

static bool parse_hex4(const char* s, uint16_t& out) {
  uint16_t v = 0;
  for (int i = 0; i < 4; i++) {
    char c = s[i];
    v <<= 4;
    if (c >= '0' && c <= '9') v |= (c - '0');
    else if (c >= 'A' && c <= 'F') v |= (c - 'A' + 10);
    else if (c >= 'a' && c <= 'f') v |= (c - 'a' + 10);
    else return false;
  }
  out = v;
  return true;
}

bool decode(char* line, size_t len, Frame& out) {
  out = Frame{};
  if (!line) return false;
  // Tolerate a trailing CR/LF.
  while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) len--;
  // Minimum: ver + type + >=1 seq digit + '|' + '|' + 4 crc = 9.
  if (len < 9) return false;

  // The CRC field is the last 4 chars, preceded by '|'.
  if (line[len - 5] != '|') return false;
  uint16_t want = 0;
  if (!parse_hex4(line + len - 4, want)) return false;
  const size_t head_len = len - 5;             // everything before "|<crc>"

  // head = <ver><type><seq>|<payload> ; find the FIRST '|'.
  size_t bar = 0;
  while (bar < head_len && line[bar] != '|') bar++;
  if (bar >= head_len) return false;           // no header/payload separator
  if (bar < 3) return false;                    // need ver+type+>=1 digit before '|'

  out.ver = line[0];
  if (out.ver != VERSION) return false;
  out.type = type_of(line[1]);
  if (out.type == Type::Invalid) return false;

  uint32_t seq = 0;
  for (size_t i = 2; i < bar; i++) {
    char c = line[i];
    if (c < '0' || c > '9') return false;
    seq = seq * 10 + (uint32_t)(c - '0');
  }
  out.seq = seq;

  out.payload = line + bar + 1;
  out.payload_len = head_len - (bar + 1);
  line[head_len] = '\0';                        // null-terminate the payload

  out.crc_ok = (crc16_ccitt(line, head_len) == want);
  return true;                                   // structurally valid; crc_ok says if intact
}

}  // namespace sentlink
