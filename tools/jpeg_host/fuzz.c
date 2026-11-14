// Fuzz target: any bytes through the frame's photo path (jpeg_probe, jpeg_convert) on both panels and
// orientations, and its EXIF segment through the EXIF parser on its own. Memory errors and undefined
// behaviour stop it (ASan, UBSan), as do a pixel outside the frame, a frame left incomplete or an
// impossible EXIF value. Built by fuzz.sh, with libFuzzer or as a corpus replay.
#include "jpeg_convert.h"
#include "palette.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    const uint8_t *data;
    size_t size, pos;
} Source;

typedef struct
{
    int width, height;
    bool valid[256]; // panel codes a pixel may have: the palette's and its bar placeholder
    uint8_t writes[800 * 480];
} Frame;

static size_t source_read(void *ctx, uint8_t *buf, size_t n)
{
    Source *s = ctx;
    size_t left = s->size - s->pos;
    if (n > left)
        n = left;
    if (buf)
        memcpy(buf, s->data + s->pos, n);
    s->pos += n;
    return n;
}

static bool source_rewind(void *ctx)
{
    ((Source *)ctx)->pos = 0;
    return true;
}

static void fail(const char *what)
{
    fprintf(stderr, "fuzz: %s\n", what);
    abort();
}

static void frame_put(void *ctx, int x, int y, uint8_t code)
{
    Frame *f = ctx;
    if (x < 0 || y < 0 || x >= f->width || y >= f->height)
        fail("pixel outside the frame");
    if (!f->valid[code])
        fail("colour code not in the palette");
    f->writes[y * f->width + x]++;
}

static void progress(void *ctx)
{
    (void)ctx; // the firmware feeds the watchdog here
}

static void convert(const uint8_t *data, size_t size, int width, int height, const Palette *palette)
{
    static Frame frame;
    frame.width = width;
    frame.height = height;
    memset(frame.valid, 0, sizeof(frame.valid));
    for (int i = 0; i < palette->count; i++)
        frame.valid[palette->colors[i].code] = true;
    frame.valid[palette->bars] = true;
    memset(frame.writes, 0, sizeof(frame.writes));
    // Exactly the firmware's work memory, so ASan catches any use beyond it
    uint8_t *work = malloc(JPEG_WORK_SIZE);
    Source source = {.data = data, .size = size};
    JpegInput in = {.read = source_read, .rewind = source_rewind, .ctx = &source};
    JpegOutput out = {.width = width, .height = height, .palette = palette, .put_pixel = frame_put,
                      .progress = progress, .ctx = &frame};
    JpegInfo info;
    jpeg_probe(&in, &info, work, JPEG_WORK_SIZE);
    source_rewind(&source);
    JpegResult r = jpeg_convert(&in, &out, work, JPEG_WORK_SIZE, &info);
    if (r > JPEG_TOO_SMALL)
        fail("unknown result");
    if (r == JPEG_OK)
        for (int i = 0; i < width * height; i++)
            if (frame.writes[i] != 1)
                fail("converted frame with a pixel not set exactly once");
    free(work);
}

// The first APP1 segment's payload, in a buffer of exactly its size: inside jpeg_convert it sits in the
// larger work memory, where ASan cannot see a read past its end
static void parse_exif(const uint8_t *data, size_t size)
{
    for (size_t i = 0; i + 4 <= size; i++)
        if (data[i] == 0xFF && data[i + 1] == 0xE1)
        {
            size_t len = (size_t)(data[i + 2] << 8 | data[i + 3]);
            len = len < 2 ? 0 : len - 2;
            if (len > size - i - 4)
                len = size - i - 4;
            uint8_t *app1 = malloc(len ? len : 1);
            memcpy(app1, data + i + 4, len);
            int orientation = exif_orientation(app1, len);
            if (orientation != 1 && orientation != 3 && orientation != 6 && orientation != 8)
                fail("orientation not 1, 3, 6 or 8");
            ExifDate date;
            if (exif_date(app1, len, &date) && (date.month < 1 || date.month > 12 || date.day < 1 || date.day > 31))
                fail("impossible date");
            free(app1);
            return;
        }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    parse_exif(data, size);
    convert(data, size, 800, 480, &palette_7color);
    convert(data, size, 480, 800, &palette_6color);
    return 0;
}

#ifdef FUZZ_REPLAY
// Without libFuzzer: run each file given, e.g. the corpus, once
int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        FILE *f = fopen(argv[i], "rb");
        if (!f)
        {
            perror(argv[i]);
            return 2;
        }
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        uint8_t *data = malloc(size > 0 ? (size_t)size : 1);
        size_t got = fread(data, 1, (size_t)size, f);
        fclose(f);
        LLVMFuzzerTestOneInput(data, got);
        free(data);
    }
    printf("replayed %d inputs\n", argc - 1);
    return 0;
}
#endif
