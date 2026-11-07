# Ideas

In no particular order; not everything here will happen.

## Code and tooling

- **Web flasher**: a page on GitHub Pages that updates the frame from Chrome/Edge: Web Serial 1200-baud touch reboots it into BOOTSEL, then the release `.uf2` is written to the `RPI-RP2` drive (File System Access API) or flashed over WebUSB (PICOBOOT). Card and settings untouched. After releases and CI exist.
- **Host tests + CI badge**: PlatformIO native + Unity for image order, random rounds and `settings.ini` parsing. GitHub Actions (`ubuntu-latest`, free for public repos) builds firmware and runs `pio test -e native` on every push; badge in README. The JPEG checks of `tools/jpeg_host/` could run there too.
- **Fuzzing**: JPEG/EXIF parsing as a repeatable test (CI), to prove "bad files don't break the frame".
- **Build hygiene**: warnings, static analysis, stack usage and firmware size checks in CI.

## Battery

- **Measure first**: time per wake phase (boot, SD read, ~30 s panel refresh) from the log, as baseline.
- **Savings**: sleep instead of busy-waiting during refresh; lower CPU clock when nothing heavy runs.
- **Write-up**: before/after numbers in README.

## Known issues

- Serial log lines sometimes lose their first character after idle gaps.
- KEY presses during a panel refresh are probably lost (unconfirmed).

## Considered, dropped

- Night pause: fixed change times would replace it; both need a real clock.
- Photos for a date: needs order exceptions and a hidden naming rule, for a few uses a year.
- Info screen (battery, photo count, version, last error): little use for the user; owner reads logs on the card.
- Real clock and fixed change times (`change_at = 07:00`): needs time sync and a DST decision; the interval is good enough.
- User BMP images (made with a conversion script): JPEGs are converted on the frame; BMPs needed a computer.
- Only one new photo after eject as confirmation: in random order (the default) the other new photos could wait months for their slot.
- Convert all new photos right after eject on USB power: users unplug after ejecting, and converting during the refresh costs almost nothing.
- Card errors in the wake log: the ones that matter show indirectly (a repeated photo, `converted` again and again, `failed`), and the main one, an unreadable card, cannot be written to the card.
