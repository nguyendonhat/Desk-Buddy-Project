# ESP32 Calendar firmware

The production prototype connects a NodeMCU-32S to Wi-Fi, reads the DS3231, fetches normalized Calendar events from the Worker, shows date/time and connection status on the LCD1602, and provides Calendar, event-note, Pomodoro, and account screens on the WeAct 2.9-inch monochrome e-paper. It uses the buzzer for event reminders and Pomodoro transitions; the prototype omits the status LED.

## Hardware assumptions

- NodeMCU-32S with ESP-WROOM-32.
- WeAct Studio 2.9-inch black/white SPI e-paper. The selected driver class is `GxEPD2_290_GDEY029T94`; verify the controller/revision printed on the display before wiring.
- LCD1602 I²C address `0x27`; change the address in `src/main.cpp` if an I²C scan reports another address.
- DS3231 on GPIO21/22.
- E-paper and I²C pins follow `HARDWARE_DESIGN.md`.
- The buzzer is switched by a transistor from GPIO17.

## Buttons and display pages

- MODE cycles Calendar, Notes, Pomodoro, and Google-account setup.
- Tap MODE to change page. MODE always changes the current screen; there is no sleep gesture.
- Calendar shows Monday through Sunday. UP/DOWN selects a weekday; OK opens that day's events and notes. In Notes, UP/DOWN switches events, title and description alternate every four seconds, and OK returns to the week view. Fifteen seconds idle also returns to the week view.
- Pomodoro setup uses UP/DOWN to adjust cycles (1–12), work duration (10–120 minutes in 10-minute steps), and break duration (5–60 minutes in 5-minute steps). OK confirms each setting and starts the timer after the third confirmation. Work/break timing continues while MODE switches screens.
- At a work-to-break transition the buzzer sounds twice for about one second per pulse; when all cycles finish it sounds for about three seconds and returns to Pomodoro setup.
- On Account, OK opens the Wi-Fi setup access point. MODE changes screens in every mode.
- Wire MODE/UP/DOWN from GPIO32/33/19 to GND. Wire OK from GPIO35 to GND and add an external 10 kΩ pull-up to 3.3 V because GPIO35 has no internal pull-up.
- The Account page draws a QR linking to `WEB_APP_URL?device_id=...`. Set `WEB_APP_URL` in local `src/config.h` to the deployed Desk Buddy web app. Keep the base URL short (no query string) so it fits the version-8 QR. The QR contains only the public web URL and device ID; it does not contain Google credentials or tokens.

## Build setup

1. Install PlatformIO and open `firmware/ESP32` as the project.
2. Copy `src/config.example.h` to `src/config.h`.
3. Set the deployed HTTPS Worker URL, `DB001`, and the matching device token configured in the Worker secret `DEVICE_API_TOKENS`.
4. Set `WEB_APP_URL` to the deployed HTTPS web app and put the root CA PEM that validates the Worker URL in `API_ROOT_CA_PEM`. TLS certificate validation stays enabled. The web app's `VITE_API_BASE_URL` must point to that same deployed Worker.
5. Connect the hardware and upload the `nodemcu-32s` environment.

## Configure Wi-Fi on the device

- On first boot, or when the saved network cannot connect, the ESP32 starts a temporary password-protected access point. The e-paper shows its network name, one-time password, and `192.168.4.1`.
- Connect a phone to that access point, open `http://192.168.4.1`, select a scanned network, and enter its password. The ESP32 stores credentials in NVS and restarts to connect.
- To change a saved network, open the Account screen and press OK. MODE remains the screen-switching button in every mode.
- Wi-Fi credentials are stored locally in ESP32 Preferences/NVS; they are no longer compiled into `config.h`.
- The PlatformIO environment uses the project's 4 MB partition table with two approximately 1.94 MB app slots so the firmware and future OTA-capable layout fit.

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
- Clock, calendar display, and reminders are fixed to Vietnam time (`Asia/Ho_Chi_Minh`, UTC+7). The POSIX TZ string `ICT-7` represents UTC+7.
- Calendar week navigation, alternating event/note display, Pomodoro cycle timing, buzzer timing, and offline behavior need Wokwi/physical verification. A successful compile alone does not validate these interactions.

## Test without hardware

From this folder, run `pio test -e native` to test RFC3339 event-time conversion, reminder timing, and button navigation on the computer. This does not need an ESP32 or connected peripherals. To compile the full firmware without uploading it, run `pio run -e nodemcu-32s` after creating `src/config.h` from the example. The native test does not validate QR scanning, display drivers, GPIO wiring, TLS connectivity, or real RTC behavior.
