# Wake log

The frame writes one line per event to `.photopainter/log.csv` on the SD card (a hidden folder: in
drive mode, show hidden files, or copy the folder). When `log.csv` reaches 256 KB it becomes
`log.old.csv` (replacing the previous one), so the card holds the last ~4,000 lines. Opening the file
in Numbers or Excel shows it as a table.

There are no dates: the frame has no real clock (the RTC only counts the interval). The line order is
the timeline; on battery, consecutive `change` lines with reason `rtc` are `interval_h` hours apart.

## Columns

```
event,reason,version,battery_v,usb,charging,interval_h,boot_ms,card_ms,select_ms,load_ms,refresh_ms,photo,result,photos,failed
```

| Column | Meaning |
|---|---|
| `event` | What happened, see below |
| `reason` | Why the frame woke or acted, see below |
| `version` | Firmware version (`git describe`, e.g. `v1.1.0`) |
| `battery_v` | Battery voltage measured at boot, before the card and the panel draw current. On USB it reads the charging voltage |
| `usb` | 1 on USB power, 0 on battery |
| `charging` | 1 while the battery charges |
| `interval_h` | `interval_hours` from `settings.ini` (the default 12 when the card cannot be read; 0 on `bat_off` lines, written before the card is read) |
| `boot_ms` | Boot until the card is read; includes fixed waits of 1 s in `setup()` and 0.5 s in the RTC init |
| `card_ms` | Detecting the card (a fresh card init) and reading `settings.ini` (created with comments if missing) |
| `select_ms` | Choosing the photo (scanning the card root) |
| `load_ms` | Loading the photo from its cache file or converting it; on `drive_end` lines the length of the drive-mode session |
| `refresh_ms` | Panel refresh (~31 s; longer when the next photo's conversion outlasts it) |
| `photo` | File name shown (quoted when it contains a comma or quote) |
| `result` | `cache` (from its cache file), `converted` (decoded now), `message` (a screen instead of a photo) |
| `photos` | JPEGs in the card root; on `drive_end` lines the number of photos added |
| `failed` | Photos skipped because they could not be shown |

Times are milliseconds; a step that did not run is 0.

## Events

| `event` | When |
|---|---|
| `change` | A photo or a message screen was shown |
| `drive_end` | Drive mode ended (eject, long press, or timeout). `refresh_ms` and `result` (`message`) are the "USB Drive" screen's; `card_ms` is the card scan after the session, so the `change` line after it has 0 there |
| `usb_plugged` | The frame was plugged into USB (it stays awake; changes follow as `change` lines) |
| `usb_unplugged` | USB was removed and the frame switched off (not logged when it happens in drive mode) |
| `bat_empty` | Battery below 3.3 V: the "Battery Empty" screen, no more changes until charged |
| `bat_off` | Battery below 3.1 V: switched off without a refresh |

## Reasons

| `reason` | Meaning |
|---|---|
| `rtc` | The scheduled change (RTC alarm), on battery or USB |
| `key` | Short KEY press |
| `key_long` | Drive mode ended by holding KEY |
| `serial` | Serial test command `n` |
| `usb_plugged` | Powered on with USB: plugged in, or restarted after flashing |
| `eject` | Drive mode ended by ejecting the drive |
| `drive_timeout` | Drive mode ended by itself (the computer never read the drive, or left it idle 30 minutes) |
| `watchdog:<step>` | The frame hung and the watchdog restarted it in `<step>`: `boot`, `card`, `select`, `load`, `refresh`, `save`, `drive` (`idle` between changes) |

The reason stays for the rest of the wake: the `change` after a `drive_end` line, or the `usb_unplugged`
line, has the reason of the last action.

## Reading it

- **Discharge curve:** filter `usb = 0`, chart `battery_v` against the line number, or against the
  running sum of `interval_h` over `rtc` lines for hours on battery. Wakes per charge are the battery
  lines between two `usb_plugged` lines.
- **Charging:** a `usb_plugged` / `usb_unplugged` pair is one session on USB; `battery_v` of the next
  battery line shows how far it charged.
- **Energy per wake:** the sum of the `*_ms` columns is the awake time; `converted` lines cost the
  conversion on top.
- **Hangs:** `watchdog:` reasons name the step that never finished.
