#include "cache_format.h"

#include <stdio.h>
#include <string.h>

#define CACHE_VERSION 2 // bump when the conversion's output changes
#define BARS_SHOWN_AS 128 // grey: the bar placeholder on a computer

static void put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
    put16(p, v);
    put16(p + 2, v >> 16);
}

// 64-bit FNV-1a
static uint64_t hash_bytes(uint64_t h, const void *data, size_t len)
{
    const uint8_t *p = data;
    for (size_t i = 0; i < len; i++)
        h = (h ^ p[i]) * 0x100000001b3ULL;
    return h;
}

uint64_t cache_key(const char *name, uint32_t size, uint16_t date, uint16_t time)
{
    uint8_t entry[1 + 4 + 2 + 2] = {CACHE_VERSION};
    put32(entry + 1, size);
    put16(entry + 5, date);
    put16(entry + 7, time);
    uint64_t h = hash_bytes(0xcbf29ce484222325ULL, name, strlen(name) + 1); // with the NUL: no ambiguity
    return hash_bytes(h, entry, sizeof(entry));
}

void cache_name(char out[CACHE_NAME_LEN + 1], uint64_t key)
{
    snprintf(out, CACHE_NAME_LEN + 1, "%08lx%08lx", (unsigned long)(key >> 32), (unsigned long)(key & 0xFFFFFFFF));
}

bool cache_parse_name(const char *file_name, uint64_t *key, bool *tmp)
{
    uint64_t k = 0;
    for (int i = 0; i < CACHE_NAME_LEN; i++)
    {
        char c = file_name[i];
        int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (digit < 0)
            return false;
        k = k << 4 | (uint64_t)digit;
    }
    const char *ext = file_name + CACHE_NAME_LEN;
    if (strcmp(ext, ".bmp") != 0 && strcmp(ext, ".tmp") != 0)
        return false;
    *key = k;
    *tmp = ext[1] == 't';
    return true;
}

void cache_header(uint8_t out[CACHE_HEADER_SIZE], const Palette *palette)
{
    memset(out, 0, CACHE_HEADER_SIZE);
    out[0] = 'B';
    out[1] = 'M';
    put32(out + 2, CACHE_FILE_SIZE);
    put32(out + 10, CACHE_HEADER_SIZE);
    put32(out + 14, 40);
    put32(out + 18, CACHE_WIDTH);
    put32(out + 22, (uint32_t)-CACHE_HEIGHT); // negative: top-down
    put16(out + 26, 1);
    put16(out + 28, 4);
    put32(out + 34, CACHE_ROW_BYTES * CACHE_HEIGHT);
    put32(out + 46, 16);
    for (int i = 0; i < palette->count; i++)
    {
        uint8_t *e = out + 54 + 4 * palette->colors[i].code; // B, G, R, 0
        e[0] = palette->colors[i].b;
        e[1] = palette->colors[i].g;
        e[2] = palette->colors[i].r;
    }
    memset(out + 54 + 4 * palette->bars, BARS_SHOWN_AS, 3);
}

bool cache_header_ok(const uint8_t header[CACHE_HEADER_SIZE])
{
    uint8_t want[CACHE_HEADER_SIZE];
    static const Palette none = {0};
    cache_header(want, &none);
    return memcmp(header, want, 54) == 0; // both headers, without the palette
}

static uint8_t swap_nibbles(uint8_t b)
{
    return (uint8_t)(b << 4 | b >> 4);
}

void cache_row_from_frame(uint8_t row[CACHE_ROW_BYTES], const uint8_t *frame, int y)
{
    const uint8_t *src = frame + (size_t)(CACHE_HEIGHT - 1 - y) * CACHE_ROW_BYTES;
    for (int i = 0; i < CACHE_ROW_BYTES; i++)
        row[i] = swap_nibbles(src[CACHE_ROW_BYTES - 1 - i]);
}

void cache_row_to_frame(uint8_t *frame, const uint8_t row[CACHE_ROW_BYTES], int y)
{
    uint8_t *dst = frame + (size_t)(CACHE_HEIGHT - 1 - y) * CACHE_ROW_BYTES;
    for (int i = 0; i < CACHE_ROW_BYTES; i++)
        dst[CACHE_ROW_BYTES - 1 - i] = swap_nibbles(row[i]);
}
