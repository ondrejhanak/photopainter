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
- Three file management modes (auto-sorted, filesystem order, manual list)

## Hardware

| Component | Interface | Pins |
|-----------|-----------|------|
| E-ink display (800x480, 7-color) | SPI1 @ 8MHz | CLK=10, MOSI=11, CS=9, DC=8, RST=12, BUSY=13, PWR=16 |
| SD card (FAT32) | SPI0 @ 12.5MHz | CLK=2, MOSI=3, MISO=4, CS=5 |
| RTC (PCF85063) | I2C1 @ 100kHz | SDA=14, SCL=15, INT=6, addr=0x51 |
| Battery | ADC | VBAT=29, BAT_OFF=18, BAT_STATE=19, CHARGE=17, VBUS=24 |
| LEDs | GPIO | ACT=25, PWR=26 |

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

Edit `src/main.c` before building.

### File Mode

```c
#define Mode 0  // 0=sorted, 1=filesystem, 2=manual
```

- **Mode 0**: Auto-scan `/pic/`, sort alphabetically
- **Mode 1**: Auto-scan `/pic/`, filesystem order (deterministic but not alphabetical - order determined by FAT directory structure)
- **Mode 2**: Manual list from `fileList.txt`

### Refresh Interval

Set in `main()` by adjusting `alarmTime` (default: 12 hours):

```c
// alarmTime.minutes += 15;
alarmTime.hours += 12;
```

Common configurations:
- 15 min: `alarmTime.minutes += 15;`
- 30 min: `alarmTime.minutes += 30;`
- 1 hour: `alarmTime.hours += 1;`
- 12 hours: `alarmTime.hours += 12;`
- 24 hours: `alarmTime.hours += 24;`

Use only one of the `minutes` / `hours` lines.

## Build

```bash
pio run
```

Output: `.pio/build/pico/firmware.uf2`. Flash and RAM usage are printed at the end of the build.

Clean build: `pio run -t clean`

## Flashing

1. Hold **BOOT** button
2. Press **RUN** button
3. Release **RUN**
4. Release **BOOT**
5. Either drag `.pio/build/pico/firmware.uf2` to the `RPI-RP2` USB drive, or run `pio run -t upload`

## Architecture

```
platformio.ini                  Build configuration (flags, include paths, ignored framework libs)
src/
├── main.c                      Entry point, power management, main loop, configuration
├── EPD_7in3f_test.c            Display functions, BMP rendering
└── run_File.c                  File scanning, image index management
include/                        Headers for src/ (EPD_Test.h, run_File.h)
lib/
├── Config/DEV_Config.h         Pin definitions, SPI/I2C/GPIO init
├── e-Paper/EPD_7in3f.c         Display driver (init, display, sleep, clear)
├── GUI/
│   ├── GUI_Paint.c             Drawing primitives, text rendering
│   └── GUI_BMPfile.c           BMP parsing, RGB→7-color mapping
├── RTC/waveshare_PCF85063.c    RTC driver, alarm scheduling
├── FatFs_SPI/                  SD card filesystem (FatFs library)
├── Fonts/                      ASCII (8-24pt) and Chinese fonts
└── led/led.c                   LED indicators, power control
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
- Location: `/pic/` folder on SD card

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

1. `DEV_Module_Init()` - GPIO, SPI, I2C, ADC setup
2. `watchdog_enable(8000)` - 8s watchdog
3. Battery check (min 3.1V) → low power shutdown if below
4. SD card detection and file list scan
5. `run_display()` - render current BMP to display
6. Set RTC alarm for next wake
7. Sleep until RTC interrupt or button press
8. Repeat from step 3

## Key Functions

| Function | File | Purpose |
|----------|------|---------|
| `EPD_7IN3F_Init()` | EPD_7in3f.c | Initialize display |
| `EPD_7IN3F_Display()` | EPD_7in3f.c | Send buffer to display |
| `EPD_7IN3F_Sleep()` | EPD_7in3f.c | Enter low power mode |
| `GUI_ReadBmp_RGB_7Color()` | GUI_BMPfile.c | Load BMP, map exact RGB values to 7-color indices |
| `sdScanDir()` | run_File.c | Scan /pic/ for images |
| `setFilePath()` | run_File.c | Get current image path |
| `updatePathIndex()` | run_File.c | Advance to next image |
| `rtcRunAlarm()` | waveshare_PCF85063.c | Set wake alarm |
| `powerOff()` | led.c | Shutdown (BAT_OFF=LOW) |

## SD Card Files

| File | Purpose |
|------|---------|
| `/pic/*.bmp` | Image files |
| `/fileList.txt` | Generated/manual image list |
| `/index.txt` | Current image index (persists across power cycles) |

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
