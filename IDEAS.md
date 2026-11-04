# Ideas

In no particular order; not everything here will happen.

## Features

- **Unusable photos notice**: JPEGs the frame cannot show (progressive, CMYK) are skipped and logged; the drive-mode exit screen could say "M photos cannot be used".
- **Frame orientation**: `orientation = landscape | portrait`, default `landscape`. Photos, message screens and battery icon follow it; converted photos get a cache folder per orientation (`cache/portrait/` next to `cache/landscape/`).
- **One new photo after eject**: show the first new photo (natural order) as confirmation, then the normal order. New = hash (the cache key) not in the set taken before drive mode. `state.txt` not updated, so the photo still gets its own slot later.
- **Low-battery icon**: low = small red icon in a corner, photos keep changing; critical = existing low-battery screen, then no more wake-ups (e-paper keeps it unpowered). Voltage read at the same point every wake, with hysteresis; thresholds from the logged discharge curve.
- **Photo date caption**: `show_date = no | yes`, default `no`. EXIF `DateTimeOriginal`, fallback `DateTime`, read from the JPEG when shown; plain string, no calendar math. Formatted per `language` (e.g. 24. 12. 1985), black on a small white box in a corner, not in the cache. No EXIF date, no caption.

## Code and tooling

- **Host tests + CI badge**: PlatformIO native + Unity for image order, random rounds and `settings.ini` parsing. GitHub Actions (`ubuntu-latest`, free for public repos) builds firmware and runs `pio test -e native` on every push; badge in README. The JPEG checks of `tools/jpeg_host/` could run there too.
- **Fuzzing**: JPEG/EXIF parsing as a repeatable test (CI), to prove "bad files don't break the frame".
- **Build hygiene**: warnings, static analysis, stack usage and firmware size checks in CI.

## Battery

- **Measure first**: time per wake phase (boot, SD read, ~30 s panel refresh) from the log, as baseline.
- **Savings**: drop the unexplained 1 s boot delay; sleep instead of busy-waiting during refresh; lower CPU clock when nothing heavy runs.
- **Write-up**: before/after numbers in README.

## Logging

- **Wake log**: one CSV line (~100 B) per wake in `.photopainter/`. Rotation `log.txt`, `log1.txt` … `logN.txt` (e.g. N = 4, 256 KB each): when full, delete the oldest, rename the rest up by one, oldest first (FatFs `f_rename` refuses an existing target). Fields:
  - wake reason: RTC alarm, KEY, USB plugged in, power-on, watchdog reset
  - battery voltage (same point every wake), USB present, charging
  - time per phase: boot, SD mount, image scan, image load/decode, panel refresh
  - image shown, result (ok / error code), image count on card
  - later: JPEG cache hit/miss, decode time per megapixel
- **Events**: firmware version at boot, drive-mode sessions (duration, photos added), SD/FatFs errors.
- **Reset diagnostics**: after a watchdog reset, reason and phase, kept across the reset in watchdog scratch registers.
- **Uses**: discharge curve and wakes per charge (low-battery thresholds), energy baseline, remote diagnosis of a gifted frame, checking known issues below.

## Known issues

- Serial log lines sometimes lose their first character after idle gaps.
- KEY presses during a panel refresh are probably lost (unconfirmed).

## Considered, dropped

- Night pause: fixed change times would replace it; both need a real clock.
- Photos for a date: needs order exceptions and a hidden naming rule, for a few uses a year.
- Info screen (battery, photo count, version, last error): little use for the user; owner reads logs on the card.
- Real clock and fixed change times (`change_at = 07:00`): needs time sync and a DST decision; the interval is good enough.
- User BMP images (made with a conversion script): JPEGs are converted on the frame; BMPs needed a computer.
- Convert all new photos right after eject on USB power: users unplug after ejecting, and converting during the refresh costs almost nothing.
