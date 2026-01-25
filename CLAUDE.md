# Claude Code Project Guidelines

## Git Workflow

**CRITICAL:** Do NOT commit changes to Git without explicit request from the user.

## Build Instructions

The development environment is already configured (set in ~/.zshrc):
- `PICO_SDK_PATH="$HOME/pico-sdk"` 
- `PATH="/opt/arm-gnu-toolchain/bin:$PATH"`

### Standard Build Process

```bash
cd build
cmake ..
make -j4
```

### Binary Size Checking

```bash
arm-none-eabi-size build/epd.elf
```

## Project Architecture

See README.md for architecture overview.

## Optimization Focus

Focus optimizations on **firmware-specific code** (main.c, examples/) rather than library code (lib/) which may be reused or referenced elsewhere.

## Key Files

- **main.c** - Main application logic, safe to modify
- **examples/** - Application-level display functions, safe to modify
- **lib/** - Generic reusable libraries, modify with caution
