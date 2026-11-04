#ifndef EXIF_H
#define EXIF_H

#include <stddef.h>
#include <stdint.h>

// EXIF orientation from the start of an APP1 segment payload ("Exif\0\0" + TIFF data).
// Returns 1 (as stored), 3 (180 deg), 6 (rotate 90 deg clockwise to view) or 8 (counter-clockwise);
// mirrored variants, other APP1 data and anything outside the first len bytes count as 1.
int exif_orientation(const uint8_t *app1, size_t len);

#endif
