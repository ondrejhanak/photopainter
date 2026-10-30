# PhotoPainter

Firmware for the Waveshare PhotoPainter: a battery-powered 7.3" 7-color e-paper photo frame with an
RP2040 microcontroller. Copy photos onto its SD card and it shows a new one every few hours, switching
itself off in between.

This is a fork of the [official Waveshare firmware](https://github.com/waveshareteam/PhotoPainter),
reworked to be easier to use and to develop.

## What's improved over the original firmware

### Using the frame

- **Photos go straight onto the card.** Copy them to the card root under any name; the original only
  looked in a `/pic/` folder.
- **Natural or random order.** `Photo 2` comes before `Photo 10`; random order shows every photo once
  before reshuffling. Adding or removing photos never causes skips or repeats (the original kept a
  position in a list file, which shifted whenever photos changed).
- **Settings without rebuilding.** Interval and order live in `settings.ini` on the card, created with
  comments on first start; the original needed code changes and a rebuild.
- **Sensible default interval:** 12 hours instead of 15 minutes, which drained the battery quickly.
- **Only images are shown.** Only `.bmp` files (any letter case) are used; other files, folders,
  hidden and system files and dot-files (e.g. macOS `._*` companions, `.DS_Store`) are ignored. The
  original tried to display every file in `/pic/`.
- **A tidy card.** The frame's own state lives in a hidden folder; no list, index or temporary files.
- **Clear on-screen messages** for a missing card or missing images.
- **Better photo conversion.** `convert/convert.py` crops, enhances and dithers photos against a palette
  modelled on the panel's real ink colors.

### Developing

- **PlatformIO build.** No manual Pico SDK or ARM toolchain setup (the original used CMake with both
  installed by hand).
- **One-command flashing.** `pio run -t upload` reboots the board into BOOTSEL over USB, flashes and
  verifies; no button combination or dragging files.
- **Arduino-Pico `setup()`/`loop()` structure**, with `printf` logging over USB serial.
- **Documented hardware behavior**, e.g. the RTC alarm arriving on the KEY line and the missing SD
  card-detect pin (see [How it behaves](#how-it-behaves)).

### Bugs fixed

- **Frozen on a bad file.** A broken or unsupported image made the frame freeze and reboot on the same
  file over and over; now it shows an error screen with the file name and moves on.
- **Swapped card ignored.** On USB the card was checked only at boot and a swapped card was never
  re-initialized; now it is re-read before every image change.
- **Wrong sort length.** The image list was sorted with the file-name length instead of the number of
  files, so it was sorted only partly or past its end.
- **Buffer overflows** when reading file names (up to 999 bytes read into smaller buffers).
- **`u_int8_t` typo** in the SD driver, which broke builds on toolchains without BSD types (also sent
  [upstream](https://github.com/waveshareteam/PhotoPainter/pull/7)).

Ideas being considered [IDEAS.md](IDEAS.md).

## Photos & SD card

| On the card | Purpose |
|---|---|
| `*.bmp` in the root | Photos (subfolders, dot-files and non-`.bmp` files are ignored) |
| `settings.ini` | Settings, created with comments on first start |
| `.photopainter/` | The frame's own state (hidden) |

Photos are shown in natural name order or randomly; adding or removing photos never causes skips or
repeats.

### settings.ini

Edit in any text editor; changes apply with the next image change (scheduled, or press KEY).
Unknown keys are ignored and invalid values fall back to the default.

```ini
; Hours between image changes (1 to 47)
interval_hours = 12

; Image order: alphabetical or random (random shows every image once before reshuffling)
order = alphabetical
```

### Preparing photos

Images must be 800x480 (or 480x800) 24-bit BMPs that use only the panel's 7 colors: black
`(0,0,0)`, white `(255,255,255)`, green `(0,255,0)`, blue `(0,0,255)`, red `(255,0,0)`,
yellow `(255,255,0)`, orange `(255,128,0)`.

`convert/convert.py` (needs [Pillow](https://pypi.org/project/pillow/)) produces exactly that: it
crops a photo to fill the frame, enhances it slightly and dithers it to the palette, saving
`<name>_output.bmp`. `convert/convert_all.sh` converts all JPEGs in the current folder.

```bash
python3 convert/convert.py photo.jpg                  # orientation from the aspect ratio
python3 convert/convert.py photo.jpg --dir portrait   # or force landscape/portrait
```

## Hardware

Product details and schematic: [PhotoPainter wiki](https://www.waveshare.com/wiki/PhotoPainter).

| Component | Interface | Pins |
|-----------|-----------|------|
| E-ink display (800x480, 7-color) | SPI1 @ 8MHz | CLK=10, MOSI=11, CS=9, DC=8, RST=12, BUSY=13, PWR=16 |
| SD card (FAT32/exFAT) | SPI0 @ 12.5MHz | CLK=2, MOSI=3, MISO=4, CS=5 (no card-detect pin) |
| RTC (PCF85063) | I2C1 @ 100kHz | SDA=14, SCL=15, INT=6 (unused), addr=0x51 |
| Power / battery | GPIO, ADC | VBAT=29 (ADC), BAT_OFF=18, BAT_STATE=19 (KEY / RTC wake sense), CHARGE=17, VBUS=24 |
| LEDs | GPIO | ACT=25, PWR=26 |

### How it behaves

- **Power:** on battery the board keeps itself powered while working and cuts its own power with
  `BAT_OFF` (`powerOff()`) when done. It is fully off between images.
- **Wake-up:** the KEY button and the RTC alarm both feed the power-on circuit and both pull
  `BAT_STATE` low. On battery either one powers the board on; on USB the firmware sees either as a
  "KEY" event. The RTC `INT` pin (GPIO 6) never shows the alarm and is not used.
- **RTC:** used only as a countdown timer. On every run it is reset to a fixed reference time and the
  alarm is set `interval_hours` (settings.ini) later; the time is never read.
- **On USB:** the board stays awake. KEY or the RTC alarm shows the next image, the SD card is
  re-scanned each time (cards can be swapped), and unplugging USB powers the board off.
- **SD card:** with no card-detect pin, the firmware forces card re-initialization on every check.

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
pio run                      # build (.pio/build/pico/firmware.uf2)
pio run -t upload            # flash: reboots into BOOTSEL over USB, flashes, verifies, starts
pio run -t upload -t monitor # flash, then show the serial log
pio device monitor           # serial log (printf output)
```

If the board has no working USB serial (first flash, crashed firmware), enter BOOTSEL by hand (hold
**BOOT**, press and release **RUN**, release **BOOT**), then run the upload again or drag the `.uf2`
onto the `RPI-RP2` drive. Extra library debug output: add `-D DEBUG=1` to `build_flags`.

### Architecture

```
platformio.ini                  Build configuration (flags, include paths, ignored framework libs)
src/
├── main.c                      Entry point, power management, main loop, configuration
├── display.c                   Image and message screens on the panel
├── sdcard.c                    SD card mount and detection
├── images.c                    Next-image selection, frame state
├── image_order.c               Natural and random image order (no hardware access)
└── settings.c                  settings.ini loading (via inih)
include/                        Headers for src/, one per module
lib/
├── Config/DEV_Config.h         Pin definitions, SPI/I2C/GPIO init
├── e-Paper/EPD_7in3f.c         Display driver (init, display, sleep, clear)
├── GUI/
│   ├── GUI_Paint.c             Drawing primitives, text rendering
│   └── GUI_BMPfile.c           BMP parsing, RGB→7-color mapping
├── RTC/waveshare_PCF85063.c    RTC driver, alarm scheduling
├── FatFs_SPI/                  SD card filesystem (FatFs library)
├── Fonts/                      ASCII fonts (16 and 20 px)
├── led/led.c                   LED indicators, power control
└── inih/                       INI parser (r62, BSD) for settings.ini
convert/                        Host-side image conversion script (Python)
```

### Program flow

`setup()` (runs once per power-on):

1. `DEV_Module_Init()` - GPIO, SPI, I2C, ADC setup
2. `watchdog_enable(8000)` - 8s watchdog
3. RTC reset to the reference time, alarm set 12 h later (safe default)
4. Battery check (min 3.1V) → low power shutdown if below
5. `scan_card()` - SD card detection and `settings.ini`; alarm re-armed with `interval_hours`
6. On battery: `show_next()` shows the next image, then `powerOff()`; the RTC alarm or KEY powers
   the board on again
7. On USB: enter charging mode and return

`loop()` (only in charging mode, every 200 ms): power off if USB is unplugged; on KEY or RTC alarm,
`scan_card()` then `show_next()`.
