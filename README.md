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

## Development Environment Setup (macOS)

### Prerequisites

```bash
# Install build tools
brew install cmake

# Install ARM toolchain (official ARM version with newlib)
curl -LO "https://developer.arm.com/-/media/Files/downloads/gnu/13.2.rel1/binrel/arm-gnu-toolchain-13.2.rel1-darwin-arm64-arm-none-eabi.tar.xz"
tar -xf arm-gnu-toolchain-13.2.rel1-darwin-arm64-arm-none-eabi.tar.xz
sudo mv arm-gnu-toolchain-13.2.Rel1-darwin-arm64-arm-none-eabi /opt/arm-gnu-toolchain

# Clone Pico SDK
git clone --depth 1 https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
cd ~/pico-sdk && git submodule update --init
```

### Environment Variables

Add to `~/.zshrc`:
```bash
export PICO_SDK_PATH="$HOME/pico-sdk"
export PATH="/opt/arm-gnu-toolchain/bin:$PATH"
```

**Note:** The Homebrew `arm-none-eabi-gcc` package lacks newlib and won't work. Use the official ARM toolchain.

## Configuration

Edit `main.c` before building to configure operation mode and refresh interval:

### File Mode (line 20)

```c
#define Mode 0  // 0=sorted, 1=filesystem, 2=manual
```

- **Mode 0**: Auto-scan `/pic/`, sort alphabetically
- **Mode 1**: Auto-scan `/pic/`, filesystem order (deterministic but not alphabetical - order determined by FAT directory structure)
- **Mode 2**: Manual list from `fileList.txt`

### Refresh Interval (lines 64-65)

```c
alarmTime.minutes += 15;    // For 15/30 minute intervals
// alarmTime.hours += 1;    // For 1-24 hour intervals
```

Common configurations:
- 15 min: `alarmTime.minutes += 15;`
- 30 min: `alarmTime.minutes += 30;`
- 1 hour: `alarmTime.hours += 1;` (comment out minutes line)
- 2 hours: `alarmTime.hours += 2;`
- 24 hours: `alarmTime.hours += 24;`

## Build

```bash
mkdir build && cd build
cmake ..
make -j4
```

Output: `build/epd.uf2`

**Note:** `-j4` runs 4 parallel compile jobs. Adjust for your CPU core count.

## Flashing

1. Hold **BOOT** button
2. Press **RUN** button
3. Release **RUN**
4. Release **BOOT**
5. Drag `epd.uf2` to the USB drive that appears

## Architecture

```
main.c                          Entry point, power management, main loop
├── lib/Config/DEV_Config.h     Pin definitions, SPI/I2C/GPIO init
├── lib/e-Paper/EPD_7in3f.c     Display driver (init, display, sleep, clear)
├── lib/GUI/
│   ├── GUI_Paint.c             Drawing primitives, text rendering
│   └── GUI_BMPfile.c           BMP parsing, RGB→7-color conversion
├── lib/RTC/waveshare_PCF85063.c RTC driver, alarm scheduling
├── lib/FatFs_SPI/              SD card filesystem (FatFs library)
├── lib/Fonts/                  ASCII (8-24pt) and Chinese fonts
├── lib/led/led.c               LED indicators, power control
└── examples/
    ├── EPD_7in3f_test.c        Display test functions, BMP rendering
    └── run_File.c              File scanning, image index management
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

- Format: 24-bit BMP
- Resolution: 800x480 or 480x800
- Location: `/pic/` folder on SD card
- Use Waveshare's dithering tool for best results

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
| `GUI_ReadBmp_RGB_7Color()` | GUI_BMPfile.c | Load BMP, convert to 7-color |
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

Enable in `lib/Config/Debug.h`:
```c
#define DEBUG 1
```

USB serial output enabled by default (`pico_enable_stdio_usb`).

## Documentation

- **Product wiki**: [PhotoPainter Wiki](https://www.waveshare.com/wiki/PhotoPainter)
- **Image preparation tools**: See wiki for 7-color dithering conversion tools
- **Pico SDK**: [raspberrypi/pico-sdk](https://github.com/raspberrypi/pico-sdk)
- **Original repository**: [waveshareteam/PhotoPainter](https://github.com/waveshareteam/PhotoPainter)

## License

Open-sourced by Waveshare Electronics. Bug reports and contributions welcome.
