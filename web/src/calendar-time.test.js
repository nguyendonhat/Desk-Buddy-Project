import assert from 'node:assert/strict';
import test from 'node:test';
import {
  calendarEventDateKey,
  formatCalendarDayHeading,
  formatCalendarEventTime,
} from './calendar-time.js';

test('offset datetime displays the Vietnam wall time', () => {
  const start = '2026-10-07T07:00:00+07:00';
  assert.equal(formatCalendarEventTime(start), '07:00');
  assert.equal(calendarEventDateKey(start), '2026-10-07');
});

test('UTC instant displays as Vietnam time regardless of event timezone', () => {
  const start = '2026-10-07T00:00:00Z';
  assert.equal(formatCalendarEventTime(start), '07:00');
  assert.equal(calendarEventDateKey(start), '2026-10-07');
});

test('all-day date remains date-only and is not shifted by timezone conversion', () => {
  const start = '2026-10-07';
  assert.equal(formatCalendarEventTime(start), 'Cả ngày');
  assert.equal(calendarEventDateKey(start), '2026-10-07');
  assert.match(formatCalendarDayHeading('2026-10-07'), /07\/10\/2026/);
});

test('foreign calendar timestamps are still displayed in Vietnam time', () => {
  const start = '2026-10-07T11:00:00Z';
  assert.equal(formatCalendarEventTime(start), '18:00');
  assert.equal(calendarEventDateKey(start), '2026-10-07');
});

test('Vietnam date grouping accounts for a UTC event crossing midnight', () => {
  const start = '2026-10-06T20:00:00Z';
  assert.equal(formatCalendarEventTime(start), '03:00');
  assert.equal(calendarEventDateKey(start), '2026-10-07');
});

test('timezone-less wall datetime is preserved rather than interpreted in browser timezone', () => {
  assert.equal(formatCalendarEventTime('2026-10-07T07:00:00'), '07:00');
});
