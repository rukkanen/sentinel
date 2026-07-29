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

// ---------------------------------------------------------------- loudness (Stage 4)
static void test_loud_baseline_learns_only_quiet(void) {
  LoudLogic l;                                     // baseline seeds at 35
  for (int i = 0; i < 200; i++) l.feed_db(40.0f, i * 50);   // steady 40 dB ambient
  TEST_ASSERT_FLOAT_WITHIN(2.0f, 40.0f, l.baseline_db());
  const float base = l.baseline_db();
  for (int i = 0; i < 10; i++) l.feed_db(80.0f, 10000 + i * 50);  // loud burst
  TEST_ASSERT_TRUE(l.loud());
  TEST_ASSERT_FLOAT_WITHIN(1.0f, base, l.baseline_db());   // loud did NOT teach the baseline
}

static void test_loud_event_hysteresis(void) {
  LoudLogic l;
  char ev[200];
  for (int i = 0; i < 100; i++) l.feed_db(40.0f, i * 50);
  TEST_ASSERT_EQUAL_size_t(0, l.next_event(ev, sizeof(ev), 0));
  l.feed_db(60, 6000); l.feed_db(60, 6050);
  TEST_ASSERT_EQUAL_size_t(0, l.next_event(ev, sizeof(ev), 6060));   // debounce 3
  l.feed_db(60, 6100);
  TEST_ASSERT_TRUE(l.next_event(ev, sizeof(ev), 6110) > 0);
  TEST_ASSERT_NOT_NULL(strstr(ev, "\"s\":\"loud\""));
  l.feed_db(52, 6200);                              // above exit margin: still loud
  TEST_ASSERT_EQUAL_size_t(0, l.next_event(ev, sizeof(ev), 6210));
  l.feed_db(42, 6300);                              // back to normal
  TEST_ASSERT_TRUE(l.next_event(ev, sizeof(ev), 6310) > 0);
  TEST_ASSERT_NOT_NULL(strstr(ev, "back to normal"));
}

static void test_loud_channel_mute_is_total(void) {
  LoudChannel c;
  char buf[200];
  c.logic.feed_db(90, 0); c.logic.feed_db(90, 50); c.logic.feed_db(90, 100);
  c.muted = true;
  TEST_ASSERT_EQUAL_size_t(0, c.next_event(buf, sizeof(buf), 200));  // no events while muted
  TEST_ASSERT_TRUE(c.telemetry_frag(buf, sizeof(buf)) > 0);
  TEST_ASSERT_EQUAL_STRING("\"mic_muted\":1", buf);                 // state IS reported
}

static void test_ir_channel_press_event(void) {
  IrChannel c;
  char buf[160];
  c.press(0x47, "mute", 1000);
  TEST_ASSERT_TRUE(c.telemetry_frag(buf, sizeof(buf)) > 0);
  TEST_ASSERT_EQUAL_STRING("\"ir\":\"mute\"", buf);
  TEST_ASSERT_TRUE(c.next_event(buf, sizeof(buf), 1001) > 0);
  TEST_ASSERT_NOT_NULL(strstr(buf, "remote: mute"));
  c.age(7000);
  TEST_ASSERT_EQUAL_size_t(0, c.telemetry_frag(buf, sizeof(buf)));   // stale button ages out
}

// ---------------------------------------------------------------- DHT11 (Stage B1)
static void test_dht_decode_and_checksum(void) {
  float t, rh;
  const uint8_t good[5] = {55, 0, 24, 3, 82};       // 55+0+24+3 = 82
  TEST_ASSERT_TRUE(DhtLogic::decode(good, t, rh));
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 24.3f, t);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 55.0f, rh);
  const uint8_t bad_sum[5] = {55, 0, 24, 3, 83};
  TEST_ASSERT_FALSE(DhtLogic::decode(bad_sum, t, rh));
  const uint8_t absurd[5] = {99, 0, 90, 0, 189};    // checksum OK, values impossible for DHT11
  TEST_ASSERT_FALSE(DhtLogic::decode(absurd, t, rh));
}

static void test_dht_freshness_and_failure_run(void) {
  DhtLogic d;
  const uint8_t good[5] = {40, 0, 21, 0, 61};
  d.feed_frame(good, 1000);
  TEST_ASSERT_TRUE(d.has_reading(2000));
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 21.0f, d.temp_c());
  d.feed_fail(3000); d.feed_fail(5000);
  TEST_ASSERT_TRUE(d.has_reading(5000));            // two fails: still trust the last value
  d.feed_fail(7000);
  TEST_ASSERT_FALSE(d.has_reading(7000));           // three in a row → honest silence
  d.feed_frame(good, 8000);
  TEST_ASSERT_TRUE(d.has_reading(8000));            // one good frame recovers
  TEST_ASSERT_FALSE(d.has_reading(8000 + 31000));   // stale-out with no feeds at all
}

static void test_dht_channel_frag(void) {
  DhtChannel c;
  char buf[80];
  TEST_ASSERT_EQUAL_size_t(0, c.telemetry_frag(buf, sizeof(buf)));   // nothing yet → silent
  const uint8_t good[5] = {47, 0, 23, 5, 75};
  c.logic.feed_frame(good, 1000);
  c.now_ms = 1500;
  TEST_ASSERT_TRUE(c.telemetry_frag(buf, sizeof(buf)) > 0);
  TEST_ASSERT_EQUAL_STRING("\"temp_c\":23.5,\"rh\":47", buf);
}

