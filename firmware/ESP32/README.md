# ESP32 Calendar firmware

This first firmware increment connects a NodeMCU-32S to Wi-Fi, reads the DS3231, fetches normalized Calendar events from the Worker, shows the clock on the LCD1602, and shows up to four events on the WeAct 2.9-inch monochrome e-paper. It also gives a short LED/buzzer reminder for an event starting within 15 minutes.

## Hardware assumptions

- NodeMCU-32S with ESP-WROOM-32.
- WeAct Studio 2.9-inch black/white SPI e-paper. The selected driver class is `GxEPD2_290_GDEY029T94`; verify the controller/revision printed on the display before wiring.
- LCD1602 I²C address `0x27`; change the address in `src/main.cpp` if an I²C scan reports another address.
- DS3231 on GPIO21/22.
- E-paper and I²C pins follow `HARDWARE_DESIGN.md`.
- The RGB LED is assumed common-cathode and the buzzer is switched by a transistor from GPIO17.

## Buttons and display pages

- MODE cycles Calendar, Notes, Pomodoro, and Google-account setup.
- Tap MODE to change page; hold it for 5 seconds to put the ESP32 into deep sleep, then tap it to wake.
- UP/DOWN switch between today and tomorrow in calendar views and adjust Pomodoro length between 1 and 60 minutes.
- OK toggles the event/note view or starts/pauses the Pomodoro session.
- Wire MODE/UP/DOWN from GPIO32/33/19 to GND. Wire OK from GPIO35 to GND and add an external 10 kΩ pull-up to 3.3 V because GPIO35 has no internal pull-up.
- The Account page draws a QR linking to `WEB_APP_URL?device_id=...`. Set `WEB_APP_URL` in local `src/config.h` to the deployed Desk Buddy web app. Keep the base URL short (no query string) so it fits the version-8 QR. The QR contains only the public web URL and device ID; it does not contain Google credentials or tokens.

## Build setup

1. Install PlatformIO and open `firmware/ESP32` as the project.
2. Copy `src/config.example.h` to `src/config.h`.
3. Set the Wi-Fi name/password, deployed HTTPS Worker URL, `DB001`, and the matching device token configured in the Worker secret `DEVICE_API_TOKENS`.
4. Set `WEB_APP_URL` to the deployed HTTPS web app and put the root CA PEM that validates the Worker URL in `API_ROOT_CA_PEM`. TLS certificate validation stays enabled. The web app's `VITE_API_BASE_URL` must point to that same deployed Worker.
5. Connect the hardware and upload the `nodemcu-32s` environment.

Do not commit `src/config.h`; it contains Wi-Fi and device credentials. The root `.gitignore` excludes it.

The Worker stores OAuth state, browser sessions, and Google Calendar connections in Cloudflare KV. Production and Preview use separate namespaces. The user has confirmed Google login/logout on the deployed web app; Calendar transfer to a physical ESP32 remains unverified.

## Link a Google account with the QR

1. Deploy the web app and Worker, then configure the Google OAuth client with the Worker's `/api/oauth/google/callback` redirect URI and the web app's authorized origin.
2. Copy `src/config.example.h` to `src/config.h` and set `WEB_APP_URL` to the web app's public HTTPS root (no query string), and set the same `DEVICE_ID` used by the Worker API token configuration.
3. Build and upload the firmware. Press MODE until **Tai khoan Google** appears, then scan its QR using a phone camera.
4. Choose the Google account, review the calendar read permission, and grant it. The phone returns to the web app for that device ID; choose the calendar to sync.
5. To switch accounts, use **Đăng xuất** on the web page, then scan the device QR and sign in again. Google account selection is shown during sign-in.

For production, the Worker cookie uses `SameSite=None; Secure` so the web app can send the session on cross-site HTTPS requests. Local HTTP development uses `SameSite=Lax`. OAuth state and account links persist in the environment's KV namespace.

## Current behavior and limits

- The firmware is a prototype, not an accepted working implementation. The team reports that current Wokwi behavior does not work as intended, although prior PlatformIO compilation succeeded. Rebuild in small verified increments; do not treat the old UI as a finished requirement.

- RTC time is synchronized from NTP when Wi-Fi is available and continues from DS3231 when Wi-Fi is lost.
- Calendar refresh runs every five minutes; the e-paper redraws only after a successful fetch.
- Calendar reminder time conversion assumes Vietnam UTC+7. Change `LOCAL_UTC_OFFSET_SECONDS` and the POSIX TZ string if using a different local zone.
- Calendar rendering/sync, Pomodoro, and offline behavior must be verified again as firmware is rebuilt. Break intervals, event caching, and enclosure wiring validation are not confirmed complete.

## Test without hardware

From this folder, run `pio test -e native` to test RFC3339 event-time conversion, reminder timing, and button navigation on the computer. This does not need an ESP32 or connected peripherals. To compile the full firmware without uploading it, run `pio run -e nodemcu-32s` after creating `src/config.h` from the example. The native test does not validate QR scanning, display drivers, GPIO wiring, TLS connectivity, or real RTC behavior.
