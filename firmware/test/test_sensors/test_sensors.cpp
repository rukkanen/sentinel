// Host tests: sensor registry + ultrasonic/radar logic (prompt 23 Stage 3, SENT-160).
#include <string.h>

#include <unity.h>

#include "channels.h"
#include "registry.h"

using namespace sentsensors;

// ---------------------------------------------------------------- ultra logic
static void test_ultra_median_smoothing(void) {
  UltraLogic u;
  u.feed(1160, 0);   // 20.0 cm
  u.feed(1160, 100);
  u.feed(5800, 200); // 100 cm spike — median eats it
  TEST_ASSERT_TRUE(u.has_range());
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 20.0f, u.range_cm());
}

static void test_ultra_timeout_run_invalidates(void) {
  UltraLogic u;
  u.feed(1160, 0);
  TEST_ASSERT_TRUE(u.has_range());
  for (int i = 0; i < 5; i++) u.feed(0, 100 * i);   // 5 misses → invalid
  TEST_ASSERT_FALSE(u.has_range());
}

static void test_ultra_close_event_debounce_and_hysteresis(void) {
  UltraLogic u;                                     // close<15, clear>25, debounce 3
  char ev[160];
  // two close readings — no event yet (debounce 3)
  u.feed(580, 0); u.feed(580, 50);                  // 10 cm
  TEST_ASSERT_EQUAL_size_t(0, u.next_event(ev, sizeof(ev), 60));
  u.feed(580, 100);                                 // third → event
  TEST_ASSERT_TRUE(u.next_event(ev, sizeof(ev), 110) > 0);
  TEST_ASSERT_NOT_NULL(strstr(ev, "\"s\":\"range\""));
  TEST_ASSERT_NOT_NULL(strstr(ev, "obstacle at"));
  // wobble to 20 cm (inside hysteresis band) — NO clear event
  u.feed(1160, 200); u.feed(1160, 250); u.feed(1160, 300);
  TEST_ASSERT_EQUAL_size_t(0, u.next_event(ev, sizeof(ev), 310));
  // past 25 cm → cleared event
  u.feed(1740, 400); u.feed(1740, 450); u.feed(1740, 500);   // 30 cm
  TEST_ASSERT_TRUE(u.next_event(ev, sizeof(ev), 510) > 0);
  TEST_ASSERT_NOT_NULL(strstr(ev, "cleared"));
}

// ---------------------------------------------------------------- radar logic
static void test_radar_debounce_and_one_event_per_episode(void) {
  RadarLogic r;                                     // debounce 150ms, gap 5s
  char ev[120];
  r.feed(true, 0);
  r.feed(true, 100);
  TEST_ASSERT_FALSE(r.motion());                    // not held long enough yet
  r.feed(true, 200);
  TEST_ASSERT_TRUE(r.motion());
  TEST_ASSERT_TRUE(r.next_event(ev, sizeof(ev), 210) > 0);
  TEST_ASSERT_EQUAL_size_t(0, r.next_event(ev, sizeof(ev), 220));   // one per episode
  // brief LOW inside the episode, HIGH again — still the same episode, no new event
  r.feed(false, 1000); r.feed(false, 1200); r.feed(true, 1300); r.feed(true, 1500);
  TEST_ASSERT_EQUAL_size_t(0, r.next_event(ev, sizeof(ev), 1510));
  // long quiet ends the episode; the next motion is a fresh event
  r.feed(false, 2000); r.feed(false, 2200);
  r.feed(false, 8000);
  r.feed(true, 8100); r.feed(true, 8300);
  TEST_ASSERT_TRUE(r.next_event(ev, sizeof(ev), 8310) > 0);
  TEST_ASSERT_EQUAL_UINT32(2, r.episodes());
}

// ---------------------------------------------------------------- registry
static void test_registry_builds_combined_telemetry(void) {
  Registry reg;
  UltraChannel ultra;
  RadarChannel radar;
  radar.wired = true;
  TEST_ASSERT_TRUE(reg.add(&ultra));
  TEST_ASSERT_TRUE(reg.add(&radar));

  char t[200];
  // ultra has no reading yet → only radar speaks
  TEST_ASSERT_TRUE(reg.build_telemetry(1234, t, sizeof(t)) > 0);
  TEST_ASSERT_EQUAL_STRING("{\"up_ms\":1234,\"radar\":0}", t);
  // give ultra a reading → both speak
  ultra.logic.feed(1160, 0);
  TEST_ASSERT_TRUE(reg.build_telemetry(2000, t, sizeof(t)) > 0);
  TEST_ASSERT_EQUAL_STRING("{\"up_ms\":2000,\"range_cm\":20.0,\"radar\":0}", t);
}

static void test_registry_silent_when_no_driver_speaks(void) {
  Registry reg;
  UltraChannel ultra;                               // no reading → silent
  RadarChannel radar;                               // not wired → silent
  reg.add(&ultra);
  reg.add(&radar);
  char t[200];
  TEST_ASSERT_EQUAL_size_t(0, reg.build_telemetry(1, t, sizeof(t)));   // honest: no frame
}

static void test_registry_bounded(void) {
  Registry reg;
  UltraChannel c[Registry::MAX_DRIVERS + 2];
  for (int i = 0; i < Registry::MAX_DRIVERS; i++) TEST_ASSERT_TRUE(reg.add(&c[i]));
  TEST_ASSERT_FALSE(reg.add(&c[Registry::MAX_DRIVERS]));   // full → refused, never grows
}

static void test_registry_drains_events_round_robin(void) {
  Registry reg;
  UltraChannel ultra;
  RadarChannel radar;
  radar.wired = true;
  reg.add(&ultra);
  reg.add(&radar);
  // queue one event in each
  ultra.logic.feed(580, 0); ultra.logic.feed(580, 50); ultra.logic.feed(580, 100);
  radar.logic.feed(true, 0); radar.logic.feed(true, 200);
  char ev[160];
  int got = 0;
  while (reg.next_event(ev, sizeof(ev), 300) > 0) got++;
  TEST_ASSERT_EQUAL_INT(2, got);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_ultra_median_smoothing);
  RUN_TEST(test_ultra_timeout_run_invalidates);
  RUN_TEST(test_ultra_close_event_debounce_and_hysteresis);
  RUN_TEST(test_radar_debounce_and_one_event_per_episode);
  RUN_TEST(test_registry_builds_combined_telemetry);
  RUN_TEST(test_registry_silent_when_no_driver_speaks);
  RUN_TEST(test_registry_bounded);
  RUN_TEST(test_registry_drains_events_round_robin);
  return UNITY_END();
}
