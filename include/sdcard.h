#ifndef SDCARD_H
#define SDCARD_H

#include <stdbool.h>

// Whether a readable SD card is inserted (forces a fresh card init, the board has no card-detect pin)
bool sdcard_present(void);

void sdcard_mount(void);
void sdcard_unmount(void);

#endif
