#pragma once

#include <cstdint>
#include <string>

namespace deskbuddy {

inline bool isLeapYear(int year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

inline int daysInMonth(int year, int month) {
  static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 0;
  return month == 2 && isLeapYear(year) ? 29 : days[month - 1];
}

inline bool readDigits(const std::string& value, size_t start, size_t count, int& out) {
  if (start + count > value.size()) return false;
  out = 0;
  for (size_t i = start; i < start + count; ++i) {
    if (value[i] < '0' || value[i] > '9') return false;
    out = out * 10 + (value[i] - '0');
  }
  return true;
}

// Days from 1970-01-01; Gregorian calendar arithmetic, independent of RTC libraries.
inline int64_t daysFromCivil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
  const unsigned dayOfYear = (153 * ((month > 2 ? month - 3 : month + 9)) + 2) / 5 + day - 1;
  const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(dayOfEra) - 719468;
}

// Convert an RFC3339 event start to the RTC's local-wall-time epoch. Date-only events
// intentionally return false because they do not have a reminder time.
inline bool parseEventLocalEpoch(const std::string& value, int32_t localUtcOffsetSeconds,
                                 int64_t& localEpoch) {
  if (value.size() < 16 || value[4] != '-' || value[7] != '-' || value[10] != 'T' ||
      value[13] != ':') return false;

  int year, month, day, hour, minute, second = 0;
  if (!readDigits(value, 0, 4, year) || !readDigits(value, 5, 2, month) ||
      !readDigits(value, 8, 2, day) || !readDigits(value, 11, 2, hour) ||
      !readDigits(value, 14, 2, minute)) return false;
  if (year < 1 || month < 1 || month > 12 || day < 1 || day > daysInMonth(year, month) ||
      hour > 23 || minute > 59) return false;

  size_t pos = 16;
  if (pos < value.size() && value[pos] == ':') {
    if (!readDigits(value, pos + 1, 2, second) || second > 59) return false;
    pos += 3;
    if (pos < value.size() && value[pos] == '.') {
      ++pos;
      const size_t fractionStart = pos;
      while (pos < value.size() && value[pos] >= '0' && value[pos] <= '9') ++pos;
      if (pos == fractionStart) return false;
    }
  }

  int32_t eventOffset = localUtcOffsetSeconds;
  if (pos < value.size() && value[pos] == 'Z') {
    eventOffset = 0;
    ++pos;
  } else if (pos < value.size() && (value[pos] == '+' || value[pos] == '-')) {
    const int sign = value[pos] == '-' ? -1 : 1;
    int offsetHours, offsetMinutes;
    if (pos + 6 != value.size() || value[pos + 3] != ':' ||
        !readDigits(value, pos + 1, 2, offsetHours) ||
        !readDigits(value, pos + 4, 2, offsetMinutes) || offsetHours > 23 || offsetMinutes > 59) return false;
    eventOffset = sign * (offsetHours * 3600 + offsetMinutes * 60);
    pos += 6;
  }
  if (pos != value.size()) return false;

  const int64_t wallEpoch = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) * 86400 +
      hour * 3600 + minute * 60 + second;
  localEpoch = wallEpoch - eventOffset + localUtcOffsetSeconds;
  return true;
}

inline bool isWithinReminderWindow(int64_t eventEpoch, int64_t nowEpoch, int64_t windowSeconds) {
  const int64_t secondsUntil = eventEpoch - nowEpoch;
  return secondsUntil >= 0 && secondsUntil <= windowSeconds;
}

inline bool shouldBeepForEvent(const std::string& eventStart, const std::string& lastReminderKey) {
  return eventStart != lastReminderKey;
}

}  // namespace deskbuddy

