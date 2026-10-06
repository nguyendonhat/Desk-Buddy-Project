const DATE_ONLY_PATTERN = /^\d{4}-\d{2}-\d{2}$/;
const EXPLICIT_OFFSET_PATTERN = /(?:Z|[+-]\d{2}:\d{2})$/i;
const DATE_TIME_PATTERN = /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2})(?::(\d{2})(?:\.(\d+))?)?/;

function hasExplicitOffset(value) {
  return EXPLICIT_OFFSET_PATTERN.test(value);
}

function zonedDateTimeParts(value, timeZone) {
  const parts = new Intl.DateTimeFormat('en-CA', {
    timeZone,
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    hourCycle: 'h23',
  }).formatToParts(new Date(value));
  return Object.fromEntries(parts.filter((part) => part.type !== 'literal').map(({ type, value: partValue }) => [type, partValue]));
}

function localDateTimeAsInstant(value, timeZone) {
  const match = value.match(DATE_TIME_PATTERN);
  if (!match || !timeZone) return Date.parse(value);

  const [, year, month, day, hour, minute, second = '00', fraction = '0'] = match;
  const target = {
    year: Number(year), month: Number(month), day: Number(day),
    hour: Number(hour), minute: Number(minute), second: Number(second),
  };
  const targetAsUtc = Date.UTC(target.year, target.month - 1, target.day, target.hour, target.minute, target.second);
  const milliseconds = Number(`0.${fraction}`) * 1000;
  let instant = targetAsUtc + milliseconds;

  // Resolve a local wall time using its IANA zone. Iteration naturally accounts for DST offsets.
  for (let attempt = 0; attempt < 4; attempt += 1) {
    const parts = zonedDateTimeParts(new Date(instant).toISOString(), timeZone);
    const representedAsUtc = Date.UTC(
      Number(parts.year), Number(parts.month) - 1, Number(parts.day),
      Number(parts.hour), Number(parts.minute), target.second,
    );
    const correction = targetAsUtc - representedAsUtc;
    instant += correction;
    if (correction === 0) break;
  }
  return instant;
}

export function calendarEventTimestamp(value, timeZone) {
  if (DATE_ONLY_PATTERN.test(value)) return Date.parse(`${value}T00:00:00Z`);
  return hasExplicitOffset(value) ? Date.parse(value) : localDateTimeAsInstant(value, timeZone);
}

export function calendarEventDateKey(value, timeZone) {
  if (DATE_ONLY_PATTERN.test(value) || !hasExplicitOffset(value) || !timeZone) return value.slice(0, 10);
  try {
    const parts = zonedDateTimeParts(value, timeZone);
    return `${parts.year}-${parts.month}-${parts.day}`;
  } catch {
    return value.slice(0, 10);
  }
}

export function formatCalendarEventTime(value, timeZone) {
  if (DATE_ONLY_PATTERN.test(value)) return 'Cả ngày';
  if (hasExplicitOffset(value) && timeZone) {
    try {
      return new Intl.DateTimeFormat('vi-VN', {
        timeZone,
        hour: '2-digit',
        minute: '2-digit',
        hourCycle: 'h23',
      }).format(new Date(value));
    } catch {
      // If a provider ever returns an unknown zone, preserve the wall-clock value in its datetime.
    }
  }
  const match = value.match(/T(\d{2}):(\d{2})/);
  return match ? `${match[1]}:${match[2]}` : value;
}

export function formatCalendarDayHeading(dateKey) {
  const date = new Date(`${dateKey}T12:00:00Z`);
  const weekday = new Intl.DateTimeFormat('vi-VN', { weekday: 'long', timeZone: 'UTC' }).format(date);
  const [year, month, day] = dateKey.split('-');
  return `${weekday}, ${day}/${month}/${year}`;
}
