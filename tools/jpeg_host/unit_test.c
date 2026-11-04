// Unit checks for EXIF parsing, the colour enhancement and the cache format; run by check.py.
// Includes jpeg_convert.c to reach its static functions.
#include "../../src/jpeg_convert.c"
#include "cache_format.h"
#include "EPD_7in3f.h"

#define SHORT 3 // TIFF field type
#include <stdio.h>
#include <stdlib.h>

static int failures;

static void expect(const char *name, int got, int want)
{
    if (got != want)
    {
        printf("FAIL %s: got %d, want %d\n", name, got, want);
        failures++;
    }
}

static void put16(uint8_t *p, int big_endian, unsigned v)
{
    p[big_endian ? 0 : 1] = (uint8_t)(v >> 8);
    p[big_endian ? 1 : 0] = (uint8_t)v;
}

static void put32(uint8_t *p, int big_endian, unsigned v)
{
    put16(p + (big_endian ? 0 : 2), big_endian, v >> 16);
    put16(p + (big_endian ? 2 : 0), big_endian, v & 0xFFFF);
}

// APP1 payload with IFD0 at offset 8 holding `count` entries; the orientation entry is the last one
static size_t make_exif(uint8_t *buf, int big_endian, int count, unsigned type, unsigned n, unsigned value)
{
    memcpy(buf, "Exif\0\0", 6);
    uint8_t *t = buf + 6;
    memcpy(t, big_endian ? "MM\0\x2a" : "II\x2a\0", 4);
    put32(t + 4, big_endian, 8);
    put16(t + 8, big_endian, (unsigned)count);
    for (int i = 0; i < count; i++)
    {
        uint8_t *e = t + 10 + 12 * i;
        int last = i == count - 1;
        put16(e, big_endian, last ? 0x0112 : 0x010F + (unsigned)i); // Make, Model, ... then Orientation
        put16(e + 2, big_endian, last ? type : 2);
        put32(e + 4, big_endian, last ? n : 4);
        put32(e + 8, big_endian, 0);
        if (last)
            put16(e + 8, big_endian, value);
    }
    return 6 + 10 + 12 * (size_t)count + 4;
}

static void test_exif(void)
{
    uint8_t buf[256];
    char name[80];
    for (int be = 0; be < 2; be++)
    {
        const char *order = be ? "big-endian" : "little-endian";
        static const int values[][2] = {{1, 1}, {3, 3}, {6, 6}, {8, 8}, {2, 1}, {4, 1}, {5, 1}, {7, 1}, {9, 1}};
        for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++)
        {
            size_t len = make_exif(buf, be, 3, SHORT, 1, (unsigned)values[i][0]);
            snprintf(name, sizeof(name), "%s orientation %d", order, values[i][0]);
            expect(name, exif_orientation(buf, len), values[i][1]);
        }
        size_t len = make_exif(buf, be, 3, 4, 1, 6); // LONG instead of SHORT
        snprintf(name, sizeof(name), "%s wrong type", order);
        expect(name, exif_orientation(buf, len), 1);
        len = make_exif(buf, be, 3, SHORT, 2, 6);
        snprintf(name, sizeof(name), "%s two values", order);
        expect(name, exif_orientation(buf, len), 1);

        // The orientation entry lies just past len: the bytes are there, but must not be read
        len = make_exif(buf, be, 3, SHORT, 1, 6);
        snprintf(name, sizeof(name), "%s entry beyond len", order);
        expect(name, exif_orientation(buf, len - 4 - 1), 1);
        put32(buf + 6 + 4, be, 300); // IFD0 offset beyond the data
        snprintf(name, sizeof(name), "%s IFD beyond len", order);
        expect(name, exif_orientation(buf, len), 1);
    }
    size_t len = make_exif(buf, 1, 1, SHORT, 1, 6);
    memcpy(buf, "Exif\0X", 6);
    expect("not Exif", exif_orientation(buf, len), 1);
    len = make_exif(buf, 1, 1, SHORT, 1, 6);
    memcpy(buf + 6, "MX", 2);
    expect("bad byte order mark", exif_orientation(buf, len), 1);
    expect("too short", exif_orientation(buf, 10), 1);
}

static int clip(double v)
{
    return v < 0 ? 0 : v > 255 ? 255 : (int)v;
}

// Pillow's ImageEnhance.Contrast (pivot 128 here) then ImageEnhance.Color, in floating point
static void test_enhance(void)
{
    int worst = 0;
    for (int r = 0; r < 256; r += 15)
        for (int g = 0; g < 256; g += 15)
            for (int b = 0; b < 256; b += 15)
            {
                uint8_t rgb[3] = {(uint8_t)r, (uint8_t)g, (uint8_t)b};
                int v[3], want[3];
                enhance(rgb, v);
                int c[3];
                for (int ch = 0; ch < 3; ch++)
                    c[ch] = clip(128 + (rgb[ch] - 128) * 1.1);
                double luma = c[0] * 0.299 + c[1] * 0.587 + c[2] * 0.114;
                for (int ch = 0; ch < 3; ch++)
                {
                    want[ch] = clip(luma + (c[ch] - luma) * 1.2);
                    int d = abs(v[ch] - want[ch]);
                    if (d > worst)
                        worst = d;
                }
            }
    if (worst > 3)
    {
        printf("FAIL enhance: off by up to %d from Pillow's formulas\n", worst);
        failures++;
    }
}

