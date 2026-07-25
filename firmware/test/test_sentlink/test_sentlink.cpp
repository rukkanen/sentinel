// Host unit tests for the SENT-LINK codec (spec SENT-160). Run: pio test -e native
// (needs a host C++ compiler — `sudo apt install build-essential`). These are the
// owner's proxy eyes (D3): they must be RED before the code exists and GREEN after.
#include <string.h>
#include <unity.h>

#include "sentlink.h"

using namespace sentlink;

static char buf[MAX_FRAME];

void setUp() {}
void tearDown() {}

// --- CRC vectors (independently checkable; matched on hardware S68) ------------------
void test_crc_known_vectors() {
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, crc16_ccitt("", 0));
  TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16_ccitt("123456789", 9));  // canonical CCITT-FALSE
}

// --- encode → decode round-trips ----------------------------------------------------
void test_encode_shape() {
  size_t n = encode(buf, sizeof(buf), Type::Event, 7, "{\"s\":\"sound\"}");
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_EQUAL('1', buf[0]);
  TEST_ASSERT_EQUAL('E', buf[1]);
  TEST_ASSERT_EQUAL('\n', buf[n - 1]);
  // "1E7|{\"s\":\"sound\"}|<crc>\n"
  TEST_ASSERT_EQUAL(0, strncmp(buf, "1E7|{\"s\":\"sound\"}|", 17));
}

void test_roundtrip() {
  size_t n = encode(buf, sizeof(buf), Type::Telemetry, 4242, "{\"lvl\":123}");
  TEST_ASSERT_TRUE(n > 0);
  Frame f;
  TEST_ASSERT_TRUE(decode(buf, n, f));
  TEST_ASSERT_TRUE(f.crc_ok);
  TEST_ASSERT_EQUAL('1', f.ver);
  TEST_ASSERT_TRUE(f.type == Type::Telemetry);
  TEST_ASSERT_EQUAL_UINT32(4242, f.seq);
  TEST_ASSERT_EQUAL_STRING("{\"lvl\":123}", f.payload);
}

// --- corruption is caught, not crashed (spec SENT-023) ------------------------------
void test_bad_crc_flagged_not_rejected() {
  size_t n = encode(buf, sizeof(buf), Type::Event, 1, "{\"x\":1}");
  buf[n - 2] ^= 0x01;  // flip a CRC hex nibble
  Frame f;
  TEST_ASSERT_TRUE(decode(buf, n, f));   // still structurally valid…
  TEST_ASSERT_FALSE(f.crc_ok);           // …but flagged corrupt (→ caller NACKs)
}

void test_payload_corruption_detected() {
  size_t n = encode(buf, sizeof(buf), Type::Event, 9, "{\"v\":0}");
  // corrupt a payload byte (index 4 is inside the JSON) — CRC must now fail
  buf[5] = (buf[5] == 'x') ? 'y' : 'x';
  Frame f;
  TEST_ASSERT_TRUE(decode(buf, n, f));
  TEST_ASSERT_FALSE(f.crc_ok);
}

void test_malformed_lines_rejected() {
  Frame f;
  char a[] = "garbage";                 TEST_ASSERT_FALSE(decode(a, strlen(a), f));
  char b[] = "1E7|nopipe";              TEST_ASSERT_FALSE(decode(b, strlen(b), f));
  char c[] = "9E7|{}|FFFF";             TEST_ASSERT_FALSE(decode(c, strlen(c), f)); // bad ver
  char d[] = "1Z7|{}|FFFF";             TEST_ASSERT_FALSE(decode(d, strlen(d), f)); // bad type
  char e[] = "1E|{}|FFFF";              TEST_ASSERT_FALSE(decode(e, strlen(e), f)); // no seq
  char g[] = "1E7|{}|ZZZZ";             TEST_ASSERT_FALSE(decode(g, strlen(g), f)); // bad crc hex
}

void test_illegal_payload_refused() {
  TEST_ASSERT_EQUAL(0, encode(buf, sizeof(buf), Type::Event, 1, "has|pipe"));
  TEST_ASSERT_EQUAL(0, encode(buf, sizeof(buf), Type::Event, 1, "has\nnewline"));
}

void test_encode_bounds() {
  char tiny[8];
  TEST_ASSERT_EQUAL(0, encode(tiny, sizeof(tiny), Type::Event, 1, "{\"way\":\"too long\"}"));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_crc_known_vectors);
  RUN_TEST(test_encode_shape);
  RUN_TEST(test_roundtrip);
  RUN_TEST(test_bad_crc_flagged_not_rejected);
  RUN_TEST(test_payload_corruption_detected);
  RUN_TEST(test_malformed_lines_rejected);
  RUN_TEST(test_illegal_payload_refused);
  RUN_TEST(test_encode_bounds);
  return UNITY_END();
}
