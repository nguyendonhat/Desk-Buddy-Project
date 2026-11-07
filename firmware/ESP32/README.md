# ESP32 Calendar firmware

This first firmware increment connects a NodeMCU-32S to Wi-Fi, reads the DS3231, fetches normalized Calendar events from the Worker, shows the clock on the LCD1602, and shows up to four events on the WeAct 2.9-inch monochrome e-paper. It also gives a short LED/buzzer reminder for an event starting within 15 minutes.

## Hardware assumptions

- NodeMCU-32S with ESP-WROOM-32.
- WeAct Studio 2.9-inch black/white SPI e-paper. The selected driver class is `GxEPD2_290_GDEY029T94`; verify the controller/revision printed on the display before wiring.
- LCD1602 I²C address `0x27`; change the address in `src/main.cpp` if an I²C scan reports another address.
- DS3231 on GPIO21/22.
- E-paper and I²C pins follow `HARDWARE_DESIGN.md`.
- The RGB LED is assumed common-cathode and the buzzer is switched by a transistor from GPIO17.

## Build setup

1. Install PlatformIO and open `firmware/ESP32` as the project.
2. Copy `src/config.example.h` to `src/config.h`.
3. Set the Wi-Fi name/password, deployed HTTPS Worker URL, `DB001`, and the matching device token configured in the Worker secret `DEVICE_API_TOKENS`.
4. Put the root CA PEM that validates the Worker URL in `API_ROOT_CA_PEM`. TLS certificate validation stays enabled.
5. Connect the hardware and upload the `nodemcu-32s` environment.

Do not commit `src/config.h`; it contains Wi-Fi and device credentials. The root `.gitignore` excludes it.

The Worker currently keeps OAuth connection state in memory for local development. If it restarts, reconnect Google in the Dashboard before testing the device.

## Current behavior and limits

- RTC time is synchronized from NTP when Wi-Fi is available and continues from DS3231 when Wi-Fi is lost.
- Calendar refresh runs every five minutes; the e-paper redraws only after a successful fetch.
- Calendar reminder time conversion assumes Vietnam UTC+7. Change `LOCAL_UTC_OFFSET_SECONDS` and the POSIX TZ string if using a different local zone.
- This increment does not yet implement Pomodoro controls, web device commands, event caching, or enclosure wiring validation.
