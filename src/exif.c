#include "exif.h"

#include <string.h>

#define TAG_ORIENTATION 0x0112
#define TAG_DATE_TIME 0x0132 // in IFD0: when the file was last changed
#define TAG_EXIF_IFD 0x8769 // in IFD0: offset of the Exif IFD
#define TAG_DATE_TIME_ORIGINAL 0x9003 // in the Exif IFD: when the photo was taken
#define TYPE_ASCII 2
#define TYPE_SHORT 3
#define TYPE_LONG 4

// TIFF data of an APP1 payload; offsets in it count from its start
typedef struct
{
    const uint8_t *data;
    size_t len;
    int big_endian;
} Tiff;

static uint32_t get16(const uint8_t *p, int big_endian)
{
    return big_endian ? (uint32_t)(p[0] << 8 | p[1]) : (uint32_t)(p[1] << 8 | p[0]);
}

static uint32_t get32(const uint8_t *p, int big_endian)
{
    return big_endian ? get16(p, 1) << 16 | get16(p + 2, 1) : get16(p + 2, 0) << 16 | get16(p, 0);
}

static bool open_tiff(const uint8_t *app1, size_t len, Tiff *t)
{
    if (len < 6 + 8 || memcmp(app1, "Exif\0\0", 6) != 0)
        return false;
    t->data = app1 + 6;
    t->len = len - 6;
    if (memcmp(t->data, "MM\0\x2a", 4) == 0)
        t->big_endian = 1;
    else if (memcmp(t->data, "II\x2a\0", 4) == 0)
        t->big_endian = 0;
    else
        return false;
    return true;
}

// The 12-byte entry of tag in the IFD at offset ifd; NULL if absent, or if the IFD runs past the data
// before it is found
static const uint8_t *find_tag(const Tiff *t, uint32_t ifd, uint32_t tag)
{
    if (ifd > t->len - 2)
        return NULL;
    uint32_t count = get16(t->data + ifd, t->big_endian);
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t entry = ifd + 2 + i * 12;
        if (t->len < 12 || entry > t->len - 12) // short data: t->len - 12 would wrap around
            return NULL;
        const uint8_t *e = t->data + entry;
        if (get16(e, t->big_endian) == tag)
            return e;
    }
    return NULL;
}

int exif_orientation(const uint8_t *app1, size_t len)
{
    Tiff t;
    if (!open_tiff(app1, len, &t))
        return 1;
    const uint8_t *e = find_tag(&t, get32(t.data + 4, t.big_endian), TAG_ORIENTATION);
    if (!e || get16(e + 2, t.big_endian) != TYPE_SHORT || get32(e + 4, t.big_endian) != 1)
        return 1;
    uint32_t value = get16(e + 8, t.big_endian); // a single SHORT sits at the start of the value field
    return value == 3 || value == 6 || value == 8 ? (int)value : 1;
}

static int digits(const uint8_t *p, int n)
{
    int v = 0;
    for (int i = 0; i < n; i++)
    {
        if (p[i] < '0' || p[i] > '9')
            return -1;
        v = v * 10 + (p[i] - '0');
    }
    return v;
}

// "YYYY:MM:DD HH:MM:SS" of a date entry; false if missing, outside the data or not a real date
// (cameras without a set clock write zeros)
static bool read_date(const Tiff *t, const uint8_t *e, ExifDate *date)
{
    if (!e || get16(e + 2, t->big_endian) != TYPE_ASCII || get32(e + 4, t->big_endian) < 11)
        return false;
    uint32_t at = get32(e + 8, t->big_endian); // longer than 4 bytes: stored at this offset
    if (at > t->len - 10)
        return false;
    const uint8_t *s = t->data + at;
    int year = digits(s, 4), month = digits(s + 5, 2), day = digits(s + 8, 2);
    if (s[4] != ':' || s[7] != ':' || year < 1800 || month < 1 || month > 12 || day < 1 || day > 31)
        return false;
    *date = (ExifDate){.year = (uint16_t)year, .month = (uint8_t)month, .day = (uint8_t)day};
    return true;
}

bool exif_date(const uint8_t *app1, size_t len, ExifDate *date)
{
    Tiff t;
    if (!open_tiff(app1, len, &t))
        return false;
    uint32_t ifd0 = get32(t.data + 4, t.big_endian);
    const uint8_t *exif = find_tag(&t, ifd0, TAG_EXIF_IFD);
    if (exif && get16(exif + 2, t.big_endian) == TYPE_LONG &&
        read_date(&t, find_tag(&t, get32(exif + 8, t.big_endian), TAG_DATE_TIME_ORIGINAL), date))
        return true;
    return read_date(&t, find_tag(&t, ifd0, TAG_DATE_TIME), date);
}
