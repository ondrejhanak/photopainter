# Claude Code Project Guidelines

## Git Workflow

**CRITICAL:** Do NOT commit changes to Git without explicit request from the user.

## Build Instructions

The project is built with PlatformIO (Arduino-Pico core on top of the Pico SDK). PlatformIO manages
its own ARM toolchain, so no `PICO_SDK_PATH` or system toolchain is needed.

`pio` is available on PATH (symlinked from `~/.platformio/penv/bin` into `~/.local/bin`):

```bash
pio run                 # build
pio run -t clean        # clean
pio run -t upload       # flash via picotool (auto-resets into BOOTSEL over USB serial)
pio device monitor      # USB serial
```

Output: `.pio/build/pico/firmware.uf2` (and `firmware.elf`).

### Binary Size Checking

Flash/RAM usage is printed at the end of every build (`-Wl,--print-memory-usage` in `platformio.ini`).
For per-section detail:

```bash
~/.platformio/packages/toolchain-rp2040-earlephilhower/bin/arm-none-eabi-size .pio/build/pico/firmware.elf
```

## Device Testing

The frame is usually connected over USB, so flashing and logs need no button presses.

- **Flash only between panel refreshes** (~35-40 s each). A reset mid-refresh leaves the panel black
  (one KEY press fixes it). If unsure, wait for `Goto Sleep...` in the log before `pio run -t upload`.
- **Serial log without the user:** run `~/.platformio/penv/bin/python tools/serial_log.py` in the
  background (from a scratch directory; writes `serial.log`, sends what is written to `cmd.txt`),
  then grep the log. It reopens the port after flashing or re-enumeration; stop it before flashing
  (picotool needs the port). The first character of a line is sometimes lost after idle gaps.
- **Image change without the user:** `printf n > cmd.txt` instead of a KEY press; input sent during a
  refresh is discarded, so wait for `Goto Sleep...` before the next `n`. The KEY/RTC path itself
  still needs the real button.
- **Drive mode:** `python3 tools/drive_test.py` runs `d` / mount / eject cycles against the logger
  (`-n` cycles, `-w MB` write and verify after remount, `-x` command on the mounted card, `-v` volume
  name) and stops at a reboot. The log loses nothing around enter/leave (no re-enumeration).
- **Test screens without card slot changes:** in drive mode, change the card from the Mac (move images
  into a folder, add a broken `.bmp`, edit `settings.ini`), eject, and look at the screen. For
  screens that need the card out, temporarily force them in `show_next()` instead; never commit that.
- **Finding hangs and reboots (watchdog):** write step markers to `watchdog_hw->scratch[0..3]` (they
  survive the reset; the SDK uses 4-7, `watchdog_enable()` overwrites 4) and print them at boot. A
  temporary HardFault handler storing the stacked PC, or `-Wl,--wrap=panic` storing the message
  pointer, names the crash site via `arm-none-eabi-addr2line` on the saved `firmware.elf`.
- **Test timer behavior fast:** temporarily set the alarm to seconds (e.g. `alarmTime.seconds += 40`
  in `armAlarm()`), flash, record, then restore and reflash. Never commit the temporary change.
- **Logic without hardware:** compile `src/` files on the Mac against small stub headers (FatFs,
  hw_config, pico) to test parsing, ordering and edge cases before flashing.
- KEY presses are only seen between refreshes; the RTC alarm also arrives on the KEY line (`BAT_STATE`).
- The SD card slot is fiddly; prefer tests that don't need card changes.

## Project Architecture

See README.md for architecture overview and hardware behavior (power, wake-up, RTC, USB mode).

## Optimization Focus

Focus optimizations on **firmware-specific code** (`src/`, `include/`) rather than library code (`lib/`)
which may be reused or referenced elsewhere.

## Key Files

- **src/main.c** - Main application logic (mode, refresh interval, power flow), safe to modify
- **src/display.c, src/sdcard.c, src/images.c, src/image_order.c** - Display screens, SD card access, next-image selection and its order (pure logic), safe to modify
- **src/settings.c** - `settings.ini` on the SD card (user settings, parsed with inih in `lib/inih/`)
- **src/messages.c** - All user-visible texts per language (`msg(id)`, `language` setting). Every new
  on-screen text goes here in English and Czech (full diacritics, UTF-8), titles without a full stop, other lines are sentences with one; line
  lengths in the file's comment
- **include/** - Headers for the `src/` files
- **lib/** - Generic reusable libraries (display driver, GUI/BMP decoding, FatFs, RTC), modify with caution
- **tools/gen_font.py** - Generates the font tables in `lib/Fonts/` from Terminus BDF files (don't edit them by hand)
- **tools/serial_log.py, tools/drive_test.py** - Host-side device test tools (serial log + commands, drive mode cycles)
- **convert/** - Host-side Python script to resize/crop photos and dither them to the exact 7-color palette (24-bit BMP) for the SD card
- **platformio.ini** - Build flags, include paths, ignored framework libraries
