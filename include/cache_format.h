#ifndef CACHE_FORMAT_H
#define CACHE_FORMAT_H

#include "palette.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// A converted photo as stored in the cache: an upright, top-down 4-bit BMP of the whole frame whose
// palette indices are the panel codes, so it opens on a computer as the frame shows it
#define CACHE_WIDTH 800
#define CACHE_HEIGHT 480
#define CACHE_ROW_BYTES (CACHE_WIDTH / 2)
#define CACHE_HEADER_SIZE 118 // file header, info header, 16 palette entries
#define CACHE_FILE_SIZE (CACHE_HEADER_SIZE + CACHE_ROW_BYTES * CACHE_HEIGHT)
#define CACHE_NAME_LEN 16 // hex digits

// Cache file name (without extension) for a photo's directory entry: a replaced photo with the same
// name gets a new one. Changing CACHE_VERSION in cache_format.c makes every photo convert again.
void cache_name(char out[CACHE_NAME_LEN + 1], const char *name, uint32_t size, uint16_t date, uint16_t time);

void cache_header(uint8_t out[CACHE_HEADER_SIZE], const Palette *palette);
// Whether a header describes a cache file (size, depth, top-down); the palette is not checked
bool cache_header_ok(const uint8_t header[CACHE_HEADER_SIZE]);

// Row y of the file from the panel frame buffer, which holds the frame turned 180 degrees (the panel
// is mounted upside down), and back
void cache_row_from_frame(uint8_t row[CACHE_ROW_BYTES], const uint8_t *frame, int y);
void cache_row_to_frame(uint8_t *frame, const uint8_t row[CACHE_ROW_BYTES], int y);

#endif
