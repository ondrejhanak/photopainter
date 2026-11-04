#ifndef CACHE_H
#define CACHE_H

#include <stdbool.h>
#include <stdint.h>

// Converted photos on the card, one cache file per photo (see cache_format.h). Expects the card mounted.

// Whether photo `name` (in the card root) has a cache file
bool cache_exists(const char *name);
// Load the cached frame of photo `name` (in the card root) into the panel frame buffer; false if there
// is none or it cannot be read
bool cache_load(const char *name, uint8_t *frame);
// Save the frame buffer as the cache file of photo `name`. Written under a temporary name and renamed
// when complete, so a power cut never leaves a partial cache file
bool cache_save(const char *name, const uint8_t *frame);

#endif
