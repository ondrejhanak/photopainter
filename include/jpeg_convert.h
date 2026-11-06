#ifndef JPEG_CONVERT_H
#define JPEG_CONVERT_H

#include "exif.h"
#include "palette.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Work memory for one conversion: TJpgDec pool, row buffers and dither errors; the rest is read-ahead
#define JPEG_WORK_SIZE (48 * 1024)

typedef enum
{
    JPEG_OK,
    JPEG_READ_ERROR, // input ended early or could not be read
    JPEG_BROKEN, // not a JPEG, or damaged data
    JPEG_UNSUPPORTED, // progressive, arithmetic coded, CMYK or unusual chroma subsampling
    JPEG_TOO_SMALL, // would need more work memory than given (tiny or extremely narrow image)
} JpegResult;

// Byte source: read(ctx, NULL, n) skips n bytes; returns the bytes read or skipped.
// rewind(ctx) starts over at the first byte.
typedef struct
{
    size_t (*read)(void *ctx, uint8_t *buf, size_t n);
    bool (*rewind)(void *ctx);
    void *ctx;
} JpegInput;

typedef struct
{
    uint16_t width, height; // as stored, before orientation
    int orientation; // EXIF: 1, 3, 6 or 8
    ExifDate date; // when the photo was taken; year 0 if unknown
} JpegInfo;

typedef struct
{
    int width, height; // frame size as viewed: 800x480 landscape, 480x800 portrait
    const Palette *palette;
    // One output pixel, in viewed frame coordinates; every pixel of the frame is set exactly once
    void (*put_pixel)(void *ctx, int x, int y, uint8_t code);
    void (*progress)(void *ctx); // called regularly during decoding (watchdog); may be NULL
    void *ctx;
} JpegOutput;

// Read the headers up to the frame header: size and orientation, no decoding
JpegResult jpeg_probe(const JpegInput *in, JpegInfo *info, void *work, size_t work_size);

// Decode, scale and dither a JPEG onto the frame: a photo in the frame's orientation fills it
// (centre crop), any other photo fits in whole between bars (Palette.bars)
JpegResult jpeg_convert(const JpegInput *in, const JpegOutput *out, void *work, size_t work_size);

const char *jpeg_result_str(JpegResult result);

#endif
