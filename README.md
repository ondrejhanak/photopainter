# PhotoPainter

Firmware for the Waveshare PhotoPainter and PhotoPainter (B), battery-powered 7.3" e-paper photo
frames: the PhotoPainter has a 7-color panel and an RP2040 microcontroller, the PhotoPainter (B) a
6-color panel and an RP2350. Copy photos onto the SD card and the frame shows a new one every few
hours, switching itself off in between.

Based on the official Waveshare firmware ([PhotoPainter](https://github.com/waveshareteam/PhotoPainter),
[PhotoPainter (B)](https://github.com/waveshareteam/PhotoPainter_B)), reworked to be easier to use and
to develop.

## What's improved over the original firmware

### Using the frame

- **Photos go straight onto the card.** Copy them to the card root under any name; the original only
  looked in a `/pic/` folder.
- **Random or natural order.** Random order (the default) shows every photo once before reshuffling;
  natural order puts `Photo 2` before `Photo 10`. Photos copied over USB are shown first, one per
  change (the first right after eject), then the order goes on where it was. Otherwise adding or
  removing photos never causes skips or repeats (the original kept a position in a list file, which
  shifted whenever photos changed).
- **Settings without rebuilding.** Interval and order live in `settings.ini` on the card, created with
  comments on first start; the original needed code changes and a rebuild.
- **Landscape or portrait.** `orientation = portrait` for a frame turned counter-clockwise: photos,
  screens, the battery icon and the note follow it. Photos that fit the frame's shape fill it, the
  others fit whole with bars.
- **Date the photo was taken.** `show_date = yes` puts it in the bottom-right corner (e.g. `24 Dec 1985`,
  in Czech `24. 12. 1985`), read from the camera's EXIF data; photos without one show none.
- **Sensible default interval:** 12 hours instead of 15 minutes, which drained the battery quickly.
- **Low battery shown on the photo.** A small battery icon in the top-right corner when the battery
  runs low; when it is empty, a "Battery Empty" screen asks for charging and the frame stops changing
  photos until it is charged. The original put a voltage message over the photo below 3.3 V and
  stopped without a word below 3.1 V.
- **Photos as they come from a phone or camera.** JPEGs are decoded, scaled and dithered on the
  frame itself, once; the result is kept in the frame's hidden folder, and the next photo is prepared
  while the screen refreshes. Rotated phone photos are turned upright. The original needed every
  photo converted on a computer to an exact-palette BMP.
- **Only photos are shown.** Only `.jpg` and `.jpeg` files (any letter case) are used; other files,
  folders, hidden and system files and dot-files (e.g. macOS `._*` companions, `.DS_Store`) are
  ignored. The original tried to display every file in `/pic/`.
- **Broken photos are skipped.** A JPEG the frame cannot show (progressive, CMYK, damaged) is passed
  over and the next photo is shown, with its name in a strip at the bottom; it is tried again in
  the next round.
- **No need to remove the card.** Connected to a computer, hold the button for 3 seconds: the card
  appears as a USB drive. Copy photos, then eject the drive; the frame shows the first new photo. The
  original needed the card taken out of its fiddly slot.
- **A tidy card.** The frame's own state lives in a hidden folder; no list, index or temporary files.
- **Clear on-screen messages** for a missing card or missing images.
- **Czech or English on-screen messages** (`language` in `settings.ini`), with accents, in larger
  fonts. The frame remembers the language, so even the "no card" message uses it.
- **Colors modelled on the panel's real inks**, so photos are dithered against what the panel can show.
  On the 6-color panel the inks are colorimeter measurements, and each photo's brightness is fitted
  to the range the panel can show, so highlights and shadows keep their detail.
- **One firmware for both models:** the same features on the PhotoPainter and the PhotoPainter (B),
  each with its own firmware file.

### Developing

- **PlatformIO build.** No manual Pico SDK or ARM toolchain setup (the original used CMake with both
  installed by hand).
- **One-command flashing.** `pio run -t upload` reboots the board into BOOTSEL over USB, flashes and
  verifies; no button combination or dragging files.
- **Arduino-Pico `setup()`/`loop()` structure**, with `printf` logging over USB serial.
- **Serial test commands:** on USB, sending `n` shows the next image like a KEY press and `d` enters
  USB drive mode like a long press (ignored in drive mode, eject leaves it), so tests can run without
  pressing the button.
- **JPEG pipeline on the computer:** `tools/jpeg_host/` builds the frame's conversion on a computer (macOS, Linux)
  and checks it on real photos and edge cases in seconds; its output is identical to the frame's.
- **Photo order tests on the computer:** `tools/images_host/build.sh` runs the photo selection (order,
  new photos first, cache cleanup) on the real FatFs over a RAM disk.
- **Documented hardware behavior**, e.g. the RTC alarm arriving on the KEY line and the missing SD
  card-detect pin (see [How it behaves](#how-it-behaves)).

### Bugs fixed

- **Frozen on a bad file.** A broken or unsupported image made the frame freeze and reboot on the same
  file over and over; now it is skipped and named at the bottom of the next photo.
- **Swapped card ignored.** On USB the card was checked only at boot and a swapped card was never
  re-initialized; now it is re-read before every image change.
- **Wrong sort length.** The image list was sorted with the file-name length instead of the number of
  files, so it was sorted only partly or past its end.
- **Garbled accented file names.** An image named e.g. `Dovolená.jpg` that could not be shown drew
  garbage instead of its name (bytes from past the font table); accented letters now draw as their
  base letter.
- **Restart after plugging in during a change.** USB connected while the frame was changing the photo
  on battery kept it running after it switched itself off, until the watchdog restarted it 8 s later;
  now it carries on as on USB.
- **Buffer overflows** when reading file names (up to 999 bytes read into smaller buffers).
- **RTC alarm flag never reported.** `PCF85063_get_alarm_flag()` tested `reg & 0x40 == 0x40`, which C
  reads as `reg & 1`, so a fired alarm looked like a KEY press. Needed to tell the two apart.
- **SD card never deselected.** The SD pin table set the MOSI, clock and chip-select fields twice, the
  second time to a drive-strength constant (0), so the driver toggled GPIO 0 while the real chip
  select stayed low. Worked by luck (the pins were already set up elsewhere), against the SD spec.
- **Red LED switched by the SD driver.** The driver drove the red LED as its card-activity light, so
  any LED state set by the firmware was switched off by the next card access.
- **Misaligned key lists.** The shared work memory, which also holds 64-bit photo keys, was only aligned
  for 32-bit values: undefined in C (found by the host tests' sanitizer), harmless on the RP2040, which
  moves 64-bit values as two 32-bit ones; now aligned for 64-bit values.
- **`u_int8_t` typo** in the SD driver, which broke builds on toolchains without BSD types (also sent
  [upstream](https://github.com/waveshareteam/PhotoPainter/pull/7)).

Ideas being considered [IDEAS.md](IDEAS.md).

## Installing

Download the file for your frame from [Releases](https://github.com/ondrejhanak/photopainter/releases):
`photopainter-7color-<version>.uf2` for the PhotoPainter, `photopainter-6color-<version>.uf2` for the
PhotoPainter (B). Hold **BOOT** while connecting the frame to a computer over USB (or hold **BOOT**,
press and release **RUN**, release **BOOT**): it appears as a drive named `RPI-RP2` (PhotoPainter) or
`RP2350` (PhotoPainter (B)). Copy the `.uf2` onto it; the frame restarts with the new firmware. A file
for the other model is refused. Photos, settings and converted photos on the SD card stay as they are.

## Photos & SD card

| On the card | Purpose |
|---|---|
| `*.jpg`, `*.jpeg` in the root | Photos (subfolders, dot-files and other files are ignored) |
| `settings.ini` | Settings, created with comments on first start |
| `.photopainter/` | The frame's own state and converted photos (hidden) |
| `.photopainter/log.csv` | Wake log: one line per photo change, drive session or battery event ([docs/wake-log.md](docs/wake-log.md)) |

Photos are shown in natural name order or randomly; photos copied over USB come first (see above),
otherwise adding or removing photos never causes skips or repeats.

### settings.ini

Edit in any text editor; changes apply after ejecting the USB drive or with the next image change
(scheduled, or press KEY).
Unknown keys are ignored and invalid values fall back to the default.

```ini
; Hours between image changes (1 to 47)
interval_hours = 12

; Image order: random or alphabetical (random shows every image once before reshuffling)
order = random

; Frame position: landscape, or portrait (turned counter-clockwise)
orientation = landscape

; Date the photo was taken (from the camera), at the bottom: no or yes
show_date = no

; Colour around photos that don't fill the screen: white or black
bars = white

; Language of on-screen messages: en (English) or cs (Czech)
language = en
```

### Photos

Copy JPEGs as they come; no preparation. A landscape photo fills the screen (cropped at the
edges); portrait and square photos are shown whole, with bars at the sides (`bars` in
`settings.ini`, to match the frame's mount). The first time a photo is shown it is converted (up
to about 40 s for a 24-megapixel photo), and usually that already happens while the previous photo
is on screen. Progressive JPEGs and CMYK JPEGs cannot be shown, nor HEIC (an iPhone's default; the
Photos app exports JPEG).

## Hardware

Product details and schematics: [PhotoPainter wiki](https://www.waveshare.com/wiki/PhotoPainter),
[PhotoPainter (B) wiki](https://www.waveshare.com/wiki/PhotoPainter_(B)). Both models use the same pins;
the (B) has an RP2350 with 16 MB flash and a 6-color panel (7.3" e-Paper (E)) instead of the 7-color
one (F).

| Component | Interface | Pins |
|-----------|-----------|------|
| E-ink display (800x480, 7 or 6 colors) | SPI1 @ 8MHz | CLK=10, MOSI=11, CS=9, DC=8, RST=12, BUSY=13, PWR=16 |
| SD card (FAT32/exFAT) | SPI0 @ 24MHz (18.75MHz on the (B)) | CLK=2, MOSI=3, MISO=4, CS=5 (no card-detect pin) |
| RTC (PCF85063) | I2C1 @ 100kHz | SDA=14, SCL=15, INT=6 (unused), addr=0x51 |
| Power / battery | GPIO, ADC | VBAT=29 (ADC), BAT_OFF=18, BAT_STATE=19 (KEY / RTC wake sense), CHARGE=17, VBUS=24 |
| LEDs | GPIO | ACT=25 (red), PWR=26 (green) |

### How it behaves

- **Power:** on battery the board keeps itself powered while working and cuts its own power with
  `BAT_OFF` (`powerOff()`) when done. It is fully off between images.
- **Wake-up:** the KEY button and the RTC alarm both feed the power-on circuit and both pull
  `BAT_STATE` low. On battery either one powers the board on; on USB the firmware sees either as a
  "KEY" event. The RTC `INT` pin (GPIO 6) never shows the alarm and is not used.
- **RTC:** used only as a countdown timer. On every run it is reset to a fixed reference time and the
  alarm is set `interval_hours` (settings.ini) later; the time is never read.
- **On USB:** the board stays awake. A short KEY press (released within 3 s) or the RTC alarm shows
  the next image, the SD card is re-scanned each time (cards can be swapped), and unplugging USB
  powers the board off. Holding KEY for 3 s is a long press; the red LED lights when it is reached.
  The RTC alarm holds the KEY line low until its flag is cleared, so the flag tells it apart from a
  held KEY.
- **USB drive mode:** on USB the frame is always a USB drive next to the serial port, empty like a
  card reader without a card (Windows may show an empty drive letter). A long press with a computer
  connected puts the SD card in: the computer mounts it within a second, while a "USB Drive" screen
  refreshes (flashing and the log keep working). The frame leaves the card alone until the drive is
  ejected or KEY is held again, then re-reads the card and `settings.ini` and shows the first new
  photo (or the next image). It also ends by itself when the computer does not read the drive
  within a minute (mount failed) or leaves it untouched for 30 minutes (forgotten eject,
  computer asleep), so the photos never stop for good. On a plain charger a long press is ignored
  and the red LED blinks 3 times. About 0.4 MB/s, limited by USB full speed with 64-byte packets.
- **LEDs:** green (PWR) is the charging light: on while the battery charges, off when it is full or
  on battery. Red (ACT) blinks 3 times at power-on, flickers while the SD card is in use (the SD
  driver reports activity, the firmware owns the LED), lights once a long KEY press is reached and
  stays on in USB drive mode.
- **SD card:** with no card-detect pin, the firmware forces card re-initialization on every check.
- **Refresh:** about 31 s on the PhotoPainter, about 20 s on the PhotoPainter (B); colder panels take
  longer.

## Development

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
onto the `RPI-RP2` (or `RP2350`) drive. Extra library debug output: add `-D DEBUG=1` to `build_flags`.

The firmware prints its version at boot: the nearest `v*` tag (`git describe`), e.g. `v1.0.0`, or
`v1.0.0-3-g725e442` between tags and `-dirty` with uncommitted changes (`tools/version.py`).

### Tests

`tools/jpeg_host/` (JPEG conversion) and `tools/images_host/` (photo selection, cache cleanup) run the
firmware's own C files on a computer, with sanitizers. Every new test is proven by mutation: the code
it guards is broken on purpose (a dropped call, a loosened check) and the test must fail; a test no
mutation fails is dropped. Code that only runs on the frame (screens, the skip loop) is checked the
same way by flashing a deliberately broken build and reading the serial log.

### Architecture

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
tools/jpeg_host/                JPEG pipeline on the computer: harness, checks, reference converter
tools/images_host/              Photo selection tests on the computer (FatFs on a RAM disk)
tools/gen_font.py               Generates lib/Fonts/fontNN.c from a BDF bitmap font
tools/serial_log.py             Serial log with timestamps, sends test commands (device tests)
tools/drive_test.py             USB drive mode enter/eject cycles with write and verify (macOS)
tools/version.py                Build step: firmware version from git tags (firmware_version.h)
```

### Program flow

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
