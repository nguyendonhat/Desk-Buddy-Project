# Desk Buddy — shared agent instructions

Desk Buddy is a desk device using ESP32, e-paper, LCD, RTC, and Google Calendar. Before changing code or design, read `STATUS.md`, `README.md`, `PROJECT_SPEC.md`, `ARCHITECTURE.md`, `API.md`, and `HARDWARE_DESIGN.md`. Treat `STATUS.md` as the current handoff; update it when verified project status changes. If docs conflict, follow API.md for API details, PROJECT_SPEC.md for product intent, and STATUS.md for verified current state; fix related stale contradictions.

## Product intent

- E-paper is the main view for Calendar events, notes from event descriptions, and Pomodoro. LCD1602 is the auxiliary clock/status display.
- Use four buttons: MODE, UP, DOWN, OK. Do not redesign around three buttons.
- The web app is a small account-linking intermediary: Google sign-in, Calendar selection, connection status, logout/account switch. A `device_id` binds its Calendar connection; logging out removes the old link so the next account replaces it.
- ESP32 keeps local clock/Pomodoro behavior when offline. Calendar access goes through the Worker and existing API contract.
- Firmware is a prototype and is unfinished until the Wokwi flow works as intended. Rebuild incrementally; compilation is not proof of working behavior.

## Working safely as a team

- Start with `git status --short` and inspect existing diffs. Do not overwrite, reset, clean, or discard work you did not create.
- Work on one Git branch per issue: `issue-<number>-short-name`. Keep each change focused; do not edit another active branch's work. Coordinate file ownership so multiple agents do not edit the same files concurrently.
- Do not commit `config.h`, `.env`, `.dev.vars`, OAuth credentials, Wi-Fi passwords, device tokens, CA private keys, or other secrets. Use the example config files.
- Do not change API endpoints, JSON fields, OAuth scopes, hardware pins, or display driver without documenting the reason and updating related docs.
- The physical panel's exact model is not confirmed. Firmware currently selects `GxEPD2_290_GDEY029T94`; verify the rear label before claiming real-panel compatibility.
- `simulation/wokwi` is a mock sketch. It does not validate production OAuth, live Calendar requests, real WeAct refresh behavior, or physical wiring.
- A successful build is not proof that the firmware runs on the real device. Report build, simulator, and physical-hardware results separately.
- Never upload firmware to a board unless the user specifically asks. Never deploy or publish without authorization.

## Before handing work back

1. Run the narrowest relevant checks, then builds for changed firmware/apps when available.
2. Report exactly what files changed, commands run, pass/fail results, and what remains unverified.
3. Update `STATUS.md` only with facts you verified. Do not claim hardware tests from compilation or Wokwi.
4. Open a Pull Request for review; do not merge it unless the team explicitly asks.

## Useful checks

- ESP32 build: from `firmware/ESP32`, `pio run -e nodemcu-32s`
- ESP32 native tests: from `firmware/ESP32`, `pio test -e native` (needs GCC/G++)
- Wokwi mock build: from `simulation/wokwi`, `pio run -e esp32dev`
- Web: from `web`, `npm run build` and `npm run test:timezone`
- Worker: from `worker/cloudflare`, `npm run check` and `npm run test:timezone`

## Prompt for a teammate's AI

“Read `AGENTS.md`, `STATUS.md`, `README.md`, `PROJECT_SPEC.md`, `ARCHITECTURE.md`, `API.md`, and `HARDWARE_DESIGN.md`. Work only on Issue #N in my issue branch. First summarize the goal and files you expect to change. Preserve unrelated working-tree changes, follow the four-button product requirements and API contract, run relevant checks, and report passed checks and unverified behavior. Do not commit secrets or upload firmware/deploy unless I explicitly ask.”
