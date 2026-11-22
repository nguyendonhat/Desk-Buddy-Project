# Desk Buddy — current project status

Last checked: 2026-11-22

## Project goal

A NodeMCU-32S desk device that uses a WeAct 2.9-inch monochrome e-paper for Google Calendar events and event-description notes, with Pomodoro controls. LCD1602 + DS3231 provide an offline clock. Buttons: MODE, UP, DOWN, OK.

## Current code state

- On 2026-11-22, `firmware/ESP32/src/main.cpp` was replaced with the user's `main (1).cpp`; matching SHA-256 hashes confirmed the copied file. The new source adds UTF-8-aware note line wrapping and changes several calendar/Pomodoro timings and behaviors. A firmware build was attempted but could not run because `pio` is not available in this shell; compilation and runtime behavior remain unverified.
- ESP32 firmware and Wokwi mock both exist under `firmware/ESP32` and `simulation/wokwi`.
- Production firmware now has a seven-day (Monday–Sunday) calendar selector, selected-day event/note view with 4-second title/note alternation and 15-second return to the week, plus configurable Pomodoro cycles/work/break timing. Pomodoro continues while changing modes and has transition/completion buzzer signals. The previous firmware revision compiled; the 2026-11-22 source replacement has not compiled in this environment. Wokwi and physical behavior are unverified. Deep sleep has been removed; MODE changes screens.
- Button pin map in firmware/docs: MODE GPIO32, UP GPIO33, DOWN GPIO19, OK GPIO35. GPIO35 requires an external 10 kΩ pull-up to 3.3 V on real hardware.
- Firmware display driver currently targets `GxEPD2_290_GDEY029T94`. The exact physical WeAct panel revision still needs confirmation from its rear label.
- The Wokwi mock uses `GxEPD2_290_BS`, mock events, and virtual buttons. It is not the production firmware and does not test real Google OAuth or the physical WeAct panel.
- Wokwi sketch now has a mock NOTE flow (month grid → selected-day events → paged note/exit) and keeps the LCD on date/time while Pomodoro runs. PlatformIO compiles the Wokwi sketch via `src/main.cpp`; build passed, but button behavior in the running simulator is not yet verified.
- Google OAuth + Calendar code exists. Worker stores OAuth state, browser sessions, and tokens in `DESK_BUDDY_STORE`; Production and Preview use separate KV namespaces. PR #2 was merged to `main`; Production health returns `{"success":true,"service":"desk-buddy-api","environment":"production"}`. The user confirmed Google login and logout work. Huan reports Calendar events displayed on the physical ESP32; the updated Vietnam timezone display still needs a reflash and hardware check.
- Calendar event times are now forced to Vietnam (`Asia/Ho_Chi_Minh`, UTC+7) by the Worker, web app, and ESP32 rendering. Web/Worker timezone checks and the ESP32 firmware build passed on 2026-11-16; Huan must reflash and confirm the physical display after this change.
- Worker Calendar fetching now uses the current Vietnam calendar week (Monday 00:00 to next Monday 00:00), rather than a rolling seven-day window. Worker Production deployment was verified on 2026-11-21; latest deployment version is `019a1147-69f4-4649-a0cd-2e1960ee82be`. Production log settings remain enabled. The deployment list is verified, but `/api/health` could not be checked from this machine because its DNS resolver could not resolve the Worker hostname.
- ESP32 now supports Wi-Fi setup through a temporary password-protected access point and local web page. It scans for SSIDs, stores credentials in NVS, opens setup automatically if no saved network connects, and can reopen setup by pressing OK on the Account screen. Firmware build passed after the control-flow update; phone flow and hardware behavior remain unverified.
- Weather is outside the product scope. Pomodoro settings and break cycles are now implemented in firmware; runtime behavior still needs verification.

## Last verified checks

- On 2026-11-22, `npm run build` and `npm run test:timezone` passed in `web`; `npm run check` and `npm run test:timezone` passed in `worker/cloudflare` (4 Worker and 6 Web timezone tests). The production firmware build was attempted separately, but `pio` is unavailable in this shell. No hardware test was performed.
- On 2026-11-21, Worker `npm run check` and `npm run test:timezone` passed after changing the Calendar window (4 tests passed). Wrangler deployed `desk-buddy-api` Production; latest deployment shows version `019a1147-69f4-4649-a0cd-2e1960ee82be` in deployment history. Production `/api/health` request was attempted but blocked by local DNS resolution failure; no live Calendar request was made.

- Rechecked on 2026-11-16: `npm run build` and `npm run test:timezone` in `web`, `npm run check` and `npm run test:timezone` in `worker/cloudflare`, and `pio run -e nodemcu-32s` passed. `pio test -e native` could not compile because `gcc`/`g++` are missing.
- After adding Wi-Fi provisioning and removing deep sleep, `pio run -e nodemcu-32s` passed on 2026-11-16 using the new 4 MB partition table. The firmware occupies 1,330,276 bytes (65.5% of the 2,031,616-byte app slot); compile does not verify the phone portal or physical device.
- After implementing week navigation, event/note alternation and configurable Pomodoro, `pio run -e nodemcu-32s` passed on 2026-11-16. The firmware occupies 1,333,364 bytes (65.6% of the 2,031,616-byte app slot). This confirms compilation only; button flow, e-paper refresh, buzzer timing, Wokwi behavior, and physical hardware remain unverified.
- `pio run -e nodemcu-32s`: previously passed; firmware compiled, approximately 97% of configured app flash. This does not establish that the prototype works in Wokwi or on hardware.
- `pio run -e esp32dev` in `simulation/wokwi`: passed on 2026-11-09 after configuring PlatformIO to build the mock sketch. This is compile-only; the team previously reported simulation issues, and current runtime behavior is not yet accepted.
- `npm run build` in `web`: passed.
- `npm run check` in `worker/cloudflare`: passed.
- Timezone tests: 6 web + 3 Worker cases passed on 2026-11-16.
- Worker Wrangler dry-run: previously passed; bundle included the production KV binding and Pages origin.
- Cloudflare Preview build for commit `5fb1919` completed successfully (version `21736982`). `https://issue-1-oauth-kv-desk-buddy-api.deskbuddy67.workers.dev/api/health` returns `{"success":true,"service":"desk-buddy-api","environment":"preview"}`. Preview uses its own KV namespace and does not inherit Production OAuth secrets, so a full Google login has not been tested there.
- Production health was checked after PR #2 merged and returns `{"success":true,"service":"desk-buddy-api","environment":"production"}`. The user confirmed deployed web login/logout work. Calendar fetch/render end-to-end remains unverified.
- `pio test -e native`: not run successfully; machine lacks `gcc/g++`.
- Physical ESP32/e-paper upload and display: not verified. Last port check only showed COM3/COM4 Bluetooth, no ESP32 serial device.
- QR scan on physical panel: not verified.

## Highest-priority next steps

1. Verify the production week/event/note and Pomodoro button flows in Wokwi or a controlled bench test; mock and production implementations are separate.
2. Confirm the real panel label and driver before claiming physical compatibility; verify e-paper alternation/refresh and buzzer intervals on hardware.
3. Reconfirm Calendar transfer reaches the device using a unique test `device_id`; never commit device tokens.

## Team handoff rule

Use GitHub Issues as tasks, one branch per issue, and Pull Requests for review. Keep this file updated with verified progress so another teammate or AI can continue without relying on chat history.
