# Software Examples (DS3231)

This folder contains practical examples for the DS3231 RTC module.

## C++ Examples

- `cpp_examples/rtc/rtc.ino`
    - Base example with serial menu, RTC read, and software alarm logic.
- `cpp_examples/clock/clock.ino`
    - RTC + OLED clock display.
- `cpp_examples/clock_haptic_alm/clock_haptic_alm.ino`
    - RTC + OLED + haptic alarm (DRV2605L).
- `cpp_examples/rtc_super_basic_demo/rtc_super_basic_demo.ino`
    - Super basic RTC validation demo (recommended first test).

## Fast Start: Super Basic Demo

1. Open `cpp_examples/rtc_super_basic_demo/rtc_super_basic_demo.ino`.
2. Build and upload.
3. Open Serial Monitor at `115200` baud.
4. Confirm you see `[RTC] YYYY-MM-DD HH:MM:SS` updating every second.

### Serial Commands

- `NOW?`
    - Prints the current RTC time once.
- `SET=YYYY-MM-DD HH:MM:SS`
    - Sets RTC date and time.
- `HELP`
    - Prints available commands.

## MicroPython Examples

- `mp/rtc_read.py`
- `mp/oled_rtc.py`