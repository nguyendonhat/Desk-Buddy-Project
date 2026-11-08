# Desk Buddy — hardware prototype design

## Confirmed hardware and remaining revision detail

- Controller: NodeMCU-32S (classic ESP32-WROOM-32).
- Large display: WeAct Studio 2.9-inch monochrome e-paper over SPI. The family is commonly 128×296 pixels, but the board revision/controller marking still needs checking before choosing the exact firmware driver class.
- Small display: 16×2 character LCD with an I²C interface/backpack.
- RTC: DS3231 breakout with a suitable backup cell.
- Indicators and input: RGB LED, piezo buzzer, and four momentary push buttons (MODE, UP, DOWN, OK).
- The extra Arduino Nano boards are not needed for this prototype; the ESP32 has enough GPIO and handles Wi-Fi plus local timers.

This pin map is for the classic ESP-WROOM-32 DevKit and SPI e-paper. It is not for an ESP32-S3 or WROVER board.

## Display and interaction roles

- E-paper: show today's events and the next event. Refresh only when the data changes or every few minutes; do not animate a seconds clock on e-paper.
- 16×2 LCD: show the live clock on row 1 and Pomodoro state/countdown on row 2.
- DS3231: keep time while USB power is removed. Its coin cell powers the RTC only, not the ESP32 or displays.
- Buttons: tap MODE to cycle Calendar, Notes, Pomodoro, and Account; hold MODE for 5 seconds for deep sleep; UP/DOWN scroll events and note pages or adjust Pomodoro duration; OK opens a note or starts/pauses Pomodoro.
- RGB LED and buzzer: indicate focus, break, upcoming event, and timer completion.

## Suggested ESP32-WROOM-32 DevKit pin map

| Module signal | ESP32 pin | Notes |
|---|---:|---|
| E-paper DIN / MOSI | GPIO23 | SPI data from ESP32 |
| E-paper CLK / SCK | GPIO18 | SPI clock |
| E-paper CS | GPIO27 | Chip select |
| E-paper DC | GPIO26 | Data/command |
| E-paper RST | GPIO25 | Reset |
| E-paper BUSY | GPIO34 | Input only; appropriate for BUSY. Do not rely on an internal pull-up on GPIO34. |
| I²C SDA | GPIO21 | Shared by DS3231 and the low-voltage side of the LCD level shifter |
| I²C SCL | GPIO22 | Shared by DS3231 and the low-voltage side of the LCD level shifter |
| MODE button | GPIO32 | Button connects pin to GND; configure `INPUT_PULLUP` |
| UP button | GPIO33 | Button connects pin to GND; configure `INPUT_PULLUP` |
| DOWN button | GPIO19 | Button connects pin to GND; configure `INPUT_PULLUP` |
| OK button | GPIO35 | Input-only pin: add an external 10 kΩ pull-up to 3.3 V; button connects pin to GND |
| RGB red | GPIO13 | One current-limiting resistor per LED color |
| RGB green | GPIO14 | One current-limiting resistor per LED color |
| RGB blue | GPIO16 | For ESP-WROOM-32; avoid GPIO16/17 on boards where PSRAM uses them |
| Buzzer driver | GPIO17 | Drive a transistor/MOSFET, not the buzzer directly from the GPIO |

The e-paper and RTC run from 3.3 V. Grounds must be common. E-paper modules vary in their header labels and power requirements; follow the exact module's pin labels and datasheet.

## I²C voltage note for the 16×2 LCD

Many LCD backpacks are powered at 5 V and pull SDA/SCL up to 5 V, which must not reach ESP32 GPIO pins. Power the LCD backpack from 5 V and put a bidirectional I²C level shifter between the LCD and the 3.3 V I²C bus. Keep the DS3231 on the ESP32's 3.3 V side. If the exact backpack is documented to accept 3.3 V logic and is powered at 3.3 V, a level shifter may not be needed; check its board schematic first. The common LCD I²C addresses are 0x27 and 0x3F, but scan the actual module.

## Power and protection

- Power the DevKit through its USB connector from a stable 5 V supply (use a 5 V, 2 A USB supply for comfortable margin with Wi-Fi and the LCD backlight).
- Take 3.3 V for the e-paper and DS3231 from the DevKit's regulated 3V3 pin, subject to the exact board/module current requirements.
- Power the 16×2 LCD/backlight from 5 V, with the I²C level shifter described above.
- Use a CR2032 in the DS3231 holder only if the RTC board is designed for that cell. Do not charge a CR2032. Some DS3231 clone boards have charging circuitry intended for LIR2032 cells; inspect the board and remove/disable charging circuitry before using a non-rechargeable CR2032.
- Use a 220–330 Ω resistor in series with each discrete RGB LED color. Confirm whether the LED is common-anode or common-cathode and set the output polarity accordingly.
- Use a small NPN transistor or logic-level MOSFET to switch the buzzer. A piezo element can be driven with a suitable transistor stage; an electromagnetic buzzer also needs a flyback diode. Verify the buzzer's rated voltage and current.
- Never connect 5 V to an ESP32 GPIO. Never power the whole device from the RTC coin cell.

## Wiring overview

```text
USB 5 V ──> ESP32 DevKit ── 3V3 ──> SPI e-paper
                  │        └──────> DS3231 VCC
                  │
                  ├── GPIO18/23/27/26/25/34 ──> e-paper SPI/control
                  ├── GPIO21/22 ──> DS3231 I²C + level shifter low side
                  │                         level shifter high side ──> LCD backpack SDA/SCL
                  ├── 5 V ────────────────────────────────────────────> LCD VCC/backlight
                  ├── GPIO32/33/19 ──> MODE/UP/DOWN buttons to GND
                  ├── GPIO35 ──> OK button to GND (external 10 kΩ pull-up to 3.3 V)
                  ├── GPIO13/14/16 ──> RGB LED through resistors
                  └── GPIO17 ──> transistor ──> buzzer

All module grounds, transistor emitter/source, and ESP32 GND are common.
CR2032 ──> DS3231 VBAT holder (RTC backup only)
```

## Bring-up sequence

1. Check the exact e-paper revision/controller marking on the rear label so the firmware uses the matching driver.
2. Wire only the e-paper at 3.3 V and confirm its driver with a basic test screen.
3. Add the LCD through the I²C level shifter; scan for the address and test both rows.
4. Add DS3231 and verify time persists after disconnecting USB (the ESP32 itself will turn off).
5. Add buttons, RGB LED, and buzzer one at a time.
6. Integrate the Calendar client after the device token is provisioned locally. Do not commit the token; a token embedded in prototype firmware can be extracted from the device.

## References

- WeAct Studio e-paper modules and example code: https://github.com/WeActStudio/WeActStudio.EpaperModule
- GxEPD2 supported 2.9-inch black/white panel variants: https://github.com/ZinggJM/GxEPD2
- ESP32-WROOM-32 electrical and GPIO specifications: https://documentation.espressif.com/esp32-wroom-32_datasheet_en.html
- DS3231 backup-battery timekeeping: https://www.analog.com/en/products/ds3231.html

