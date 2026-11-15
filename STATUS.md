# Desk Buddy — current project status

Last checked: 2026-11-16

## Project goal

A NodeMCU-32S desk device that uses a WeAct 2.9-inch monochrome e-paper for Google Calendar events and event-description notes, with Pomodoro controls. LCD1602 + DS3231 provide an offline clock. Buttons: MODE, UP, DOWN, OK.

## Current code state

- ESP32 firmware and Wokwi mock both exist under `firmware/ESP32` and `simulation/wokwi`.
- Firmware prototype contains display-page, QR, button, note, Pomodoro, and deep-sleep code, but the team reports that the current Wokwi experience does not work as intended. Treat firmware as unfinished and rebuild/test it in small increments; prior compilation is not proof of working simulation.
- Button pin map in firmware/docs: MODE GPIO32, UP GPIO33, DOWN GPIO19, OK GPIO35. GPIO35 requires an external 10 kΩ pull-up to 3.3 V on real hardware.
- Firmware display driver currently targets `GxEPD2_290_GDEY029T94`. The exact physical WeAct panel revision still needs confirmation from its rear label.
- The Wokwi mock uses `GxEPD2_290_BS`, mock events, and virtual buttons. It is not the production firmware and does not test real Google OAuth or the physical WeAct panel.
- Wokwi sketch now has a mock NOTE flow (month grid → selected-day events → paged note/exit) and keeps the LCD on date/time while Pomodoro runs. PlatformIO compiles the Wokwi sketch via `src/main.cpp`; build passed, but button behavior in the running simulator is not yet verified.
- Google OAuth + Calendar code exists. Worker stores OAuth state, browser sessions, and tokens in `DESK_BUDDY_STORE`; Production and Preview use separate KV namespaces. PR #2 was merged to `main`; Production health returns `{"success":true,"service":"desk-buddy-api","environment":"production"}`. The user confirmed Google login and logout work. Huan reports Calendar events displayed on the physical ESP32; the updated Vietnam timezone display still needs a reflash and hardware check.
- Calendar event times are now forced to Vietnam (`Asia/Ho_Chi_Minh`, UTC+7) by the Worker, web app, and ESP32 rendering. Web/Worker timezone checks and the ESP32 firmware build passed on 2026-11-16; Huan must reflash and confirm the physical display after this change.
- Weather feed and location configuration are not implemented. Pomodoro break cycles are not implemented.

## Last verified checks

- Rechecked on 2026-11-16: `npm run build` and `npm run test:timezone` in `web`, `npm run check` and `npm run test:timezone` in `worker/cloudflare`, and `pio run -e nodemcu-32s` passed. `pio test -e native` could not compile because `gcc`/`g++` are missing.
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

1. Rebuild firmware in small increments, beginning with a minimal four-button Wokwi circuit and visible e-paper mock; verify each increment before adding network/API behavior.
2. Keep Wokwi behavior separate from production hardware validation. Confirm the real panel label and driver before claiming physical compatibility.
3. Once the simulator flow works, add RTC/LCD, local Pomodoro, then Wi-Fi and Calendar fetch; verify event titles and descriptions on e-paper.
4. Confirm real Calendar fetch reaches the device using a unique test `device_id`; never commit device tokens.
5. Choose a weather location/data source before implementing weather display.

## Team handoff rule

Use GitHub Issues as tasks, one branch per issue, and Pull Requests for review. Keep this file updated with verified progress so another teammate or AI can continue without relying on chat history.
