#ifndef PALETTE_H
#define PALETTE_H

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
    uint8_t white; // panel code for areas outside the photo
} Palette;

// The 7-colour 7.3" e-Paper (F) panel
extern const Palette palette_7color;

// Panel code of the colour nearest to r, g, b; *pr, *pg, *pb get that colour's RGB
uint8_t palette_nearest(const Palette *palette, int r, int g, int b, int *pr, int *pg, int *pb);

#endif
