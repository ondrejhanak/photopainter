# Changes from the original firmware

What this firmware does differently from the [official Waveshare firmware](https://github.com/waveshareteam/PhotoPainter).

## Using the frame

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
- **Firmware version on the frame:** the "USB Drive" screen shows it, so it can be checked before
  and after an update.

## Developing

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
- **Photo order and settings tests on the computer:** `tools/images_host/build.sh` runs the photo
  selection (order, new photos first, cache cleanup) and the `settings.ini` parsing on the real FatFs
  over a RAM disk.
- **Documented hardware behavior**, e.g. the RTC alarm arriving on the KEY line and the missing SD
  card-detect pin (see [How it behaves](hardware.md#how-it-behaves)).

## Bugs fixed

- **Nothing drawn when memory ran short.** The frame buffer was allocated for every change with a few
  hundred bytes to spare on the PhotoPainter, so a small change elsewhere could leave the screen
  unchanged; it is now reserved when the firmware is built.
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
- **Indented settings ignored.** A line in `settings.ini` that started with spaces was read as part of
  the line above, so that setting was silently lost; now every line is read on its own.
- **Restart when plugged in with a flat battery.** Below 3.1 V the frame switched itself off even on
  USB, which cannot cut its power, so it hung until the watchdog restarted it 8 s later; now it stays
  on and charges.
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
- **`u_int8_t` typo** in the SD driver, which broke builds on toolchains without BSD types.
