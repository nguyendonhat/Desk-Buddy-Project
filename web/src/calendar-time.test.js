import assert from 'node:assert/strict';
import test from 'node:test';
import {
  calendarEventDateKey,
  formatCalendarDayHeading,
  formatCalendarEventTime,
} from './calendar-time.js';

test('offset datetime displays the Google Calendar wall time (Vietnam)', () => {
  const start = '2026-10-07T07:00:00+07:00';
  assert.equal(formatCalendarEventTime(start, 'Asia/Ho_Chi_Minh'), '07:00');
  assert.equal(calendarEventDateKey(start, 'Asia/Ho_Chi_Minh'), '2026-10-07');
});

test('UTC instant plus event timezone displays the corresponding local wall time', () => {
  const start = '2026-10-07T00:00:00Z';
  assert.equal(formatCalendarEventTime(start, 'Asia/Ho_Chi_Minh'), '07:00');
  assert.equal(calendarEventDateKey(start, 'Asia/Ho_Chi_Minh'), '2026-10-07');
});

test('all-day date remains date-only and is not shifted by timezone conversion', () => {
  const start = '2026-10-07';
  assert.equal(formatCalendarEventTime(start, 'Asia/Ho_Chi_Minh'), 'Cả ngày');
  assert.equal(calendarEventDateKey(start, 'Asia/Ho_Chi_Minh'), '2026-10-07');
  assert.match(formatCalendarDayHeading('2026-10-07'), /07\/10\/2026/);
});

test('IANA timezone other than Vietnam is respected (New York, EDT)', () => {
  const start = '2026-10-07T11:00:00Z';
  assert.equal(formatCalendarEventTime(start, 'America/New_York'), '07:00');
  assert.equal(calendarEventDateKey(start, 'America/New_York'), '2026-10-07');
});

test('timezone-less wall datetime is preserved rather than interpreted in browser timezone', () => {
  assert.equal(formatCalendarEventTime('2026-10-07T07:00:00', 'Asia/Ho_Chi_Minh'), '07:00');
});