static void test_palette_replace(void)
{
    uint8_t buf[] = {0x77, 0x17, 0x71, 0x12, 0x07};
    static const uint8_t want[] = {0x00, 0x10, 0x01, 0x12, 0x00};
    palette_replace(buf, sizeof(buf), 7, 0);
    expect("bars replaced in both nibbles", memcmp(buf, want, sizeof(buf)), 0);
}

static void test_cache_format(void)
{
    // Frame buffer as Paint_SetPixel() fills it with rotation 180: viewed (x, y) at (W-1-x, H-1-y)
    static uint8_t frame[CACHE_ROW_BYTES * CACHE_HEIGHT], back[sizeof(frame)];
    for (size_t i = 0; i < sizeof(frame); i++)
        frame[i] = (uint8_t)(i * 7 + i / 400);
    uint8_t row[CACHE_ROW_BYTES];
    int wrong = 0;
    for (int y = 0; y < CACHE_HEIGHT; y++)
    {
        cache_row_from_frame(row, frame, y);
        for (int x = 0; x < CACHE_WIDTH; x++)
        {
            int px = CACHE_WIDTH - 1 - x, py = CACHE_HEIGHT - 1 - y;
            uint8_t b = frame[py * CACHE_ROW_BYTES + px / 2];
            int panel = px % 2 ? b & 0x0F : b >> 4;
            int file = x % 2 ? row[x / 2] & 0x0F : row[x / 2] >> 4; // BMP: left pixel in the high nibble
            wrong += panel != file;
        }
        cache_row_to_frame(back, row, y);
    }
    expect("cache rows upright", wrong, 0);
    expect("cache rows round trip", memcmp(frame, back, sizeof(frame)), 0);

    uint8_t header[CACHE_HEADER_SIZE];
    cache_header(header, &palette_7color);
    expect("cache header accepted", cache_header_ok(header), 1);
    expect("white palette entry", header[54 + 4 * EPD_7IN3F_WHITE + 2], 245);
    expect("bar placeholder grey", header[54 + 4 * palette_7color.bars + 2], 128);
    header[22] ^= 1; // other height
    expect("other header refused", cache_header_ok(header), 0);

    uint64_t k = cache_key("IMG_0001.jpg", 1000, 0x5964, 0x8000);
    expect("cache key stable", k == cache_key("IMG_0001.jpg", 1000, 0x5964, 0x8000), 1);
    expect("cache key follows the name", k != cache_key("IMG_0002.jpg", 1000, 0x5964, 0x8000), 1);
    expect("cache key follows the size", k != cache_key("IMG_0001.jpg", 1001, 0x5964, 0x8000), 1);
    expect("cache key follows the date", k != cache_key("IMG_0001.jpg", 1000, 0x5965, 0x8000), 1);
    expect("cache key follows the time", k != cache_key("IMG_0001.jpg", 1000, 0x5964, 0x8001), 1);

    // File names: written by cache_name(), read back by cache_parse_name()
    char name[CACHE_NAME_LEN + 1 + 4];
    static const uint64_t keys[] = {0, 1, 0xfedcba9876543210ULL, 0xffffffffffffffffULL};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    {
        cache_name(name, keys[i]);
        expect("cache name length", (int)strlen(name), CACHE_NAME_LEN);
        strcat(name, i % 2 ? ".tmp" : ".bmp");
        uint64_t back = 0;
        bool tmp = !(i % 2);
        expect("cache name parsed", cache_parse_name(name, &back, &tmp), 1);
        expect("cache name round trip", back == keys[i], 1);
        expect("cache name .tmp", tmp, i % 2);
    }
    static const char *const others[] = {"fedcba987654321.bmp", "fedcba9876543210.bmpx", "FEDCBA9876543210.bmp",
                                         "fedcba9876543210.jpg", "fedcba987654321g.bmp", "fedcba9876543210", ""};
    for (size_t i = 0; i < sizeof(others) / sizeof(others[0]); i++)
    {
        uint64_t key;
        bool tmp;
        char label[64];
        snprintf(label, sizeof(label), "not a cache name: \"%s\"", others[i]);
        expect(label, cache_parse_name(others[i], &key, &tmp), 0);
    }
}

int main(void)
{
    test_exif();
    test_enhance();
    test_cache_format();
    test_palette_replace();
    printf(failures ? "%d failed\n" : "unit tests passed\n", failures);
    return failures != 0;
}
