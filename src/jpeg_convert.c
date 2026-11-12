#include "jpeg_convert.h"
#include "exif.h"
#include "tjpgd.h"

#include <string.h>

// Start of APP1 searched for the orientation and the date; phone photos keep DateTimeOriginal past the
// first 512 bytes
#define EXIF_PROBE_BYTES 4096
#define IN_BUFFER_MAX (16 * 1024) // read-ahead, if the work memory has room

// Scaling runs on a grid of cells per axis, n = min(decoded size d, placed output size s):
// shrinking, a cell is one output pixel averaging its span of decoded pixels; enlarging, a cell
// is one decoded pixel repeated over its span of output pixels. Spans partition both ranges.
// Only cells [c0, c1) that hold visible output [vis0, vis1) are kept.
typedef struct
{
    uint32_t d, s, n;
    int vis0, vis1;
    int c0, c1;
} Axis;

typedef struct
{
    JDEC jd;
    const JpegInput *in;
    const JpegOutput *out;
    Axis ax, ay; // in stored orientation: x along decoded rows
    int orientation;
    int pw, ph; // photo size as viewed, after scaling
    int ox, oy; // its top-left corner in the frame (negative when cropped)
    uint32_t dw; // decoded width; a block ending there ends an MCU row
    uint8_t *ring; // RGB cell rows in flight, ring_rows of them
    int ring_rows;
    int16_t *err; // Floyd-Steinberg errors, RGB per output pixel plus one at each end
    int next_cy; // next cell row to dither
    bool failed;
    // Read-ahead from the rest of the work memory: TJpgDec reads 512 bytes at a time, and a card
    // takes milliseconds per read command however short
    uint8_t *inbuf;
    size_t in_size, in_pos, in_len;
} Conv;

static int src_start(const Axis *a, int c)
{
    return (int)((uint32_t)c * a->d / a->n);
}

static int cell_of_src(const Axis *a, int i)
{
    return (int)((((uint32_t)i + 1) * a->n - 1) / a->d);
}

static int out_start(const Axis *a, int c)
{
    return (int)((uint32_t)c * a->s / a->n);
}

static int cell_of_out(const Axis *a, int q)
{
    return (int)((((uint32_t)q + 1) * a->n - 1) / a->s);
}

static void setup_axis(Axis *a, uint32_t d, uint32_t s, int vis0, int vis1)
{
    a->d = d;
    a->s = s;
    a->n = d < s ? d : s;
    a->vis0 = vis0;
    a->vis1 = vis1;
    a->c0 = cell_of_out(a, vis0);
    a->c1 = cell_of_out(a, vis1 - 1) + 1;
}

// Size TJpgDec delivers at 1/2^k: each MCU is scaled on its own, so a partial last one rounds down
static uint32_t scaled_size(uint32_t size, uint32_t mcu, int k)
{
    return size / mcu * (mcu >> k) + ((size % mcu) >> k);
}

