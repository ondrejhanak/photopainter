#ifndef SDCARD_H
#define SDCARD_H

#include <stdbool.h>

// Whether a readable SD card is inserted (forces a fresh card init, the board has no card-detect pin)
bool sdcard_present(void);

void sdcard_mount(void);
void sdcard_unmount(void);

// The frame's own folder on the card (state, cache): a dot-folder (hidden on macOS) with the FAT
// hidden attribute (Windows)
#define SDCARD_FRAME_DIR "/.photopainter"
// Create SDCARD_FRAME_DIR if it is missing
void sdcard_make_frame_dir(void);

#endif
