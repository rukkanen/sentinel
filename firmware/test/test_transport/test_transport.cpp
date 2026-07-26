// Host tests for the SENT-LINK transport (spec SENT-024/025/160). RED before transport.cpp
// exists, GREEN after. Time and I/O are injected, so the retransmit state machine is fully
// exercised with no board.
#include <string.h>
#include <unity.h>

#include "transport.h"

using namespace sentlink;

// --- capturing sink -----------------------------------------------------------------
struct Cap {
  int calls = 0;
  char last[MAX_FRAME] = {0};
  size_t last_len = 0;
};
static void cap_sink(const char* f, size_t n, void* ctx) {
  Cap* c = (Cap*)ctx;
  c->calls++;
  memcpy(c->last, f, n);
  c->last[n] = 0;
  c->last_len = n;
}

static Cap cap;
static Transport* tp;

void setUp() {
  cap = Cap{};
  static Transport t(cap_sink, &cap, TransportCfg{/*ack_timeout_ms=*/200, /*max_retries=*/3});
  tp = &t;
  // drain any prior outbox state (fresh object each test via placement-free static: reset by acking)
}
void tearDown() {}

// events are fire-fast and never tracked
void test_event_is_immediate_and_untracked() {
  Transport t(cap_sink, &cap);
  uint32_t s = t.send_event("{\"s\":\"sound\"}");
  TEST_ASSERT_EQUAL(1, cap.calls);              // emitted right now
  TEST_ASSERT_TRUE(s > 0);
  TEST_ASSERT_TRUE(t.outbox_empty());           // not tracked for ACK
  TEST_ASSERT_EQUAL('E', cap.last[1]);          // an Event frame
}

// data is emitted once and tracked
void test_data_is_tracked() {
  Transport t(cap_sink, &cap);
  uint32_t s = t.send_data("{\"n\":1}", 1000);
  TEST_ASSERT_TRUE(s > 0);
  TEST_ASSERT_EQUAL(1, cap.calls);
  TEST_ASSERT_EQUAL(1, t.outbox_count());
  TEST_ASSERT_EQUAL('B', cap.last[1]);          // a Data frame
}

// no retransmit before the timeout; retransmit after
void test_retransmit_on_timeout() {
  Transport t(cap_sink, &cap, TransportCfg{200, 3});
  t.send_data("{\"n\":1}", 1000);
  TEST_ASSERT_EQUAL(0, t.tick(1100));           // 100ms < 200ms: nothing
  TEST_ASSERT_EQUAL(1, cap.calls);
  TEST_ASSERT_EQUAL(0, t.tick(1250));           // 250ms ≥ 200ms: retransmit (not a give-up)
  TEST_ASSERT_EQUAL(2, cap.calls);
  TEST_ASSERT_EQUAL(1, t.outbox_count());       // still in flight
}

// ack clears it
void test_ack_clears() {
  Transport t(cap_sink, &cap);
  uint32_t s = t.send_data("{\"n\":1}", 1000);
  t.on_ack(s);
  TEST_ASSERT_TRUE(t.outbox_empty());
  TEST_ASSERT_EQUAL(0, t.tick(2000));           // nothing to retransmit
  TEST_ASSERT_EQUAL(1, cap.calls);
}

// nack retransmits immediately
void test_nack_retransmits_now() {
  Transport t(cap_sink, &cap);
  uint32_t s = t.send_data("{\"n\":1}", 1000);
  t.on_nack(s, 1050);
  TEST_ASSERT_EQUAL(2, cap.calls);
  TEST_ASSERT_EQUAL(1, t.outbox_count());
}

// gives up after max_retries and reports the loss
void test_give_up_after_max_retries() {
  Transport t(cap_sink, &cap, TransportCfg{100, 3});
  t.send_data("{\"n\":1}", 0);                  // send #1 (tries=1)
  TEST_ASSERT_EQUAL(0, t.tick(150));            // retransmit #2
  TEST_ASSERT_EQUAL(0, t.tick(300));            // retransmit #3
  int gaveup = t.tick(450);                     // 3 tries used → give up
  TEST_ASSERT_EQUAL(1, gaveup);
  TEST_ASSERT_TRUE(t.outbox_empty());
  TEST_ASSERT_EQUAL(3, cap.calls);              // three sends total, no fourth
}

// outbox is bounded; overflow is refused (0), not overrun
void test_outbox_bounded() {
  Transport t(cap_sink, &cap);
  int ok = 0;
  for (int i = 0; i < Transport::OUTBOX + 3; i++)
    if (t.send_data("{\"n\":1}", 1000) != 0) ok++;
  TEST_ASSERT_EQUAL(Transport::OUTBOX, ok);
  TEST_ASSERT_EQUAL(Transport::OUTBOX, t.outbox_count());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_event_is_immediate_and_untracked);
  RUN_TEST(test_data_is_tracked);
  RUN_TEST(test_retransmit_on_timeout);
  RUN_TEST(test_ack_clears);
  RUN_TEST(test_nack_retransmits_now);
  RUN_TEST(test_give_up_after_max_retries);
  RUN_TEST(test_outbox_bounded);
  return UNITY_END();
}
