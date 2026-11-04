#ifndef PALETTE_H
#define PALETTE_H

#include <stddef.h>
#include <stdint.h>

// One panel ink: the code the panel takes and how the ink looks (duller than pure RGB), which is
// what dithering matches against and what a cache BMP shows on a computer
typedef struct
{
    uint8_t code;
    uint8_t r, g, b;
} PaletteColor;

typedef struct
{
    const PaletteColor *colors;
    int count;
    // Placeholder for the bars around a photo that doesn't fill the frame: a code no photo uses,
    // painted in the bar colour when shown (palette_replace()), so the cache serves every bar colour
    uint8_t bars;
} Palette;

// The 7-colour 7.3" e-Paper (F) panel
extern const Palette palette_7color;

// Replace every pixel of code `from` by `to` in a 4-bit buffer (two pixels per byte)
void palette_replace(uint8_t *buf, size_t len, uint8_t from, uint8_t to);

// Panel code of the colour nearest to r, g, b; *pr, *pg, *pb get that colour's RGB
uint8_t palette_nearest(const Palette *palette, int r, int g, int b, int *pr, int *pg, int *pb);

#endif
