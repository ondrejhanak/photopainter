# PhotoPainter

Fork of the [official Waveshare PhotoPainter firmware](https://github.com/waveshareteam/PhotoPainter).

C firmware for Waveshare 7.3" 7-color e-ink photo frame with RP2040 microcontroller.

![waveshare_logo.png](waveshare_logo.png)

---

## Features

- 7.3" e-ink display with 7 colors (800x480 resolution)
- Battery-powered with USB-C charging
- SD card storage for BMP images
- RTC-based scheduled refresh (15min to 24hr intervals)
- Images copied straight to the SD card root, shown in natural name order (`Photo 2` before `Photo 10`) or randomly

## Hardware

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

## Development Environment Setup

The firmware is built with [PlatformIO](https://platformio.org) using the
[Arduino-Pico](https://github.com/earlephilhower/arduino-pico) core (which bundles the Pico SDK).
PlatformIO downloads the ARM toolchain and framework automatically on first build.

Install either the **PlatformIO IDE** extension for VS Code (recommended in `.vscode/extensions.json`)
or the CLI:

```bash
curl -fsSL -o get-platformio.py https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py
python3 get-platformio.py
```

The CLI is installed to `~/.platformio/penv/bin/pio` (add it to `PATH` or use the full path).

## Configuration

### settings.ini (on the SD card)

User settings live in `settings.ini` in the SD card root. If it is missing, the frame creates a
commented default on first start. Edit it in any text editor; changes apply on the next start, or on
the next KEY press while connected to USB.

```ini
; Hours between image changes (1 to 47)
interval_hours = 12

; Image order: alphabetical or random (random shows every image once before reshuffling)
order = alphabetical
```

Unknown keys are ignored and invalid values fall back to the default (both are logged over USB
serial). The 1–47 hour limit comes from the RTC library, which carries hour overflow into the next
day only once.

## Build

```bash
pio run
```

Output: `.pio/build/pico/firmware.uf2`. Flash and RAM usage are printed at the end of the build.

Clean build: `pio run -t clean`

## Flashing

With the device connected over USB and running this firmware:

```bash
pio run -t upload
```

`picotool` reboots the board into BOOTSEL mode (1200-baud touch on its USB serial port),
flashes and verifies the firmware, and starts it. Flash and open the serial monitor in one go
with `pio run -t upload -t monitor`.

If the board has no working USB serial (first flash, or firmware that crashed), enter BOOTSEL
manually and then run the same command, or drag `.pio/build/pico/firmware.uf2` to the `RPI-RP2` drive:

1. Hold **BOOT** button
2. Press **RUN** button
3. Release **RUN**
4. Release **BOOT**

## Architecture

```
platformio.ini                  Build configuration (flags, include paths, ignored framework libs)
src/
├── main.c                      Entry point, power management, main loop, configuration
├── EPD_7in3f_test.c            Display functions, BMP rendering
├── run_File.c                  SD card, next-image selection, frame state
└── settings.c                  settings.ini loading (via inih)
include/                        Headers for src/ (EPD_Test.h, run_File.h, settings.h)
lib/
├── Config/DEV_Config.h         Pin definitions, SPI/I2C/GPIO init
├── e-Paper/EPD_7in3f.c         Display driver (init, display, sleep, clear)
├── GUI/
│   ├── GUI_Paint.c             Drawing primitives, text rendering
│   └── GUI_BMPfile.c           BMP parsing, RGB→7-color mapping
├── RTC/waveshare_PCF85063.c    RTC driver, alarm scheduling
├── FatFs_SPI/                  SD card filesystem (FatFs library)
├── Fonts/                      ASCII (8-24pt) and Chinese fonts
├── led/led.c                   LED indicators, power control
└── inih/                       INI parser (r62, BSD) for settings.ini
convert/                        Host-side image conversion script (Python)
```

## 7-Color Palette

| Index | Color |
|-------|-------|
| 0x0 | Black |
| 0x1 | White |
| 0x2 | Green |
| 0x3 | Blue |
| 0x4 | Red |
| 0x5 | Yellow |
| 0x6 | Orange |
| 0x7 | Clean (avoid - causes artifacts) |

## Image Requirements

- Format: 24-bit BMP using **only these exact RGB values** — any other value is not recognized
  and the pixel repeats the previous pixel's color:
  black `(0,0,0)`, white `(255,255,255)`, green `(0,255,0)`, blue `(0,0,255)`, red `(255,0,0)`,
  yellow `(255,255,0)`, orange `(255,128,0)`
- Resolution: 800x480 or 480x800
- Location: SD card root (subfolders are ignored)

### Image Preparation

`convert/convert.py` (requires [Pillow](https://pypi.org/project/pillow/)) resizes a photo to cover the
frame, crops it from the center, applies light sharpening / contrast / saturation, dithers it
(Floyd–Steinberg) to the exact display palette, and saves `<name>_output.bmp`:

```bash
python3 convert/convert.py photo.jpg                  # orientation picked from aspect ratio
python3 convert/convert.py photo.jpg --dir portrait   # force landscape/portrait
```

`convert/convert_all.sh` converts every `*.jpg` / `*.jpeg` in the current directory
(run it from the folder with your photos, with `convert.py` alongside).

## Program Flow

`setup()` (runs once per power-on):

1. `DEV_Module_Init()` - GPIO, SPI, I2C, ADC setup
2. `watchdog_enable(8000)` - 8s watchdog
3. RTC reset to the reference time, alarm set 12 h later (safe default)
4. Battery check (min 3.1V) → low power shutdown if below
5. `scanCard()` - SD card detection and `settings.ini`; alarm re-armed with `interval_hours`
6. On battery: `run_display()` shows the next image, then `powerOff()`; the RTC alarm or KEY powers
   the board on again
7. On USB: enter charging mode and return

`loop()` (only in charging mode, every 200 ms): power off if USB is unplugged; on KEY or RTC alarm,
`scanCard()` then `run_display()`.

## Key Functions

| Function | File | Purpose |
|----------|------|---------|
| `EPD_7IN3F_Init()` | EPD_7in3f.c | Initialize display |
| `EPD_7IN3F_Display()` | EPD_7in3f.c | Send buffer to display |
| `EPD_7IN3F_Sleep()` | EPD_7in3f.c | Enter low power mode |
| `GUI_ReadBmp_RGB_7Color()` | GUI_BMPfile.c | Load BMP, map exact RGB values to 7-color indices |
| `selectNextImage()` | run_File.c | Pick the image after the last shown one (natural order, wraps) |
| `saveShownImage()` | run_File.c | Remember the shown image in the state file |
| `rtcRunAlarm()` | waveshare_PCF85063.c | Set wake alarm |
| `powerOff()` | led.c | Shutdown (BAT_OFF=LOW) |

## SD Card Files

| File | Purpose |
|------|---------|
| `/*.bmp` | Images, in the card root |
| `/settings.ini` | User settings (created with defaults if missing) |
| `/.photopainter/state.txt` | Last shown image name and random-order seed; the folder is hidden (dot name + FAT hidden attribute) |

The next image is the one after the last shown name in natural order, so adding or removing images
never causes skips or repeats. Random order works the same way on a pseudo-random key per name
(hash of the name and a seed stored in `state.txt`): every image is shown once per round, then a new
seed reshuffles (never starting the new round with the image just shown). Dot-files (e.g. macOS `._*` companions), hidden/system files and
folders are ignored.

## Debugging

Enable debug output by adding to `build_flags` in `platformio.ini`:
```ini
    -D DEBUG=1
```

`printf()` output goes to USB serial via `-D DEBUG_RP2040_PORT=Serial` in `platformio.ini`. View it with:
```bash
pio device monitor
```

## Documentation

- **Product wiki**: [PhotoPainter Wiki](https://www.waveshare.com/wiki/PhotoPainter)
- **PlatformIO**: [docs.platformio.org](https://docs.platformio.org)
- **Arduino-Pico core**: [earlephilhower/arduino-pico](https://github.com/earlephilhower/arduino-pico)
- **Original repository**: [waveshareteam/PhotoPainter](https://github.com/waveshareteam/PhotoPainter)

## License

Open-sourced by Waveshare Electronics. Bug reports and contributions welcome.
