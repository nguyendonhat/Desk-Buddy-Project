# Desk Buddy — current project status

Last checked: 2026-11-08

## Project goal

A NodeMCU-32S desk device that uses a WeAct 2.9-inch monochrome e-paper for Google Calendar events and event-description notes, with Pomodoro controls. LCD1602 + DS3231 provide an offline clock. Buttons: MODE, UP, DOWN, OK.

## Current code state

- ESP32 firmware and Wokwi mock both exist under `firmware/ESP32` and `simulation/wokwi`.
- Firmware prototype contains display-page, QR, button, note, Pomodoro, and deep-sleep code, but the team reports that the current Wokwi experience does not work as intended. Treat firmware as unfinished and rebuild/test it in small increments; prior compilation is not proof of working simulation.
- Button pin map in firmware/docs: MODE GPIO32, UP GPIO33, DOWN GPIO19, OK GPIO35. GPIO35 requires an external 10 kΩ pull-up to 3.3 V on real hardware.
- Firmware display driver currently targets `GxEPD2_290_GDEY029T94`. The exact physical WeAct panel revision still needs confirmation from its rear label.
- The Wokwi mock uses `GxEPD2_290_BS`, mock events, and virtual buttons. It is not the production firmware and does not test real Google OAuth or the physical WeAct panel.
- Google OAuth + Calendar code exists. Worker stores OAuth state, browser sessions, and tokens in `DESK_BUDDY_STORE`; Production and Preview use separate KV namespaces. PR #2 was merged to `main`; Production health returns `{"success":true,"service":"desk-buddy-api","environment":"production"}`. The user confirmed Google login and logout work. Calendar fetch/render on the physical ESP32 remains unverified.
- Weather feed and location configuration are not implemented. Pomodoro break cycles are not implemented.

## Last verified checks

- `pio run -e nodemcu-32s`: previously passed; firmware compiled, approximately 97% of configured app flash. This does not establish that the prototype works in Wokwi or on hardware.
- `pio run -e esp32dev` in `simulation/wokwi`: previously passed, but the team reports the simulation does not work as intended. Recheck after rebuilding; current functional status is not accepted.
- `npm run build` in `web`: passed.
- `npm run check` in `worker/cloudflare`: passed.
- Timezone tests: 5 web + 4 Worker cases passed.
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
