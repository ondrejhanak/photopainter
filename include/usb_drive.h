#ifndef USB_DRIVE_H
#define USB_DRIVE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Whether a computer is connected (not just a charger)
bool usb_drive_computer_connected(void);
// Offer the SD card to the computer as a USB drive, next to the serial port; false if the card is unreadable
bool usb_drive_start(void);
// Take the drive away from the computer (the serial port stays)
void usb_drive_stop(void);
// Answer the computer's reads and writes for ms milliseconds
void usb_drive_serve(uint32_t ms);
// Whether the computer ejected the drive
bool usb_drive_ejected(void);

#ifdef __cplusplus
}
#endif

#endif
