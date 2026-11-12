# Hardware

Product details and schematics: [PhotoPainter wiki](https://www.waveshare.com/wiki/PhotoPainter),
[PhotoPainter (B) wiki](https://www.waveshare.com/wiki/PhotoPainter_(B)). Both models use the same pins;
the (B) has an RP2350 with 16 MB flash and a 6-color panel (7.3" e-Paper (E)) instead of the 7-color
one (F).

| Component | Interface | Pins |
|-----------|-----------|------|
| E-ink display (800x480, 7 or 6 colors) | SPI1 @ 8MHz | CLK=10, MOSI=11, CS=9, DC=8, RST=12, BUSY=13, PWR=16 |
| SD card (FAT32/exFAT) | SPI0 @ 24MHz (18.75MHz on the (B)) | CLK=2, MOSI=3, MISO=4, CS=5 (no card-detect pin) |
| RTC (PCF85063) | I2C1 @ 100kHz | SDA=14, SCL=15, INT=6 (unused), addr=0x51 |
| Power / battery | GPIO, ADC | VBAT=29 (ADC), BAT_OFF=18, BAT_STATE=19 (KEY / RTC wake sense), CHARGE=17, VBUS=24 |
| LEDs | GPIO | ACT=25 (red), PWR=26 (green) |

## How it behaves

- **Power:** on battery the board keeps itself powered while working and cuts its own power with
  `BAT_OFF` (`powerOff()`) when done. It is fully off between images.
- **Wake-up:** the KEY button and the RTC alarm both feed the power-on circuit and both pull
  `BAT_STATE` low. On battery either one powers the board on; on USB the firmware sees either as a
  "KEY" event. The RTC `INT` pin (GPIO 6) never shows the alarm and is not used.
- **RTC:** used only as a countdown timer. On every run it is reset to a fixed reference time and the
  alarm is set `interval_hours` (settings.ini) later; the time is never read.
- **On USB:** the board stays awake. A short KEY press (released within 3 s) or the RTC alarm shows
  the next image, the SD card is re-scanned each time (cards can be swapped), and unplugging USB
  powers the board off. Holding KEY for 3 s is a long press; the red LED lights when it is reached.
  The RTC alarm holds the KEY line low until its flag is cleared, so the flag tells it apart from a
  held KEY.
- **USB drive mode:** on USB the frame is always a USB drive next to the serial port, empty like a
  card reader without a card (Windows may show an empty drive letter). A long press with a computer
  connected puts the SD card in: the computer mounts it within a second, while a "USB Drive" screen
  refreshes (flashing and the log keep working). The frame leaves the card alone until the drive is
  ejected or KEY is held again, or until the computer unmounts it without ejecting (e.g. ForkLift):
  a computer marks the card's file system "in use" while mounted (FAT32 clean-shutdown bit, exFAT
  VolumeDirty) and "clean" on unmount, and the frame leaves 3 s after a "clean" with no command
  following. Then it re-reads the card and `settings.ini` and shows the first new
  photo (or the next image). It also ends by itself when the computer does not read the drive
  within a minute (mount failed) or leaves it untouched for 30 minutes (forgotten eject,
  computer asleep), so the photos never stop for good. On a plain charger a long press is ignored
  and the red LED blinks 3 times. Up to about 0.4 MB/s, the limit of USB full speed with 64-byte
  packets; how the computer handles the drive can make it slower.
- **LEDs:** green (PWR) is the charging light: on while the battery charges, off when it is full or
  on battery. Red (ACT) blinks 3 times at power-on, flickers while the SD card is in use (the SD
  driver reports activity, the firmware owns the LED), lights once a long KEY press is reached and
  stays on in USB drive mode.
- **SD card:** with no card-detect pin, the firmware forces card re-initialization on every check.
- **Refresh:** about 31 s on the PhotoPainter, about 20 s on the PhotoPainter (B); colder panels take
  longer.
