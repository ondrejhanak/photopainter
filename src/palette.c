#include "palette.h"
#include "EPD_7in3e.h"
#include "EPD_7in3f.h"

// RGB values as the dither palette of tools/jpeg_host/convert_reference.py, the look the frame matches
static const PaletteColor colors_7color[] = {
    {EPD_7IN3F_BLACK, 5, 5, 5},
    {EPD_7IN3F_WHITE, 245, 245, 245},
    {EPD_7IN3F_GREEN, 65, 140, 30},
    {EPD_7IN3F_BLUE, 30, 65, 240},
    {EPD_7IN3F_RED, 190, 0, 0},
    {EPD_7IN3F_YELLOW, 255, 245, 55},
    {EPD_7IN3F_ORANGE, 230, 125, 0},
};

const Palette palette_7color = {
    .colors = colors_7color,
    .count = sizeof(colors_7color) / sizeof(colors_7color[0]),
    .bars = EPD_7IN3F_CLEAN, // leaves an afterimage, so never a photo colour
};

// The inks as measured with a colorimeter on a Spectra 6 panel (esp32-photoframe, docs/MEASURED_PALETTE.md)
static const PaletteColor colors_6color[] = {
    {EPD_7IN3E_BLACK, 2, 2, 2},
    {EPD_7IN3E_WHITE, 179, 182, 171},
    {EPD_7IN3E_GREEN, 33, 69, 40},
    {EPD_7IN3E_BLUE, 0, 47, 107},
    {EPD_7IN3E_RED, 117, 10, 0},
    {EPD_7IN3E_YELLOW, 201, 184, 0},
};

const Palette palette_6color = {
    .colors = colors_6color,
    .count = sizeof(colors_6color) / sizeof(colors_6color[0]),
    .bars = EPD_7IN3E_UNUSED,
    .black_luma = 2,
    .white_luma = 180,
};

void palette_replace(uint8_t *buf, size_t len, uint8_t from, uint8_t to)
{
    for (size_t i = 0; i < len; i++)
    {
        uint8_t b = buf[i];
        if ((b >> 4) == from)
            b = (uint8_t)((b & 0x0F) | to << 4);
        if ((b & 0x0F) == from)
            b = (uint8_t)((b & 0xF0) | to);
        buf[i] = b;
    }
}

uint8_t palette_nearest(const Palette *palette, int r, int g, int b, int *pr, int *pg, int *pb)
{
    const PaletteColor *best = &palette->colors[0];
    int best_dist = -1;
    for (int i = 0; i < palette->count; i++)
    {
        const PaletteColor *c = &palette->colors[i];
        int dr = r - c->r, dg = g - c->g, db = b - c->b;
        int dist = dr * dr + dg * dg + db * db;
        if (best_dist < 0 || dist < best_dist)
        {
            best = c;
            best_dist = dist;
        }
    }
    *pr = best->r;
    *pg = best->g;
    *pb = best->b;
    return best->code;
}
