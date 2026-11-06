#ifndef EXIF_H
#define EXIF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Both read from the start of an APP1 segment payload ("Exif\0\0" + TIFF data); anything outside the
// first len bytes counts as missing.

// EXIF orientation. Returns 1 (as stored), 3 (180 deg), 6 (rotate 90 deg clockwise to view) or 8
// (counter-clockwise); mirrored variants and other APP1 data count as 1.
int exif_orientation(const uint8_t *app1, size_t len);

typedef struct
{
    uint16_t year;
    uint8_t month, day;
} ExifDate;

// When the photo was taken: DateTimeOriginal, else DateTime (last change); false if neither holds a
// real date
bool exif_date(const uint8_t *app1, size_t len, ExifDate *date);

#endif
