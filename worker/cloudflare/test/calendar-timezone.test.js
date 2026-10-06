import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import ts from 'typescript';

const workerSource = readFileSync(new URL('../src/index.ts', import.meta.url), 'utf8');
const workerJavaScript = ts.transpileModule(workerSource, {
  compilerOptions: { module: ts.ModuleKind.ESNext, target: ts.ScriptTarget.ES2022 },
}).outputText;
const { normalizeCalendarEvent } = await import(`data:text/javascript;base64,${Buffer.from(workerJavaScript).toString('base64')}`);

test('Worker preserves an offset datetime and its IANA timezone metadata', () => {
  const normalized = normalizeCalendarEvent({
    start: { dateTime: '2026-10-07T07:00:00+07:00', timeZone: 'Asia/Ho_Chi_Minh' },
    end: { dateTime: '2026-10-07T08:00:00+07:00', timeZone: 'Asia/Ho_Chi_Minh' },
  });
  assert.equal(normalized.start, '2026-10-07T07:00:00+07:00');
  assert.equal(normalized.start_time_zone, 'Asia/Ho_Chi_Minh');
});

test('Worker carries calendar timezone when Google gives a UTC instant and event timezone', () => {
  const normalized = normalizeCalendarEvent({
    start: { dateTime: '2026-10-07T00:00:00Z', timeZone: 'Asia/Ho_Chi_Minh' },
    end: { dateTime: '2026-10-07T01:00:00Z', timeZone: 'Asia/Ho_Chi_Minh' },
  }, 'UTC');
  assert.equal(normalized.start, '2026-10-07T00:00:00Z');
  assert.equal(normalized.start_time_zone, 'Asia/Ho_Chi_Minh');
});

test('Worker leaves all-day dates date-only', () => {
  const normalized = normalizeCalendarEvent({
    start: { date: '2026-10-07' },
    end: { date: '2026-10-08' },
  }, 'Asia/Ho_Chi_Minh');
  assert.equal(normalized.start, '2026-10-07');
  assert.equal(normalized.end, '2026-10-08');
  assert.equal(normalized.start_time_zone, 'Asia/Ho_Chi_Minh');
});

test('Worker preserves another event IANA timezone without fixed offset conversion', () => {
  const normalized = normalizeCalendarEvent({
    start: { dateTime: '2026-10-07T11:00:00Z', timeZone: 'America/New_York' },
    end: { dateTime: '2026-10-07T12:00:00Z', timeZone: 'America/New_York' },
  });
  assert.equal(normalized.start, '2026-10-07T11:00:00Z');
  assert.equal(normalized.start_time_zone, 'America/New_York');
});
