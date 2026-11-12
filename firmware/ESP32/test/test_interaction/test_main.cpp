#include <unity.h>
#include "interaction.h"
using deskbuddy::NavigationState;
using deskbuddy::Screen;
void test_mode_cycles_views() { NavigationState s; deskbuddy::nextScreen(s); TEST_ASSERT_EQUAL((int)Screen::Note,(int)s.screen); deskbuddy::nextScreen(s); TEST_ASSERT_EQUAL((int)Screen::Pomodoro,(int)s.screen); deskbuddy::nextScreen(s); deskbuddy::nextScreen(s); TEST_ASSERT_EQUAL((int)Screen::Calendar,(int)s.screen); }
void test_calendar_scroll_today_tomorrow() { NavigationState s; deskbuddy::scrollDown(s); TEST_ASSERT_EQUAL_UINT8(1,s.dayOffset); deskbuddy::scrollUp(s); TEST_ASSERT_EQUAL_UINT8(0,s.dayOffset); }
void test_pomodoro_clamps_and_toggles() { NavigationState s; s.screen=Screen::Pomodoro; s.pomodoroMinutes=60; deskbuddy::scrollUp(s); TEST_ASSERT_EQUAL_UINT16(60,s.pomodoroMinutes); s.pomodoroMinutes=1; deskbuddy::scrollDown(s); TEST_ASSERT_EQUAL_UINT16(1,s.pomodoroMinutes); deskbuddy::confirm(s); TEST_ASSERT_TRUE(s.pomodoroRunning); }
int main(int,char**) { UNITY_BEGIN(); RUN_TEST(test_mode_cycles_views); RUN_TEST(test_calendar_scroll_today_tomorrow); RUN_TEST(test_pomodoro_clamps_and_toggles); return UNITY_END(); }
