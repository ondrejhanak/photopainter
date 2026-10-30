// C access to Arduino-Pico's USB serial (Serial is a C++ object), for test commands in loop()
#include "usb_serial.h"
#include <Arduino.h>

int usb_serial_read(void)
{
    return Serial.read();
}

void usb_serial_discard(void)
{
    while (Serial.read() >= 0)
        ;
}