static int clamp255(int v)
{
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

static bool read_exact(const JpegInput *in, uint8_t *buf, size_t n)
{
    return in->read(in->ctx, buf, n) == n;
}

static bool is_sof(uint8_t m)
{
    return m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC;
}

JpegResult jpeg_probe(const JpegInput *in, JpegInfo *info, void *work, size_t work_size)
{
    uint8_t b[2];
    bool have_exif = false;
    info->width = info->height = 0;
    info->orientation = 1;
    info->date = (ExifDate){0};

    if (!read_exact(in, b, 2))
        return JPEG_READ_ERROR;
    if (b[0] != 0xFF || b[1] != 0xD8)
        return JPEG_BROKEN;
    for (;;)
    {
        if (!read_exact(in, b, 2))
            return JPEG_READ_ERROR;
        if (b[0] != 0xFF)
            return JPEG_BROKEN;
        uint8_t m = b[1];
        while (m == 0xFF) // fill bytes
            if (!read_exact(in, &m, 1))
                return JPEG_READ_ERROR;
        if (m == 0x00 || m == 0x01 || (m >= 0xD0 && m <= 0xDA)) // no length, or scan before a frame
            return JPEG_BROKEN;
        if (!read_exact(in, b, 2))
            return JPEG_READ_ERROR;
        size_t len = (size_t)(b[0] << 8 | b[1]);
        if (len < 2)
            return JPEG_BROKEN;
        len -= 2;

        if (m == 0xC0)
        {
            // Precision, height, width, component count, then per component id, sampling, table;
            // accepted as TJpgDec does, so the probe already tells which photos cannot be used
            uint8_t sof[6 + 3 * 3];
            if (len < 6)
                return JPEG_BROKEN;
            if (!read_exact(in, sof, 6))
                return JPEG_READ_ERROR;
            info->height = (uint16_t)(sof[1] << 8 | sof[2]);
            info->width = (uint16_t)(sof[3] << 8 | sof[4]);
            if (!info->width || !info->height || len != 6 + 3 * (size_t)sof[5])
                return JPEG_BROKEN;
            if (sof[5] != 1 && sof[5] != 3)
                return JPEG_UNSUPPORTED; // CMYK
            if (!read_exact(in, sof + 6, len - 6))
                return JPEG_READ_ERROR;
            uint8_t luma = sof[7];
            if (luma != 0x11 && luma != 0x21 && luma != 0x22)
                return JPEG_UNSUPPORTED;
            for (int i = 1; i < sof[5]; i++)
                if (sof[7 + 3 * i] != 0x11)
                    return JPEG_UNSUPPORTED;
            return JPEG_OK;
        }
        if (is_sof(m))
            return JPEG_UNSUPPORTED;
        if (m == 0xE1 && !have_exif)
        {
            size_t n = len < EXIF_PROBE_BYTES ? len : EXIF_PROBE_BYTES;
            if (n > work_size)
                n = work_size;
            if (!read_exact(in, work, n))
                return JPEG_READ_ERROR;
            if (n >= 6 && memcmp(work, "Exif\0\0", 6) == 0)
            {
                info->orientation = exif_orientation(work, n);
                exif_date(work, n, &info->date);
                have_exif = true;
            }
            len -= n;
        }
        if (len && in->read(in->ctx, NULL, len) != len)
            return JPEG_READ_ERROR;
    }
}

// convert_reference.py look, as Pillow does it: contrast 1.1 around mid grey (Pillow pivots on the
// image mean, unknown while streaming), then saturation 1.2 against luma; clipped after each step
static void enhance(const uint8_t rgb[3], int v[3])
{
    for (int ch = 0; ch < 3; ch++)
        v[ch] = clamp255(128 + ((rgb[ch] - 128) * 282 >> 8));
    int luma = (v[0] * 77 + v[1] * 150 + v[2] * 29) >> 8;
    for (int ch = 0; ch < 3; ch++)
        v[ch] = clamp255(luma + ((v[ch] - luma) * 307 >> 8));
}

// Map the pixel's brightness into the panel's black..white range, scaling R, G and B alike so the
// colour keeps its saturation
static void fit_range(const Palette *palette, int v[3])
{
    if (!palette->white_luma)
        return;
    int luma = (v[0] * 77 + v[1] * 150 + v[2] * 29) >> 8;
    if (!luma)
        return;
    int fitted = palette->black_luma + luma * (palette->white_luma - palette->black_luma) / 255;
    for (int ch = 0; ch < 3; ch++)
        v[ch] = clamp255(v[ch] * fitted / luma);
}

// Viewed frame position of output pixel (p, q), given in stored orientation
static void to_frame(const Conv *c, int p, int q, int *x, int *y)
{
    switch (c->orientation)
    {
    case 3:
        *x = c->pw - 1 - p;
        *y = c->ph - 1 - q;
        break;
    case 6:
        *x = c->pw - 1 - q;
        *y = p;
        break;
    case 8:
        *x = q;
        *y = c->ph - 1 - p;
        break;
    default:
        *x = p;
        *y = q;
        break;
    }
    *x += c->ox;
    *y += c->oy;
}

// Dither the output rows of cell row cy (stored orientation) and hand them to put_pixel
static void emit_cell_row(Conv *c, int cy)
{
    const Axis *ax = &c->ax, *ay = &c->ay;
    const uint8_t *row = c->ring + (size_t)((cy - ay->c0) % c->ring_rows) * (size_t)(ax->c1 - ax->c0) * 3;
    int q0 = out_start(ay, cy), q1 = out_start(ay, cy + 1);
    if (q0 < ay->vis0)
        q0 = ay->vis0;
    if (q1 > ay->vis1)
        q1 = ay->vis1;

    for (int q = q0; q < q1; q++)
    {
        int16_t *e = c->err; // e[3 * (i + 1)] belongs to output pixel vis0 + i, e[0] collects the left edge
        e[0] = e[1] = e[2] = 0;
        int right[3] = {0, 0, 0}; // error for the next pixel in this row
        int prev[3] = {0, 0, 0}; // error of the previous pixel, its share below-right is still due
        int cx = ax->c0;
        int next_cx_at = out_start(ax, cx + 1);
        for (int p = ax->vis0; p < ax->vis1; p++, e += 3)
        {
            while (p >= next_cx_at)
                next_cx_at = out_start(ax, ++cx + 1);
            const uint8_t *cell = row + (cx - ax->c0) * 3;

            int v[3];
            enhance(cell, v);
            fit_range(c->out->palette, v);
            for (int ch = 0; ch < 3; ch++)
                v[ch] = clamp255(v[ch] + e[3 + ch] + right[ch]);

            int shown[3];
            uint8_t code = palette_nearest(c->out->palette, v[0], v[1], v[2], &shown[0], &shown[1], &shown[2]);
            for (int ch = 0; ch < 3; ch++)
            {
                // 7/16 right, 3/16 below-left, 5/16 below, 1/16 below-right (the rest, so nothing is lost)
                int err = v[ch] - shown[ch];
                int r7 = err * 7 / 16, r3 = err * 3 / 16, r5 = err * 5 / 16;
                right[ch] = r7;
                e[ch] += r3; // below-left is complete now
                e[3 + ch] = r5 + prev[ch];
                prev[ch] = err - r7 - r3 - r5;
            }
            int x, y;
            to_frame(c, p, q, &x, &y);
            c->out->put_pixel(c->out->ctx, x, y, code);
        }
    }
}

static size_t jd_input(JDEC *jd, uint8_t *buf, size_t n)
{
    Conv *c = jd->device;
    if (!c->inbuf)
        return c->in->read(c->in->ctx, buf, n);
    size_t done = 0;
    while (done < n)
    {
        if (c->in_pos == c->in_len)
        {
            c->in_pos = 0;
            c->in_len = c->in->read(c->in->ctx, c->inbuf, c->in_size);
            if (c->in_len == 0)
                break;
        }
        size_t part = c->in_len - c->in_pos < n - done ? c->in_len - c->in_pos : n - done;
        if (buf)
            memcpy(buf + done, c->inbuf + c->in_pos, part);
        c->in_pos += part;
        done += part;
    }
    return done;
}

// One decoded block (an MCU at the decode scale), RGB888, arriving left to right, top to bottom
static int jd_output(JDEC *jd, void *bitmap, JRECT *rect)
{
    Conv *c = jd->device;
    const Axis *ax = &c->ax, *ay = &c->ay;
    const uint8_t *px = bitmap;
    int left = rect->left, right = rect->right, top = rect->top, bottom = rect->bottom;
    int w = right - left + 1;
    size_t row_bytes = (size_t)(ax->c1 - ax->c0) * 3;

    int x_first = left > src_start(ax, ax->c0) ? left : src_start(ax, ax->c0);
    int y_first = top > src_start(ay, ay->c0) ? top : src_start(ay, ay->c0);
    if (x_first <= right && x_first < src_start(ax, ax->c1) && y_first <= bottom && y_first < src_start(ay, ay->c1))
    {
        for (int cy = cell_of_src(ay, y_first); cy < ay->c1 && src_start(ay, cy) <= bottom; cy++)
        {
            if (cy - c->next_cy >= c->ring_rows)
            {
                c->failed = true; // more rows in flight than planned: a bug, never overrun the ring
                return 0;
            }
            int y0 = src_start(ay, cy), y1 = src_start(ay, cy + 1);
            int ya = y0 > top ? y0 : top, yb = y1 < bottom + 1 ? y1 : bottom + 1;
            int rows_before = (y1 < top ? y1 : top) - y0; // cell rows from earlier MCU rows
            if (rows_before < 0)
                rows_before = 0;
            uint8_t *row = c->ring + (size_t)((cy - ay->c0) % c->ring_rows) * row_bytes;

            for (int cx = cell_of_src(ax, x_first); cx < ax->c1 && src_start(ax, cx) <= right; cx++)
            {
                int x0 = src_start(ax, cx), x1 = src_start(ax, cx + 1);
                int xa = x0 > left ? x0 : left, xb = x1 < right + 1 ? x1 : right + 1;
                int cols_before = (x1 < left ? x1 : left) - x0; // cell columns from earlier blocks
                if (cols_before < 0)
                    cols_before = 0;
                uint32_t sum[3] = {0, 0, 0};
                for (int y = ya; y < yb; y++)
                {
                    const uint8_t *s = px + ((size_t)(y - top) * (size_t)w + (size_t)(xa - left)) * 3;
                    for (int x = xa; x < xb; x++, s += 3)
                    {
                        sum[0] += s[0];
                        sum[1] += s[1];
                        sum[2] += s[2];
                    }
                }
                uint32_t count = (uint32_t)((xb - xa) * (yb - ya));
                uint32_t before = (uint32_t)((x1 - x0) * rows_before + cols_before * (yb - ya));
                uint8_t *cell = row + (cx - ax->c0) * 3;
                for (int ch = 0; ch < 3; ch++)
                {
                    uint32_t total = before * cell[ch] + sum[ch];
                    cell[ch] = (uint8_t)((total + (before + count) / 2) / (before + count));
                }
            }
        }
    }

    if ((uint32_t)right + 1 == c->dw) // end of an MCU row: dither the cell rows it completed
    {
        while (c->next_cy < ay->c1 && src_start(ay, c->next_cy + 1) <= bottom + 1)
            emit_cell_row(c, c->next_cy++);
        if (c->out->progress)
            c->out->progress(c->out->ctx);
        if (c->next_cy == ay->c1)
            return 0; // everything visible is done: stop decoding
    }
    return 1;
}

static JpegResult from_jresult(JRESULT r)
{
    switch (r)
    {
    case JDR_OK:
        return JPEG_OK;
    case JDR_INP:
        return JPEG_READ_ERROR;
    case JDR_FMT1:
        return JPEG_BROKEN;
    default: // FMT2/FMT3; MEM1/MEM2 mean unusually large tables
        return JPEG_UNSUPPORTED;
    }
}

// Photo size as viewed after scaling: fill the frame if it has the frame's orientation, else fit
static bool place(int out_w, int out_h, uint32_t view_w, uint32_t view_h, int *pw, int *ph)
{
    bool fill = (view_w > view_h) == (out_w > out_h) && view_w != view_h;
    bool width_limits_fit = (uint64_t)out_w * view_h <= (uint64_t)out_h * view_w;
    uint64_t w, h;
    if (fill != width_limits_fit)
    {
        w = (uint64_t)out_w;
        h = ((uint64_t)view_h * (uint64_t)out_w + view_w / 2) / view_w;
    }
    else
    {
        h = (uint64_t)out_h;
        w = ((uint64_t)view_w * (uint64_t)out_h + view_h / 2) / view_h;
    }
    if (w == 0)
        w = 1;
    if (h == 0)
        h = 1;
    if (w > 65535 || h > 65535)
        return false;
    *pw = (int)w;
    *ph = (int)h;
    return true;
}

// Cell rows one MCU row of band_rows decoded rows can touch, i.e. the most in flight at once
static int ring_rows_for(const Axis *ay, uint32_t band_rows)
{
    uint32_t rows = (band_rows - 1) * ay->n / ay->d + 2;
    return rows < (uint32_t)(ay->c1 - ay->c0) ? (int)rows : ay->c1 - ay->c0;
}

static size_t ring_bytes(const Conv *c)
{
    return ((size_t)c->ring_rows * (size_t)(c->ax.c1 - c->ax.c0) * 3 + 3) & ~(size_t)3;
}

static size_t err_bytes(const Conv *c)
{
    return (size_t)(c->ax.vis1 - c->ax.vis0 + 2) * 3 * sizeof(int16_t);
}

JpegResult jpeg_convert(const JpegInput *in, const JpegOutput *out, void *work, size_t work_size, JpegInfo *info)
{
    JpegResult r = jpeg_probe(in, info, work, work_size);
    if (r != JPEG_OK)
        return r;
    if (!in->rewind(in->ctx))
        return JPEG_READ_ERROR;

    Conv c = {.in = in, .out = out, .orientation = info->orientation};
    JRESULT jr = jd_prepare(&c.jd, jd_input, work, work_size, &c);
    if (jr != JDR_OK)
        return from_jresult(jr);

    // Viewed and stored placed sizes, visible part of the photo in viewed photo coordinates
    bool turned = c.orientation == 6 || c.orientation == 8;
    uint32_t view_w = turned ? c.jd.height : c.jd.width, view_h = turned ? c.jd.width : c.jd.height;
    if (!place(out->width, out->height, view_w, view_h, &c.pw, &c.ph))
        return JPEG_TOO_SMALL;
    c.ox = (out->width - c.pw) / 2;
    c.oy = (out->height - c.ph) / 2;
    int vx0 = c.ox < 0 ? -c.ox : 0, vx1 = c.pw < out->width - c.ox ? c.pw : out->width - c.ox;
    int vy0 = c.oy < 0 ? -c.oy : 0, vy1 = c.ph < out->height - c.oy ? c.ph : out->height - c.oy;
    int sw = turned ? c.ph : c.pw, sh = turned ? c.pw : c.ph;
    int sx0, sx1, sy0, sy1; // the visible part in stored orientation
    switch (c.orientation)
    {
    case 3:
        sx0 = c.pw - vx1, sx1 = c.pw - vx0, sy0 = c.ph - vy1, sy1 = c.ph - vy0;
        break;
    case 6:
        sx0 = vy0, sx1 = vy1, sy0 = c.pw - vx1, sy1 = c.pw - vx0;
        break;
    case 8:
        sx0 = c.ph - vy1, sx1 = c.ph - vy0, sy0 = vx0, sy1 = vx1;
        break;
    default:
        sx0 = vx0, sx1 = vx1, sy0 = vy0, sy1 = vy1;
        break;
    }

    // Decode scale: the smallest decode that still covers the placed size (TJpgDec averages
    // 2^k x 2^k pixels and skips the IDCT at 1/8), coarser only if the rows in flight don't fit
    uint32_t mcu_w = c.jd.msx * 8u, mcu_h = c.jd.msy * 8u;
    int k = 3;
    while (k > 0 && (scaled_size(c.jd.width, mcu_w, k) < (uint32_t)sw || scaled_size(c.jd.height, mcu_h, k) < (uint32_t)sh))
        k--;
    size_t avail = c.jd.sz_pool & ~(size_t)3;
    for (;; k++)
    {
        if (k > 3)
            return JPEG_TOO_SMALL;
        c.dw = scaled_size(c.jd.width, mcu_w, k);
        setup_axis(&c.ax, c.dw, (uint32_t)sw, sx0, sx1);
        setup_axis(&c.ay, scaled_size(c.jd.height, mcu_h, k), (uint32_t)sh, sy0, sy1);
        if (!c.ax.d || !c.ay.d)
            continue;
        c.ring_rows = ring_rows_for(&c.ay, mcu_h >> k);
        if (ring_bytes(&c) + err_bytes(&c) <= avail)
            break;
    }
    c.ring = c.jd.pool;
    c.err = (int16_t *)(c.ring + ring_bytes(&c));
    memset(c.err, 0, err_bytes(&c));
    // Read-ahead: whole sectors, as TJpgDec's reads from here on start at a multiple of 512
    size_t left = avail - ring_bytes(&c) - err_bytes(&c);
    c.in_size = (left < IN_BUFFER_MAX ? left : IN_BUFFER_MAX) & ~(size_t)511;
    if (c.in_size)
        c.inbuf = (uint8_t *)c.err + ((err_bytes(&c) + 3) & ~(size_t)3);
    c.next_cy = c.ay.c0;

    // Bars around a photo that fits in whole
    for (int y = 0; y < out->height; y++)
        for (int x = 0; x < out->width; x++)
            if (x < c.ox || x >= c.ox + c.pw || y < c.oy || y >= c.oy + c.ph)
                out->put_pixel(out->ctx, x, y, out->palette->bars);

    jr = jd_decomp(&c.jd, jd_output, (uint8_t)k);
    if (c.failed)
        return JPEG_BROKEN;
    if (jr != JDR_OK && jr != JDR_INTR)
        return from_jresult(jr);
    return c.next_cy == c.ay.c1 ? JPEG_OK : JPEG_BROKEN;
}

const char *jpeg_result_str(JpegResult result)
{
    switch (result)
    {
    case JPEG_OK:
        return "ok";
    case JPEG_READ_ERROR:
        return "read error or file ends early";
    case JPEG_BROKEN:
        return "broken JPEG data";
    case JPEG_UNSUPPORTED:
        return "unsupported JPEG type (e.g. progressive)";
    case JPEG_TOO_SMALL:
        return "image too small or too narrow";
    }
    return "?";
}