// ---------------------------------------------------------------- PIR zones (Stage B1)
static void test_pir_zones_debounce_episode_and_labels(void) {
  PirChannel c;                                     // debounce 120ms, episode gap 8s
  const int front = c.add_zone("front", true);
  const int rear = c.add_zone("rear", true);
  const int dark = c.add_zone("left", false);       // NOT wired → must stay silent
  TEST_ASSERT_TRUE(front >= 0 && rear >= 0 && dark >= 0);
  char buf[160];
  TEST_ASSERT_EQUAL_size_t(0, c.telemetry_frag(buf, sizeof(buf)));   // all quiet → silent

  c.feed(front, true, 0);
  c.feed(front, true, 60);
  TEST_ASSERT_EQUAL_size_t(0, c.next_event(buf, sizeof(buf), 70));   // not held long enough
  c.feed(front, true, 130);                          // held ≥120ms → motion
  TEST_ASSERT_TRUE(c.next_event(buf, sizeof(buf), 140) > 0);
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"s\":\"pir\""));
  TEST_ASSERT_NOT_NULL(strstr(buf, "front"));
  TEST_ASSERT_EQUAL_size_t(0, c.next_event(buf, sizeof(buf), 150));  // one per episode

  c.feed(rear, true, 200); c.feed(rear, true, 330);  // second zone fires its own event
  TEST_ASSERT_TRUE(c.next_event(buf, sizeof(buf), 340) > 0);
  TEST_ASSERT_NOT_NULL(strstr(buf, "rear"));

  TEST_ASSERT_TRUE(c.telemetry_frag(buf, sizeof(buf)) > 0);          // both active
  TEST_ASSERT_EQUAL_STRING("\"pir\":\"front+rear\"", buf);

  c.feed(dark, true, 400); c.feed(dark, true, 600);  // unwired zone: no frag, no event
  TEST_ASSERT_EQUAL_size_t(0, c.next_event(buf, sizeof(buf), 610));
  c.telemetry_frag(buf, sizeof(buf));
  TEST_ASSERT_NULL(strstr(buf, "left"));
}

// ---------------------------------------------------------------- WiFi survey (Stage B1)
static void test_wifi_survey_frag_and_change_event(void) {
  WifiChannel c;
  char buf[200];
  TEST_ASSERT_EQUAL_size_t(0, c.telemetry_frag(buf, sizeof(buf)));   // no survey yet → silent

  c.logic.begin(1000);
  c.logic.add_ap(0xAABBCCDDEE01ULL, -50);
  c.logic.add_ap(0xAABBCCDDEE02ULL, -62);
  c.logic.add_ap(0xAABBCCDDEE03ULL, -71);
  c.logic.commit(1000);
  TEST_ASSERT_EQUAL_size_t(0, c.next_event(buf, sizeof(buf), 1100)); // first survey = baseline
  TEST_ASSERT_TRUE(c.telemetry_frag(buf, sizeof(buf)) > 0);
  TEST_ASSERT_EQUAL_STRING("\"wap\":3,\"wrssi\":-50", buf);

  c.logic.begin(61000);                              // same world → no event
  c.logic.add_ap(0xAABBCCDDEE01ULL, -51);
  c.logic.add_ap(0xAABBCCDDEE02ULL, -60);
  c.logic.add_ap(0xAABBCCDDEE03ULL, -73);
  c.logic.commit(61000);
  TEST_ASSERT_EQUAL_size_t(0, c.next_event(buf, sizeof(buf), 61100));

  c.logic.begin(121000);                             // 2 vanished + 1 appeared → changed
  c.logic.add_ap(0xAABBCCDDEE01ULL, -50);
  c.logic.add_ap(0xAABBCCDDEE99ULL, -55);
  c.logic.commit(121000);
  TEST_ASSERT_TRUE(c.next_event(buf, sizeof(buf), 121100) > 0);
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"s\":\"wifi\""));
  TEST_ASSERT_NOT_NULL(strstr(buf, "radio environment changed"));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_loud_baseline_learns_only_quiet);
  RUN_TEST(test_loud_event_hysteresis);
  RUN_TEST(test_loud_channel_mute_is_total);
  RUN_TEST(test_ir_channel_press_event);
  RUN_TEST(test_ultra_median_smoothing);
  RUN_TEST(test_ultra_timeout_run_invalidates);
  RUN_TEST(test_ultra_close_event_debounce_and_hysteresis);
  RUN_TEST(test_radar_debounce_and_one_event_per_episode);
  RUN_TEST(test_registry_builds_combined_telemetry);
  RUN_TEST(test_registry_silent_when_no_driver_speaks);
  RUN_TEST(test_registry_bounded);
  RUN_TEST(test_registry_drains_events_round_robin);
  RUN_TEST(test_dht_decode_and_checksum);
  RUN_TEST(test_dht_freshness_and_failure_run);
  RUN_TEST(test_dht_channel_frag);
  RUN_TEST(test_pir_zones_debounce_episode_and_labels);
  RUN_TEST(test_wifi_survey_frag_and_change_event);
  return UNITY_END();
}
