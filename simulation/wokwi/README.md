# Desk Buddy Wokwi simulation

This is a safe, simulation-only sketch. It does not use Google OAuth, Wi-Fi credentials, a Worker URL, or a device token. It shows mock events, long-note paging, Pomodoro controls, the clock, and reminders on virtual hardware. It uses a simulated e-paper driver, so it cannot prove that the real WeAct panel revision or wiring works.

## Try it online in Wokwi

1. Open https://wokwi.com/projects/new/esp32.
2. Replace `diagram.json` with this folder's `diagram.json`.
3. Replace `sketch.ino` with this folder's `sketch.ino`.
4. Open `libraries.txt` and add the listed libraries in Wokwi's Library Manager if they are not picked up automatically.
5. Press the green Start Simulation button.
6. Click the virtual buttons next to the ESP32: MODE changes pages; UP/DOWN scroll the mock note or set Pomodoro duration; OK starts/pauses Pomodoro.

This online simulation tests the display layout and button flow only. It does not execute the production OAuth/Calendar network code, produce the Google-login QR, or verify real e-paper refresh behavior.

## Run it with Wokwi for VS Code

1. Open this `simulation/wokwi` folder in VS Code.
2. Install/enable the Wokwi extension if needed.
3. Build the PlatformIO environment `esp32dev`, then start Wokwi using the Wokwi toolbar / command.

## Run it on Wokwi.com

1. Open https://wokwi.com/ and create an ESP32 project.
2. Copy `sketch.ino`, `diagram.json`, and `libraries.txt` from this folder into the project.
3. Start the simulation. The LCD clock and e-paper mock calendar should appear; the buzzer sounds when the sample event enters the reminder window.

The RTC model in Wokwi is DS1307, used as a simulator substitute for the physical DS3231. The Wokwi e-paper model uses the GxEPD2_290_BS driver; the physical firmware retains the WeAct driver. The mock event is intentionally two minutes after the simulator's RTC start time. The simulation does not validate the exact WeAct panel revision, physical DS3231, Wi-Fi/TLS to the real Worker, or real-world wiring/voltage levels.
