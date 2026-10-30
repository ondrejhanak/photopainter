#ifndef IMAGE_ORDER_H
#define IMAGE_ORDER_H

#include "settings.h"
#include <stdint.h>

// Display order of two image names: <0 if a comes first, >0 if b does. Alphabetical is a natural,
// case-insensitive name order; random sorts by a hash of (seed, name), so a new seed reshuffles.
// Pure logic without hardware access.
int image_order_compare(const char *a, const char *b, ImageOrder order, uint32_t seed);

#endif
