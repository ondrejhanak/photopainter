// Runs the on-device JPEG conversion on the Mac: writes the frame as the 4-bit cache BMP and
// reports the result, the photo's headers and the work memory used. Build with build.sh.
#include "cache_format.h"
#include "jpeg_convert.h"
#include "palette.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    int width, height;
    uint8_t *codes;
    uint8_t *writes; // per pixel, to check that each is set exactly once
    uint8_t panel[CACHE_ROW_BYTES * CACHE_HEIGHT]; // landscape: the panel frame buffer, as on the device
} Frame;

static unsigned read_calls; // reads with data, i.e. card commands on the device

static size_t file_read(void *ctx, uint8_t *buf, size_t n)
{
    FILE *f = ctx;
    if (buf)
    {
        read_calls++;
        return fread(buf, 1, n, f);
    }
    long pos = ftell(f);
    fseek(f, 0, SEEK_END);
    long end = ftell(f);
    size_t skip = (size_t)(end - pos) < n ? (size_t)(end - pos) : n;
    fseek(f, pos + (long)skip, SEEK_SET);
    return skip;
}

static bool file_rewind(void *ctx)
{
    return fseek(ctx, 0, SEEK_SET) == 0;
}

static void frame_put(void *ctx, int x, int y, uint8_t code)
{
    Frame *f = ctx;
    if (x < 0 || y < 0 || x >= f->width || y >= f->height)
    {
        fprintf(stderr, "pixel outside the frame: %d, %d\n", x, y);
        exit(3);
    }
    f->codes[y * f->width + x] = code;
    f->writes[y * f->width + x]++;
    if (f->width == CACHE_WIDTH && f->height == CACHE_HEIGHT)
    { // as Paint_SetPixel() with rotation 180 on the device
        int px = CACHE_WIDTH - 1 - x, py = CACHE_HEIGHT - 1 - y;
        uint8_t *b = &f->panel[py * CACHE_ROW_BYTES + px / 2];
        *b = px % 2 ? (uint8_t)((*b & 0xF0) | code) : (uint8_t)((*b & 0x0F) | code << 4);
    }
}

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

// The landscape frame as the device's cache file
static int write_cache(const char *path, const Frame *f)
{
    FILE *out = fopen(path, "wb");
    if (!out)
        return -1;
    uint8_t header[CACHE_HEADER_SIZE], row[CACHE_ROW_BYTES];
    cache_header(header, &palette_7color);
    fwrite(header, 1, sizeof(header), out);
    for (int y = 0; y < CACHE_HEIGHT; y++)
    {
        cache_row_from_frame(row, f->panel, y);
        fwrite(row, 1, sizeof(row), out);
    }
    return fclose(out);
}

// Other frame sizes: 4-bit indexed, top-down (negative height), palette index = panel code
static int write_bmp(const char *path, const Frame *f, const Palette *palette)
{
    uint8_t h[118] = {0};
    uint32_t stride = ((uint32_t)f->width * 4 + 31) / 32 * 4;
    uint32_t size = stride * (uint32_t)f->height;
    h[0] = 'B';
    h[1] = 'M';
    put32(h + 2, sizeof(h) + size);
    put32(h + 10, sizeof(h));
    put32(h + 14, 40);
    put32(h + 18, (uint32_t)f->width);
    put32(h + 22, (uint32_t)-f->height);
    put16(h + 26, 1);
    put16(h + 28, 4);
    put32(h + 34, size);
    put32(h + 46, 16);
    for (int i = 0; i < palette->count; i++)
    {
        uint8_t *e = h + 54 + 4 * palette->colors[i].code; // BGR0
        e[0] = palette->colors[i].b;
        e[1] = palette->colors[i].g;
        e[2] = palette->colors[i].r;
    }
    FILE *out = fopen(path, "wb");
    if (!out)
        return -1;
    fwrite(h, 1, sizeof(h), out);
    uint8_t *row = calloc(stride, 1);
    for (int y = 0; y < f->height; y++)
    {
        for (int x = 0; x < f->width; x++)
        {
            uint8_t c = f->codes[y * f->width + x] & 0x0F;
            row[x / 2] = x % 2 ? (uint8_t)(row[x / 2] | c) : (uint8_t)(c << 4);
        }
        fwrite(row, 1, stride, out);
    }
    free(row);
    return fclose(out);
}

