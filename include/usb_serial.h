#ifndef USB_SERIAL_H
#define USB_SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

// Next character received over USB serial, or -1 when nothing waits
int usb_serial_read(void);
// Drop everything received so far
void usb_serial_discard(void);

#ifdef __cplusplus
}
#endif

#endif
