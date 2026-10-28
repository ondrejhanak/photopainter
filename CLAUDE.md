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
pio run -t upload       # flash (device must be in BOOTSEL mode)
pio device monitor      # USB serial
```

Output: `.pio/build/pico/firmware.uf2` (and `firmware.elf`).

### Binary Size Checking

Flash/RAM usage is printed at the end of every build (`-Wl,--print-memory-usage` in `platformio.ini`).
For per-section detail:

```bash
~/.platformio/packages/toolchain-rp2040-earlephilhower/bin/arm-none-eabi-size .pio/build/pico/firmware.elf
```

## Project Architecture

See README.md for architecture overview.

## Optimization Focus

Focus optimizations on **firmware-specific code** (`src/`, `include/`) rather than library code (`lib/`)
which may be reused or referenced elsewhere.

## Key Files

- **src/main.c** - Main application logic (mode, refresh interval, power flow), safe to modify
- **src/EPD_7in3f_test.c, src/run_File.c** - Application-level display and file-management functions, safe to modify
- **include/** - Headers for the `src/` files
- **lib/** - Generic reusable libraries (display driver, GUI/BMP decoding, FatFs, RTC), modify with caution
- **convert/** - Host-side Python script to resize/crop photos and dither them to the exact 7-color palette (24-bit BMP) for the SD card
- **platformio.ini** - Build flags, include paths, ignored framework libraries