// 8-bit greyscale PGM, for frames converted with the grey palette
static int write_pgm(const char *path, const Frame *f)
{
    FILE *out = fopen(path, "wb");
    if (!out)
        return -1;
    fprintf(out, "P5\n%d %d\n255\n", f->width, f->height);
    fwrite(f->codes, 1, (size_t)f->width * (size_t)f->height, out);
    return fclose(out);
}

// 256 greys, code = level: dithering is exact, so the frame shows the scaled photo itself
static PaletteColor grey_colors[256];
static const Palette grey_palette = {.colors = grey_colors, .count = 256, .white = 255};

int main(int argc, char **argv)
{
    int width = 800, height = 480;
    bool grey = false;
    size_t work_size = JPEG_WORK_SIZE;
    int arg = 1;
    for (; arg < argc && argv[arg][0] == '-'; arg++)
    {
        if (strcmp(argv[arg], "-p") == 0)
            width = 480, height = 800;
        else if (strcmp(argv[arg], "-g") == 0)
            grey = true;
        else if (strcmp(argv[arg], "-w") == 0 && arg + 1 < argc)
            work_size = strtoul(argv[++arg], NULL, 0);
        else
            break;
    }
    if (argc - arg < 1 || argc - arg > 2)
    {
        fprintf(stderr, "usage: %s [-p] [-g] [-w work_bytes] in.jpg [out.bmp]\n"
                        "  -p  portrait frame (480x800)\n"
                        "  -g  256-grey palette, no dither noise; writes a PGM\n", argv[0]);
        return 2;
    }
    FILE *in = fopen(argv[arg], "rb");
    if (!in)
    {
        perror(argv[arg]);
        return 2;
    }

    // Filled with a pattern, so the untouched tail shows how much work memory was used
    uint8_t *work = malloc(work_size);
    memset(work, 0xA5, work_size);
    JpegInput input = {.read = file_read, .rewind = file_rewind, .ctx = in};
    static Frame frame;
    frame = (Frame){.width = width, .height = height,
                    .codes = calloc((size_t)width * (size_t)height, 1),
                    .writes = calloc((size_t)width * (size_t)height, 1)};
    for (int i = 0; i < 256; i++)
        grey_colors[i] = (PaletteColor){(uint8_t)i, (uint8_t)i, (uint8_t)i, (uint8_t)i};
    JpegOutput output = {.width = width, .height = height, .palette = grey ? &grey_palette : &palette_7color,
                         .put_pixel = frame_put, .ctx = &frame};

    JpegInfo info;
    JpegResult r = jpeg_probe(&input, &info, work, work_size);
    printf("probe: %s, %ux%u, orientation %d\n", jpeg_result_str(r), info.width, info.height, info.orientation);
    file_rewind(in);
    memset(work, 0xA5, work_size);
    read_calls = 0;
    r = jpeg_convert(&input, &output, work, work_size);
    size_t used = work_size;
    while (used > 0 && work[used - 1] == 0xA5)
        used--;
    long read_to = ftell(in);
    fseek(in, 0, SEEK_END);
    printf("convert: %s, work memory %zu of %zu bytes, read %ld of %ld bytes in %u calls\n", jpeg_result_str(r),
           used, work_size, read_to, ftell(in), read_calls);

    int status = r == JPEG_OK ? 0 : 1;
    if (r == JPEG_OK)
    {
        size_t missing = 0, twice = 0;
        for (size_t i = 0; i < (size_t)width * (size_t)height; i++)
        {
            missing += frame.writes[i] == 0;
            twice += frame.writes[i] > 1;
        }
        if (missing || twice)
        {
            printf("frame: %zu pixels not set, %zu set more than once\n", missing, twice);
            status = 3;
        }
        if (argc - arg == 2 && (grey ? write_pgm(argv[arg + 1], &frame)
                                   : width == CACHE_WIDTH ? write_cache(argv[arg + 1], &frame)
                                                          : write_bmp(argv[arg + 1], &frame, &palette_7color)) != 0)
        {
            perror(argv[arg + 1]);
            status = 2;
        }
    }
    fclose(in);
    free(work);
    free(frame.codes);
    free(frame.writes);
    return status;
}
