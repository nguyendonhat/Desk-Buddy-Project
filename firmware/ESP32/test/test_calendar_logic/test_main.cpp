#include <unity.h>

#include "calendar_logic.h"

using deskbuddy::isWithinReminderWindow;
using deskbuddy::parseEventLocalEpoch;
using deskbuddy::shouldBeepForEvent;

void test_utc_event_converts_to_vietnam_local_clock() {
  int64_t epoch = 0;
  TEST_ASSERT_TRUE(parseEventLocalEpoch("2026-10-07T00:00:00Z", 7 * 3600, epoch));
  TEST_ASSERT_EQUAL_INT64(1791356400LL, epoch);
}

void test_vietnam_offset_event_keeps_local_wall_time() {
  int64_t epoch = 0;
  TEST_ASSERT_TRUE(parseEventLocalEpoch("2026-10-07T07:00:00+07:00", 7 * 3600, epoch));
  TEST_ASSERT_EQUAL_INT64(1791356400LL, epoch);
}

void test_other_offset_converts_to_vietnam_time() {
  int64_t epoch = 0;
  TEST_ASSERT_TRUE(parseEventLocalEpoch("2026-10-07T02:00:00+02:00", 7 * 3600, epoch));
  TEST_ASSERT_EQUAL_INT64(1791356400LL, epoch);
}

void test_all_day_event_has_no_reminder_time() {
  int64_t epoch = 0;
  TEST_ASSERT_FALSE(parseEventLocalEpoch("2026-10-07", 7 * 3600, epoch));
}

void test_invalid_calendar_date_is_rejected() {
  int64_t epoch = 0;
  TEST_ASSERT_FALSE(parseEventLocalEpoch("2026-02-30T07:00:00+07:00", 7 * 3600, epoch));
}

void test_reminder_window_includes_edges_and_excludes_past_events() {
  TEST_ASSERT_TRUE(isWithinReminderWindow(1000, 1000, 900));
  TEST_ASSERT_TRUE(isWithinReminderWindow(1900, 1000, 900));
  TEST_ASSERT_FALSE(isWithinReminderWindow(1901, 1000, 900));
  TEST_ASSERT_FALSE(isWithinReminderWindow(999, 1000, 900));
}

void test_event_only_beeps_once_until_new_event_key() {
  TEST_ASSERT_FALSE(shouldBeepForEvent("event-a", "event-a"));
  TEST_ASSERT_TRUE(shouldBeepForEvent("event-b", "event-a"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_utc_event_converts_to_vietnam_local_clock);
  RUN_TEST(test_vietnam_offset_event_keeps_local_wall_time);
  RUN_TEST(test_other_offset_converts_to_vietnam_time);
  RUN_TEST(test_all_day_event_has_no_reminder_time);
  RUN_TEST(test_invalid_calendar_date_is_rejected);
  RUN_TEST(test_reminder_window_includes_edges_and_excludes_past_events);
  RUN_TEST(test_event_only_beeps_once_until_new_event_key);
  return UNITY_END();
}

