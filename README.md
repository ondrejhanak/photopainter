# PhotoPainter

[![CI](https://github.com/ondrejhanak/photopainter/actions/workflows/ci.yml/badge.svg)](https://github.com/ondrejhanak/photopainter/actions/workflows/ci.yml)

Firmware for the Waveshare PhotoPainter, a battery-powered 7.3" e-paper photo frame, in both models:
the original with a 7-color panel and an RP2040, and the PhotoPainter (B) with a 6-color panel and an
RP2350. Copy photos onto the SD card and the frame shows a new one every few hours, switching itself
off in between.

Based on the official Waveshare firmware (for the [7-color](https://github.com/waveshareteam/PhotoPainter)
and the [6-color](https://github.com/waveshareteam/PhotoPainter_B) model), reworked to be easier to use
and to develop.

## What it does

- **Photos straight from your phone or camera:** copy JPEGs to the card and the frame converts them
  itself, once.
- **No need to take the card out:** with a computer connected, hold the button for 3 seconds and the
  card shows up as a USB drive.
- **New photos first**, then the rest in random order (or by name), with no skips or repeats.
- **Settings in a text file on the card** (`settings.ini`): interval, order, orientation, date, bar
  color, language.
- **Landscape or portrait**, and the date the photo was taken if you want it.
- **A battery icon** when the battery runs low, and a clear message when it's empty.
- **Broken photos are skipped**, and their names appear at the bottom of the next photo.
- **Czech or English** on-screen messages.
- **Works on both models**, the 7-color PhotoPainter and the 6-color PhotoPainter (B).

## Installing

Get the firmware from [Releases](https://github.com/ondrejhanak/photopainter/releases). There's one
file per display: `photopainter-7color-<version>.uf2` if your frame shows 7 colors, or
`photopainter-6color-<version>.uf2` if it shows 6 (the PhotoPainter (B)).

Hold **BOOT** while you plug the frame into your computer (or, if it's already plugged in, hold
**BOOT**, press and release **RUN**, then let go of **BOOT**). A drive appears: `RPI-RP2` on the 7-color
frame, `RP2350` on the 6-color one, so the drive name also tells you which file you need. Copy the
`.uf2` onto it and the frame restarts with the new firmware. A file meant for the other model is
refused. Your photos, settings and converted photos on the SD card stay as they are.

## Photos & SD card

| On the card | Purpose |
|---|---|
| `*.jpg`, `*.jpeg` in the root | Photos (subfolders, dot-files and other files are ignored) |
| `settings.ini` | Settings, created with comments on first start |
| `.photopainter/` | The frame's own state and converted photos (hidden) |
| `.photopainter/log.csv` | Wake log: one line per photo change, drive session or battery event ([docs/wake-log.md](docs/wake-log.md)) |

Photos play in random order, or by name if you prefer; the ones you've just copied over USB come
first. Adding or removing photos never makes the frame skip or repeat one.

### settings.ini

Open it in any text editor. Changes take effect when you eject the USB drive, or at the next photo
change (scheduled, or when you press the button). The frame ignores lines it doesn't know, and an
invalid value falls back to its default.

```ini
; Hours between image changes (1 to 47)
interval_hours = 12

; Image order: random or alphabetical (random shows every image once before reshuffling)
order = random

; Frame position: landscape, or portrait (turned counter-clockwise)
orientation = landscape

; Date the photo was taken (from the camera), at the bottom: no or yes
show_date = no

; Colour around photos that don't fill the screen: white or black
bars = white

; Language of on-screen messages: en (English) or cs (Czech)
language = en
```

### Photos

Copy JPEGs just as they come, without resizing or converting them. Photos in the frame's orientation
fill the screen (trimmed a little at the edges); the others are shown whole, with bars
in white or black (`bars` in `settings.ini`) to match the frame's mount. The frame converts each photo
the first time it's due, which takes up to about 40 s for a 24-megapixel photo, and usually that
happens while the previous photo is still on screen. It can't show progressive or CMYK JPEGs, or
HEIC (an iPhone's default; the Photos app exports JPEG).

## More

- [Changes from the original firmware](docs/changes.md), including the bugs fixed
- [Hardware and how the frame behaves](docs/hardware.md)
- [Development](docs/development.md): build, flash, tests, architecture
- [Wake log](docs/wake-log.md) on the card
- [Ideas](docs/ideas.md) being considered
