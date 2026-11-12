#ifndef USB_DRIVE_H
#define USB_DRIVE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Whether an awake computer is connected (not just a charger)
bool usb_drive_computer_connected(void);
// Put the SD card in the USB drive (empty until then); false if the card is unreadable
bool usb_drive_start(void);
// Take the SD card out of the USB drive again
void usb_drive_stop(void);
// Answer the computer's reads and writes for ms milliseconds
void usb_drive_serve(uint32_t ms);
// Whether the computer ejected the drive
bool usb_drive_ejected(void);
// Whether the computer has read the drive since usb_drive_start() (mounting it always does)
bool usb_drive_used(void);
// Whether the computer marked the card's file system clean, as it does when it unmounts it; an eject
// follows within a second, an unmount without eject (e.g. ForkLift) sends nothing more
bool usb_drive_unmounted(void);
// Milliseconds since the computer's last drive command (macOS sends none while a mounted drive is idle)
uint32_t usb_drive_idle_ms(void);

#ifdef __cplusplus
}
#endif

#endif
