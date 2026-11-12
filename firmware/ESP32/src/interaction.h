#pragma once
#include <stdint.h>

namespace deskbuddy {
enum class Screen : uint8_t { Calendar = 0, Note = 1, Pomodoro = 2, Account = 3 };

struct NavigationState {
  Screen screen = Screen::Calendar;
  uint8_t dayOffset = 0;       // Calendar mode: 0=today, 1=tomorrow.
  uint8_t eventIndex = 0;      // Event/note page within the selected day.
  uint8_t notePage = 0;
  uint16_t pomodoroMinutes = 25;
  bool pomodoroRunning = false;
};

inline void nextScreen(NavigationState& state) {
  state.screen = static_cast<Screen>((static_cast<uint8_t>(state.screen) + 1) % 4);
}
inline void scrollUp(NavigationState& state) {
  if (state.screen == Screen::Pomodoro) {
    if (state.pomodoroMinutes < 60) ++state.pomodoroMinutes;
  } else if (state.screen == Screen::Calendar || state.screen == Screen::Note) {
    state.dayOffset = 0;
    if (state.eventIndex > 0) --state.eventIndex;
  }
}
inline void scrollDown(NavigationState& state) {
  if (state.screen == Screen::Pomodoro) {
    if (state.pomodoroMinutes > 1) --state.pomodoroMinutes;
  } else if (state.screen == Screen::Calendar || state.screen == Screen::Note) {
    state.dayOffset = 1;
    state.eventIndex = 0;
  }
}
inline void confirm(NavigationState& state) {
  if (state.screen == Screen::Pomodoro) state.pomodoroRunning = !state.pomodoroRunning;
  else if (state.screen == Screen::Calendar) state.screen = Screen::Note;
  else if (state.screen == Screen::Note) state.screen = Screen::Calendar;
}
}  // namespace deskbuddy
