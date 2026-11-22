import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import ts from 'typescript';

const workerSource = readFileSync(new URL('../src/index.ts', import.meta.url), 'utf8');
const workerJavaScript = ts.transpileModule(workerSource, {
  compilerOptions: { module: ts.ModuleKind.ESNext, target: ts.ScriptTarget.ES2022 },
}).outputText;
const { CALENDAR_DISPLAY_TIME_ZONE, calendarWeekWindow, normalizeCalendarEvent } = await import(`data:text/javascript;base64,${Buffer.from(workerJavaScript).toString('base64')}`);

test('Worker fetches the current Monday-to-Monday week in Vietnam time', () => {
  // Wednesday 2026-11-18 at 12:00 in Vietnam.
  const window = calendarWeekWindow(Date.parse('2026-11-18T05:00:00.000Z'));
  assert.deepEqual(window, {
    timeMin: '2026-11-15T17:00:00.000Z',
    timeMax: '2026-11-22T17:00:00.000Z',
  });

  // Monday local time starts a new week, including month-boundary calculations.
  const nextWindow = calendarWeekWindow(Date.parse('2026-11-22T17:00:00.000Z'));
  assert.deepEqual(nextWindow, {
    timeMin: '2026-11-22T17:00:00.000Z',
    timeMax: '2026-11-29T17:00:00.000Z',
  });
});

test('Worker fixes Calendar response and event timezone metadata to Vietnam', () => {
  assert.equal(CALENDAR_DISPLAY_TIME_ZONE, 'Asia/Ho_Chi_Minh');
  const normalized = normalizeCalendarEvent({
    start: { dateTime: '2026-10-07T07:00:00+07:00', timeZone: 'Asia/Ho_Chi_Minh' },
    end: { dateTime: '2026-10-07T08:00:00+07:00', timeZone: 'Asia/Ho_Chi_Minh' },
  });
  assert.equal(normalized.start, '2026-10-07T07:00:00+07:00');
  assert.equal(normalized.start_time_zone, 'Asia/Ho_Chi_Minh');
  assert.equal(normalized.end_time_zone, 'Asia/Ho_Chi_Minh');
});

test('Worker ignores foreign event and calendar zones for display metadata', () => {
  const normalized = normalizeCalendarEvent({
    start: { dateTime: '2026-10-07T11:00:00Z', timeZone: 'America/New_York' },
    end: { dateTime: '2026-10-07T12:00:00Z', timeZone: 'America/New_York' },
  });
  assert.equal(normalized.start, '2026-10-07T11:00:00Z');
  assert.equal(normalized.start_time_zone, 'Asia/Ho_Chi_Minh');
  assert.equal(normalized.end_time_zone, 'Asia/Ho_Chi_Minh');
});

test('Worker leaves all-day dates date-only', () => {
  const normalized = normalizeCalendarEvent({
    start: { date: '2026-10-07' },
    end: { date: '2026-10-08' },
  });
  assert.equal(normalized.start, '2026-10-07');
  assert.equal(normalized.end, '2026-10-08');
  assert.equal(normalized.start_time_zone, 'Asia/Ho_Chi_Minh');
});
