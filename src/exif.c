#include "exif.h"

#include <string.h>

#define TAG_ORIENTATION 0x0112
#define TYPE_SHORT 3

static uint32_t get16(const uint8_t *p, int big_endian)
{
    return big_endian ? (uint32_t)(p[0] << 8 | p[1]) : (uint32_t)(p[1] << 8 | p[0]);
}

static uint32_t get32(const uint8_t *p, int big_endian)
{
    return big_endian ? get16(p, 1) << 16 | get16(p + 2, 1) : get16(p + 2, 0) << 16 | get16(p, 0);
}

int exif_orientation(const uint8_t *app1, size_t len)
{
    if (len < 6 + 8 || memcmp(app1, "Exif\0\0", 6) != 0)
        return 1;
    const uint8_t *tiff = app1 + 6; // offsets in the TIFF data count from here
    size_t tiff_len = len - 6;
    int big_endian;
    if (memcmp(tiff, "MM\0\x2a", 4) == 0)
        big_endian = 1;
    else if (memcmp(tiff, "II\x2a\0", 4) == 0)
        big_endian = 0;
    else
        return 1;

    uint32_t ifd = get32(tiff + 4, big_endian);
    if (ifd > tiff_len - 2)
        return 1;
    uint32_t count = get16(tiff + ifd, big_endian);
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t entry = ifd + 2 + i * 12;
        if (entry > tiff_len - 12)
            return 1;
        const uint8_t *e = tiff + entry;
        if (get16(e, big_endian) != TAG_ORIENTATION)
            continue;
        if (get16(e + 2, big_endian) != TYPE_SHORT || get32(e + 4, big_endian) != 1)
            return 1;
        uint32_t value = get16(e + 8, big_endian); // a single SHORT sits at the start of the value field
        return value == 3 || value == 6 || value == 8 ? (int)value : 1;
    }
    return 1;
}
