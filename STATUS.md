# Desk Buddy — current project status

Last checked: 2026-11-08

## Project goal

A NodeMCU-32S desk device that uses a WeAct 2.9-inch monochrome e-paper for Google Calendar events and event-description notes, with Pomodoro controls. LCD1602 + DS3231 provide an offline clock. Buttons: MODE, UP, DOWN, OK.

## Current code state

- ESP32 firmware and Wokwi mock both exist under `firmware/ESP32` and `simulation/wokwi`.
- Firmware implements display pages, a QR link to the account web app, button navigation, note pagination, Pomodoro countdown, and deep sleep on a 5-second MODE hold.
- Button pin map in firmware/docs: MODE GPIO32, UP GPIO33, DOWN GPIO19, OK GPIO35. GPIO35 requires an external 10 kΩ pull-up to 3.3 V on real hardware.
- Firmware display driver currently targets `GxEPD2_290_GDEY029T94`. The exact physical WeAct panel revision still needs confirmation from its rear label.
- The Wokwi mock uses `GxEPD2_290_BS`, mock events, and virtual buttons. It is not the production firmware and does not test real Google OAuth or the physical WeAct panel.
- Google OAuth + Calendar code exists. Worker source now uses the `DESK_BUDDY_STORE` KV binding for OAuth state, browser sessions, and tokens. Separate production and Preview KV namespaces exist. Production is bound to `desk-buddy-api`; the updated Worker source has not yet been deployed or verified against live Google OAuth.
- Weather feed and location configuration are not implemented. Pomodoro break cycles are not implemented.

## Last verified checks

- `pio run -e nodemcu-32s`: passed; firmware compiled, approximately 97% of the configured app flash limit.
- `pio run -e esp32dev` in `simulation/wokwi`: passed.
- `npm run build` in `web`: passed.
- `npm run check` in `worker/cloudflare`: passed.
- Timezone tests: 5 web + 4 Worker cases passed.
- Worker Wrangler dry-run: passed; bundle includes the production KV binding and Pages origin. Live Worker still reports the old `desk-buddy-worker`/`local` health response, confirming the updated source is not deployed.
- Cloudflare Preview build exposed that Wrangler requires an explicit `previews` configuration block. Preview now has its own KV namespace; a new Preview build is pending after the config update.
- `pio test -e native`: not run successfully; machine lacks `gcc/g++`.
- Physical ESP32/e-paper upload and display: not verified. Last port check only showed COM3/COM4 Bluetooth, no ESP32 serial device.
- QR scan on physical panel: not verified.

## Highest-priority next steps

1. Deploy the updated Worker source and verify OAuth returns to the Pages app with `connected: true`; then verify calendar listing and event fetch.
2. Connect the ESP32 and confirm a USB serial COM port appears; inspect boot Serial output.
3. Verify the exact e-paper model label and compare it to the selected GxEPD2 driver.
4. Test e-paper alone with a minimal Hello screen before diagnosing integrated UI; then add LCD/RTC/buttons.
5. Verify physical pin wiring against `HARDWARE_DESIGN.md`, including GPIO35 pull-up and LCD level shifting.
6. After hardware bring-up, test QR linking and Calendar fetch with real device credentials; never commit secrets.
7. Choose a weather location/data source before implementing weather display.

## Team handoff rule

Use GitHub Issues as tasks, one branch per issue, and Pull Requests for review. Keep this file updated with verified progress so another teammate or AI can continue without relying on chat history.
