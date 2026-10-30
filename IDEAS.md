# Ideas

In no particular order; not everything here will happen.

## Features

- **JPEG support**: copy photos straight from a computer, no conversion script. Decode, scale and dither once, then cache. Must stream: 264 KB RAM, 192 KB of it the frame buffer.
  - Cache: 4-bit indexed, top-down BMP, palette indices = panel color codes. Pixel data is the frame buffer as is (one read to display), and the file opens on a computer.
  - Folder per orientation (`cache/landscape/`, `cache/portrait/`), ~192 KB per photo in each.
  - File name: `hash(name, size, date, time)` of the directory entry, so a replaced file (e.g. another phone's `IMG_0001.jpg`) is converted again. Cleanup removes orphans.
  - Convert right after eject on USB power, else when the photo is due.
  - EXIF orientation 3/6/8 applied, mirrored ones ignored.
  - Unsupported files (progressive JPEG, HEIC) skipped and logged; drive-mode exit screen can say "N new photos, M cannot be used".
  - Photo in the frame's orientation: fill, center crop. Other: fit whole, white bars.
- **Frame orientation**: `orientation = landscape | portrait`, default `landscape`. Photos, message screens and battery icon follow it.
- **USB drive mode**: on a computer, long KEY press exposes the SD card as a USB drive; eject or another long press gives it back to the frame.
- **Localization**: `language = en | cs`, UTF-8 string table with diacritics.
  - Text drawing decodes UTF-8; missing glyph → base letter (č → c), else `?`.
  - Font made by a `tools/` script from an open bitmap font (e.g. Terminus), U+0020–U+017F in one table, so a new Latin-script language needs only strings. Sizes 16 and 20, ~10–13 KB each. Check room for accents above capitals.
  - Word wrap on message screens.
- **One new photo after eject**: show the first new photo (natural order) as confirmation, then the normal order. New = hash (the cache key) not in the set taken before drive mode. `state.txt` not updated, so the photo still gets its own slot later.
- **Low-battery icon**: low = small red icon in a corner, photos keep changing; critical = existing low-battery screen, then no more wake-ups (e-paper keeps it unpowered). Voltage read at the same point every wake, with hysteresis; thresholds from the logged discharge curve.
- **Photo date caption**: `show_date = no | yes`, default `no`. EXIF `DateTimeOriginal`, fallback `DateTime`, read from the JPEG when shown; plain string, no calendar math. Formatted per `language` (e.g. 24. 12. 1985), black on a small white box in a corner, not in the cache. No EXIF date, no caption.

## Code and tooling

- **Cleanup**: one naming style (snake_case, module prefixes like `display_*`, `images_*`), files named by content; `lib/` keeps upstream style. Remove Chinese fonts (GB2312 files), CN string drawing, Chinese comments, and unused Font8, Font12, Font24.
- **Host tests + CI badge**: PlatformIO native + Unity for image order, random rounds and `settings.ini` parsing. GitHub Actions (`ubuntu-latest`, free for public repos) builds firmware and runs `pio test -e native` on every push; badge in README. Later the JPEG pipeline, checked on preview PNGs instead of flashing.
- **Fuzzing**: BMP, later EXIF/JPEG parsing, to prove "bad files don't break the frame".
- **Build hygiene**: warnings, static analysis, stack usage and firmware size checks in CI.
- **BMP decoder**: nearest-colour match instead of exact; read whole rows, not three 1-byte reads per pixel.

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
