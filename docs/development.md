# Development

Built with [PlatformIO](https://platformio.org) on the
[Arduino-Pico](https://github.com/earlephilhower/arduino-pico) core (which bundles the Pico SDK);
the toolchain and framework are downloaded on the first build. Install the **PlatformIO IDE** extension
for VS Code, or the CLI:

```bash
curl -fsSL -o get-platformio.py https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py
python3 get-platformio.py   # installs ~/.platformio/penv/bin/pio
```

```bash
pio run                      # build for the PhotoPainter (.pio/build/pico/firmware.uf2)
pio run -e pico2             # build for the PhotoPainter (B) (.pio/build/pico2/firmware.uf2)
pio run -t upload            # flash: reboots into BOOTSEL over USB, flashes, verifies, starts
pio run -t upload -t monitor # flash, then show the serial log
pio device monitor           # serial log (printf output)
```

Each model has its own env in `platformio.ini` (`pico`, `pico2`), which picks the board and the panel
(`include/panel.h`); `-e pico2` works with every command above. If the board has no working USB serial
(first flash, crashed firmware), enter BOOTSEL by hand (hold
**BOOT**, press and release **RUN**, release **BOOT**), then run the upload again or drag the `.uf2`
onto the `RPI-RP2` (or `RP2350`) drive. The PhotoPainter (B)'s factory firmware ignores the reboot over USB; without the buttons, `picotool reboot -u -f` puts it into BOOTSEL (the drive appears after about 15 s). Extra library debug output: add `-D DEBUG=1` to `build_flags`.

The firmware prints its version at boot: the nearest `v*` tag (`git describe`), e.g. `v1.0.0`, or
`v1.0.0-3-g725e442` between tags and `-dirty` with uncommitted changes (`tools/version.py`).

## Tests

`tools/jpeg_host/` (JPEG conversion) and `tools/images_host/` (photo selection, cache cleanup) run the
firmware's own C files on a computer, with sanitizers. Every new test is proven by mutation: the code
it guards is broken on purpose (a dropped call, a loosened check) and the test must fail; a test no
mutation fails is dropped. Code that only runs on the frame (screens, the skip loop) is checked the
same way by flashing a deliberately broken build and reading the serial log.

```bash
sh tools/images_host/build.sh                                      # photo selection, cache, wake log, settings
sh tools/jpeg_host/build.sh && python3 tools/jpeg_host/check.py [PHOTO_DIR] SCRATCH_DIR  # needs Pillow
sh tools/jpeg_host/fuzz.sh replay                                  # saved fuzzing inputs, once
sh tools/jpeg_host/fuzz.sh [SECONDS]                               # fuzzing, default 15 min; brew install llvm
```

**Fuzzing** (`tools/jpeg_host/fuzz.c`) runs damaged JPEGs through the frame's photo path (EXIF, decoder, scaling, dithering) on both panels and orientations, under ASan and UBSan; a pixel outside the frame, an incomplete frame or an impossible EXIF value stops it too. CI only replays the saved inputs: the seeds (`make_fuzz_seeds.py`), the corpus that reaches every code path found so far (`fuzz_corpus/`, refreshed with `fuzz.sh merge` after a run) and every fixed finding (`fuzz_findings/`). Fuzz after changing `exif.c`, `jpeg_convert.c` or TJpgDec; the reached paths level off after about 15 minutes. Known limit: a read past the end of data held inside the larger work memory goes unseen unless the target copies that data out (as it does for the EXIF segment), and narrow boundaries are luck, so they get unit tests.

Warnings are errors: in the host tests, and in the firmware for our own code in `src/`
(`build_src_flags`; warnings from the framework and `lib/` don't count).

The firmware build also fails when less than 4 KB of RAM is left for the heap ([ram_check.py](../tools/ram_check.py)). Static data takes nearly all of the PhotoPainter's RAM (the frame buffer alone 188 KB), and running short shows only on the frame: FatFs or a conversion fails and nothing is drawn. Measured on the PhotoPainter by recording every heap call: the peak is 1.3 KB (boot, photo changes, drive mode, broken and large JPEGs), and a build with only the 4 KB reserve free passed the same run.

Without `PHOTO_DIR`, `check.py` replaces the photos with a generated picture and skips the look and
per-photo checks.

**Committed test photos** (`tools/jpeg_host/photos/`, 3.4 MB): real camera and phone files (Nikon, Canon, Olympus, iPhone 3GS, 7 and 16 Pro with Apple's HEIC export, one without EXIF, two rotated to 4:4:0 that the frame rejects), made by `make_test_photos.py` from originals kept outside the repo. It crops them losslessly with jpegtran, keeping each encoder's tables, sampling and restart interval, sets the orientation tag for the upright camera cases and zeroes GPS, MakerNote and serial numbers, then checks all of that. `check.py` converts each for both frames and panels and compares the output with `photos/hashes.txt`: the conversion is integer-only, so a different hash is a code change. Frames keep converted photos by cache key, so changed output needs a `CACHE_VERSION` bump in `src/cache_format.c` (a palette change renews caches by itself); after an intended change, `check.py --update-hashes` rewrites the file. GitHub Actions ([ci.yml](../.github/workflows/ci.yml)) builds both firmwares and runs
both suites that way on every push; a `v*` tag builds both `.uf2` files into a draft release
([release.yml](../.github/workflows/release.yml)).

## Architecture

```
platformio.ini                  Build configuration (flags, include paths, ignored framework libs)
src/
├── main.c                      Entry point, power management, main loop, configuration
├── display.c                   Image and message screens on the panel
├── jpeg_convert.c              JPEG decode (TJpgDec), scale, crop, dither (no hardware access)
├── exif.c                      EXIF orientation and date taken (no hardware access)
├── palette.c                   Panel colors as dithering sees them
├── cache.c, cache_format.c     Converted photos on the card (format: no hardware access)
├── work_memory.c               48 KB shared by USB drive mode and JPEG conversion
├── sdcard.c                    SD card mount and detection
├── images.c                    Next-image selection, frame state
├── image_order.c               Natural and random image order (no hardware access)
├── messages.c                  On-screen texts per language
├── settings.c                  settings.ini loading (via inih)
├── saved_language.cpp          Last language in CPU flash (for screens without a readable card)
├── usb_drive.cpp               SD card as a USB drive (mass storage next to USB serial)
├── usb_serial.cpp              USB serial input for C code (test commands)
└── wake_log.c                  Wake log on the card (docs/wake-log.md)
include/                        Headers for src/, one per module
include/panel.h                 The build's panel (7- or 6-color), chosen by the env in platformio.ini
lib/
├── Config/DEV_Config.h         Pin definitions, SPI/I2C/GPIO init
├── e-Paper/EPD_7in3f.c         7-color display driver (init, display, sleep, clear)
├── e-Paper/EPD_7in3e.c         6-color display driver (PhotoPainter (B))
├── GUI/GUI_Paint.c             Drawing primitives, text rendering
├── tjpgd/                      TJpgDec R0.03 JPEG decoder (ChaN)
├── RTC/waveshare_PCF85063.c    RTC driver, alarm scheduling
├── FatFs_SPI/                  SD card filesystem (FatFs library)
├── Fonts/                      Fonts from Terminus Font (22 and 28 px, U+0020-U+017F, SIL OFL 1.1)
├── led/led.c                   LED indicators, power control
└── inih/                       INI parser (r62, BSD) for settings.ini
tools/jpeg_host/                JPEG pipeline on the computer: harness, checks, reference converter, fuzzing
tools/images_host/              Photo selection tests on the computer (FatFs on a RAM disk)
tools/gen_font.py               Generates lib/Fonts/fontNN.c from a BDF bitmap font
tools/serial_log.py             Serial log with timestamps, sends test commands (device tests)
tools/drive_test.py             USB drive mode enter/eject cycles with write and verify (macOS)
tools/version.py                Build step: firmware version from git tags (firmware_version.h)
```

## Program flow

`setup()` (runs once per power-on):

1. `DEV_Module_Init()` - GPIO, SPI, I2C, ADC setup
2. `watchdog_enable(8000)` - 8s watchdog
3. RTC reset to the reference time, alarm set 12 h later (safe default)
4. Battery measured once (average of 8 ADC readings); below 3.1 V → shutdown without a refresh
5. `scan_card()` - SD card detection and `settings.ini`; alarm re-armed with `interval_hours`
6. On battery: `show_next()` shows the next image (battery icon below 3.5 V), then `powerOff()`; the
   RTC alarm or KEY powers the board on again. Below 3.3 V the "Battery Empty" screen instead, and
   the RTC alarm is turned off until the frame is charged
7. On USB: enter charging mode and return

`loop()` (only in charging mode, every 50 ms): power off if USB is unplugged; on KEY or RTC alarm,
`scan_card()` then `show_next()`. In drive mode it serves the computer's reads and writes instead and
leaves the card alone (the RTC alarm only re-arms); eject leaves drive mode.
