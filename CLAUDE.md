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
  (picotool needs the port).
- **Image change without the user:** `printf n > cmd.txt` instead of a KEY press; input sent during a
  refresh is discarded, so wait for `Goto Sleep...` before the next `n`. The KEY/RTC path itself
  still needs the real button.
- **Drive mode:** `python3 tools/drive_test.py` runs `d` / mount / eject cycles against the logger
  (`-n` cycles, `-w MB` write and verify after remount, `-x` command on the mounted card, `-v` volume
  name) and stops at a reboot. The log loses nothing around enter/leave (no re-enumeration).
  The Mac must be unlocked: with the screen locked it does not mount the drive (looks like drive mode
  failing; the frame gives up after 60 s).
- **Test screens without card slot changes:** in drive mode, change the card from the Mac (move images
  into a folder, add a broken `.jpg`, edit `settings.ini`), eject, and look at the screen. For
  screens that need the card out, temporarily force them in `show_next()` instead; never commit that.
- **Finding hangs and reboots (watchdog):** write step markers to `watchdog_hw->scratch[0..3]` (they
  survive the reset; the SDK uses 4-7, `watchdog_enable()` overwrites 4) and print them at boot. A
  temporary HardFault handler storing the stacked PC, or `-Wl,--wrap=panic` storing the message
  pointer, names the crash site via `arm-none-eabi-addr2line` on the saved `firmware.elf`.
- **Both frames connected:** give every tool its port (`serial_log.py --port`, `pio run -t upload
  --upload-port`). `pio`'s upload fails ("requires a single RP2040 device") once the other frame is
  also visible to picotool; then flash with `picotool load -x --ser <serial> firmware.uf2` (serials
  from `pio device list`).
- **Test timer behavior fast:** temporarily set the alarm to seconds (e.g. `alarm_time.seconds += 40`
  in `arm_alarm()`), flash, record, then restore and reflash. Never commit the temporary change.
- **Saved language** (last flash sector): in BOOTSEL, `picotool save -r 0x101ff000 0x101ff010 out.bin`
  on the PhotoPainter, `0x10fff000 0x10fff010` on the PhotoPainter (B).
- **Logic without hardware:** compile `src/` files on the computer against small stub headers (FatFs,
  hw_config, pico) to test parsing, ordering and edge cases before flashing.
- KEY presses are only seen between refreshes; the RTC alarm also arrives on the KEY line (`BAT_STATE`).
- The SD card slot is fiddly; prefer tests that don't need card changes.

## Project Architecture

See docs/development.md for the architecture and program flow, docs/hardware.md for hardware behavior
(power, wake-up, RTC, USB mode), docs/changes.md for what differs from the original firmware.

## Conventions

- **Own code** (`src/`, `include/`): snake_case functions and variables; public functions prefixed by their module (`display_*`, `images_*`, `sdcard_*`, `settings_*`), static helpers without prefix; types PascalCase; macros and enum values UPPER_SNAKE; one module is `name.c` + `name.h`.
- **Upstream libraries** (ChaN's FatFs core, inih, carlk3's FatFs_SPI, TJpgDec) stay as released apart from their configuration headers; only whole unused files are removed.
- **Waveshare board code** (`lib/GUI`, `e-Paper`, `Config`, `RTC`, `led`, `Fonts`) is maintained as our own but keeps its upstream style and generic API (e.g. `EPD_7IN3F_Clear`, `DEV_SPI_Write_nByte`, `PCF85063_get_alarm_flag`).
- **Dead code** shows with `-Wl,--print-gc-sections`; remove it in its own commit, with a smoke test on a frame.

## Optimization Focus

Focus optimizations on **firmware-specific code** (`src/`, `include/`) rather than library code (`lib/`)
which may be reused or referenced elsewhere.

## Key Files

- **src/main.c** - Main application logic (mode, refresh interval, power flow), safe to modify
- **src/display.c, src/sdcard.c, src/images.c, src/image_order.c** - Display screens, SD card access, next-image selection and its order (pure logic), safe to modify
- **src/jpeg_convert.c, src/exif.c, src/palette.c, src/cache_format.c** - JPEG decode, scale and dither, EXIF orientation and date, panel colours, cache file format (pure logic, also built on the computer by `tools/jpeg_host/`); **src/cache.c** - cache files on the card
- **src/settings.c** - `settings.ini` on the SD card (user settings, parsed with inih in `lib/inih/`)
- **src/wake_log.c** - Wake log (`.photopainter/log.csv`, rotated to `log.old.csv`): steps timed with `wake_log_step()`, one line per event with `wake_log_write()`. Columns and codes are documented in **docs/wake-log.md**; a host test fails when its header differs from `WAKE_LOG_HEADER`, so update both together
- **src/messages.c** - All user-visible texts per language (`msg(id)`, `language` setting). Every new
  on-screen text goes here in English and Czech (full diacritics, UTF-8), titles without a full stop, other lines are sentences with one; line
  lengths in the file's comment
- **include/** - Headers for the `src/` files
- **lib/** - Generic reusable libraries (display driver, drawing, FatFs, RTC, TJpgDec in `lib/tjpgd/`), modify with caution
- **tools/gen_font.py** - Generates the font tables in `lib/Fonts/` from Terminus BDF files (don't edit them by hand)
- **tools/serial_log.py, tools/drive_test.py** - Host-side device test tools (serial log + commands, drive mode cycles)
- **tools/jpeg_host/** - The JPEG pipeline on a computer (macOS, Linux; also in CI): `build.sh`, then `check.py [PHOTO_DIR] SCRATCH_DIR` (edge cases, unit tests, look vs `convert_reference.py`; without PHOTO_DIR as in CI: generated picture, no look or per-photo checks); its output is byte-identical to the device's cache files
- **tools/images_host/** - Photo selection and settings tests on a computer (macOS, Linux; also in CI): `build.sh` builds and runs `images.c`, `cache.c` and `settings.c` on the real FatFs over a RAM disk (order, new photos first, cache cleanup, settings.ini parsing); the firmware log goes to `build/images_test.log`
- **tools/version.py** - Pre-build script: `FIRMWARE_VERSION` from `git describe` of the nearest `v*` tag (releases are tagged `vX.Y.Z`)
- **platformio.ini** - Build flags, include paths, ignored framework libraries
