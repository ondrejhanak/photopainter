/*****************************************************************************
* | File      	:   display.c
* | Author      :   Waveshare team
* | Function    :   Image and message screens on the frame's e-Paper panel (panel.h)
* -----------------------------------------------------------------------------
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include "display.h"
#include "images.h"
#include "messages.h"
#include "sdcard.h"
#include "settings.h"
#include "panel.h"
#include "GUI_Paint.h"
#include "cache.h"
#include "jpeg_convert.h"
#include "palette.h"
#include "wake_log.h"
#include "work_memory.h"
#include "f_util.h"

#include <stdbool.h>
#include <string.h>

#define MESSAGE_X 125 // message screen text position, landscape
#define MESSAGE_Y 160
#define MESSAGE_PORTRAIT_X 30
#define MESSAGE_PORTRAIT_Y 300
#define NOTE_X 10 // note strip at the bottom of a photo
#define NOTE_PADDING 6
#define CORNER_MARGIN 10 // battery icon (top right, 50 x 28 px) and date box (bottom right) from the edges
#define MAX_TRIES 5 // photos tried per change: bounds the awake time, a broken big file decodes for long

// Draw in viewed frame coordinates: the panel is mounted upside down, so landscape is turned 180
// degrees and portrait (the frame turned counter-clockwise) 270. Paint.Width and Paint.Height are
// then the viewed size.
static void select_rotation(void)
{
    Paint_SetRotate(settings.orientation == ORIENTATION_PORTRAIT ? 270 : 180);
}

// Draw a message screen (title and a smaller hint below it) into the selected, cleared image
static void draw_message(const char *title, const char *hint)
{
    printf("Display message: %s\r\n", title);
    select_rotation();
    bool portrait = Paint.Width < Paint.Height;
    UWORD x = portrait ? MESSAGE_PORTRAIT_X : MESSAGE_X;
    UWORD y = Paint_DrawString(x, portrait ? MESSAGE_PORTRAIT_Y : MESSAGE_Y, title, &Font28, PANEL_BLACK,
                               PANEL_WHITE);
    Paint_DrawString(x, y + 14, hint, &Font22, PANEL_BLACK, PANEL_WHITE);
}

static size_t fat_read(void *ctx, uint8_t *buf, size_t n)
{
    FIL *fil = ctx;
    UINT br = 0;
    if (buf)
        return f_read(fil, buf, n, &br) == FR_OK ? br : 0;
    FSIZE_t from = f_tell(fil);
    if (f_lseek(fil, from + n) != FR_OK) // stops at the end of the file (opened for reading)
        return 0;
    return (size_t)(f_tell(fil) - from);
}

static bool fat_rewind(void *ctx)
{
    return f_lseek(ctx, 0) == FR_OK;
}

static void put_pixel(void *ctx, int x, int y, uint8_t code)
{
    (void)ctx;
    Paint_SetPixel((UWORD)x, (UWORD)y, code);
}

static void feed_watchdog(void *ctx)
{
    (void)ctx;
    watchdog_update();
}

static bool drawn_from_cache; // set by draw_jpeg(): the last photo came from its cache file

// A JPEG into the selected image (landscape frame): from its cache file, or decoded, scaled and
// dithered and then cached
static bool draw_jpeg(const char *path, uint8_t *image)
{
    uint32_t start = to_ms_since_boot(get_absolute_time());
    drawn_from_cache = cache_load(path, image, &PANEL_PALETTE);
    if (drawn_from_cache)
    {
        printf("jpeg: from cache, %lu ms\r\n", (unsigned long)(to_ms_since_boot(get_absolute_time()) - start));
        return true;
    }
    FIL fil;
    FRESULT fr = f_open(&fil, path, FA_READ);
    if (fr != FR_OK)
    {
        printf("f_open(%s) error: %s (%d)\r\n", path, FRESULT_str(fr), fr);
        return false;
    }
    select_rotation();
    JpegInput in = {.read = fat_read, .rewind = fat_rewind, .ctx = &fil};
    JpegOutput out = {.width = Paint.Width, .height = Paint.Height, .palette = &PANEL_PALETTE,
                      .put_pixel = put_pixel, .progress = feed_watchdog};
    uint32_t convert_start = to_ms_since_boot(get_absolute_time());
    JpegInfo info;
    JpegResult r = jpeg_convert(&in, &out, work_memory, sizeof(work_memory), &info);
    uint32_t now = to_ms_since_boot(get_absolute_time()), ms = now - start;
    if (r == JPEG_OK)
    {
        wake_log.convert_ms += now - convert_start;
        wake_log.convert_px += (uint32_t)info.width * info.height;
    }
    printf("jpeg: %s, %lu ms, read %lu of %lu bytes\r\n", jpeg_result_str(r), (unsigned long)ms,
           (unsigned long)f_tell(&fil), (unsigned long)f_size(&fil));
    f_close(&fil);
    if (r != JPEG_OK)
        return false;
    start = to_ms_since_boot(get_absolute_time());
    if (cache_save(path, image, &PANEL_PALETTE))
        printf("jpeg: cached, %lu ms\r\n", (unsigned long)(to_ms_since_boot(get_absolute_time()) - start));
    return true;
}

// The panel's frame buffer, two pixels per byte. Static, so a build without the RAM for it fails to
// link instead of failing to draw: next to it the RP2040 keeps only a few KB of heap.
static UBYTE frame[(PANEL_WIDTH + 1) / 2 * PANEL_HEIGHT];

// Set by display_image() for panel_refreshing(): the next image to convert and the frame buffer
static char prefetch_name[FF_LFN_BUF + 1];
static uint8_t *prefetch_image;

// While the panel refreshes an image, convert the image due next if it has no cache file yet, in
// the frame buffer the panel no longer needs
void panel_refreshing(void)
{
    if (!prefetch_image)
        return;
    uint8_t *image = prefetch_image;
    prefetch_image = NULL;
    if (cache_exists(prefetch_name))
        return;
    printf("converting %s during the refresh\r\n", prefetch_name);
    draw_jpeg(prefetch_name, image);
}

// Names of the photos that failed in this change, joined with ", "
static char failed[MAX_TRIES * (FF_LFN_BUF + 2) + 1];

// White strip at the bottom of the frame: the title, then the failed names. Only one line of names
// fits above the edge, longer lists are cut at a word.
static void draw_failed_note(const char *date)
{
    printf("Display note: %s\r\n", failed);
    select_rotation();
    UWORD top = Paint.Height - 2 * Font22.Height - 2 * NOTE_PADDING;
    Paint_ClearWindows(0, top, Paint.Width, Paint.Height, PANEL_WHITE);
    UWORD y = Paint_DrawString(NOTE_X, top + NOTE_PADDING, msg(MSG_CANNOT_SHOW_TITLE), &Font22,
                               PANEL_BLACK, PANEL_WHITE);
    Paint_DrawString(NOTE_X, y, failed, &Font22, PANEL_BLACK, PANEL_WHITE);
    if (date[0])
    { // at the right end of the title line, where the date box would be
        printf("Display date: %s\r\n", date);
        Paint_DrawString(Paint.Width - NOTE_X - strlen(date) * Font22.Width, top + NOTE_PADDING, date, &Font22,
                         PANEL_BLACK, PANEL_WHITE);
    }
}

// White box with the date in the bottom-right corner (the date is ASCII: one character per cell)
static void draw_date(const char *date)
{
    printf("Display date: %s\r\n", date);
    select_rotation();
    UWORD w = strlen(date) * Font22.Width + 2 * NOTE_PADDING, h = Font22.Height + 2 * NOTE_PADDING;
    UWORD x = Paint.Width - CORNER_MARGIN - w, y = Paint.Height - CORNER_MARGIN - h;
    Paint_ClearWindows(x, y, x + w, y + h, PANEL_WHITE);
    Paint_DrawString(x + NOTE_PADDING, y + NOTE_PADDING, date, &Font22, PANEL_BLACK, PANEL_WHITE);
}

// When the photo images_path was taken, written for the language; empty if its EXIF has no date.
// Read from the JPEG's header each time, so the cache files carry no date. Uses the work memory.
static void photo_date(char *out, size_t len)
{
    out[0] = '\0';
    FIL fil;
    if (f_open(&fil, images_path, FA_READ) != FR_OK)
        return;
    JpegInput in = {.read = fat_read, .rewind = fat_rewind, .ctx = &fil};
    JpegInfo info;
    jpeg_probe(&in, &info, work_memory, sizeof(work_memory)); // the date is kept when it stops later
    f_close(&fil);
    if (info.date.year)
        msg_date(out, len, info.date.year, info.date.month, info.date.day);
}

// Small battery with a little red charge on a white box, so it shows on any photo
static void draw_battery_icon(void)
{
    printf("Display battery icon\r\n");
    select_rotation();
    const UWORD x = Paint.Width - CORNER_MARGIN - 50, y = CORNER_MARGIN;
    Paint_ClearWindows(x, y, x + 50, y + 28, PANEL_WHITE);
    Paint_ClearWindows(x + 6, y + 6, x + 40, y + 22, PANEL_BLACK); // body, 2 px outline
    Paint_ClearWindows(x + 8, y + 8, x + 38, y + 20, PANEL_WHITE);
    Paint_ClearWindows(x + 40, y + 10, x + 44, y + 18, PANEL_BLACK); // terminal
    Paint_ClearWindows(x + 9, y + 9, x + 19, y + 19, PANEL_RED);
}

// Draw images_path into the image; a photo that fails is saved as shown, so the order moves past
// it, and the next one is tried. Stops after MAX_TRIES or when the first failed photo comes round
// again (a card with few photos). Expects the card mounted and leaves it mounted.
static bool draw_photo(uint8_t *image)
{
    char first_failed[FF_LFN_BUF + 1];
    failed[0] = '\0';
    wake_log_step(STEP_LOAD);
    for (int tries = 1;; tries++)
    {
        Paint_Clear(PANEL_WHITE);
        if (draw_jpeg(images_path, image))
        {
            strcpy(wake_log.photo, images_path);
            wake_log.result = drawn_from_cache ? "cache" : "converted";
            return true;
        }
        wake_log.failed++;
        printf("cannot show %s\r\n", images_path);
        if (tries == 1)
            strcpy(first_failed, images_path);
        else
            strcat(failed, ", ");
        strcat(failed, images_path);
        sdcard_unmount(); // both below mount the card themselves
        images_save_shown();
        bool next = tries < MAX_TRIES && images_select_next() && strcmp(images_path, first_failed) != 0;
        sdcard_mount();
        if (!next)
            return false;
    }
}

void display_image(bool battery_low)
{
    printf("e-Paper Init and Clear...\r\n");
    panel_init();

    UBYTE *image = frame;
    printf("Paint_NewImage\r\n");
    Paint_NewImage(image, PANEL_WIDTH, PANEL_HEIGHT, 0, PANEL_WHITE);
    Paint_SetScale(7);

    sdcard_mount();
    Paint_SelectImage(image);

    bool shown = draw_photo(image);
    if (shown)
    {
        // The panel must never get the placeholder: an unused code leaves an afterimage
        palette_replace(image, sizeof(frame), PANEL_PALETTE.bars,
                        settings.bars == BARS_BLACK ? PANEL_BLACK : PANEL_WHITE);
        char date[24] = "";
        if (settings.show_date)
            photo_date(date, sizeof(date));
        if (battery_low)
            draw_battery_icon();
        if (failed[0])
            draw_failed_note(date);
        else if (date[0])
            draw_date(date);
        if (images_peek_next(prefetch_name))
            prefetch_image = image;
    }
    else
    { // every try failed, all of them already saved as shown
        Paint_Clear(PANEL_WHITE);
        draw_message(msg(MSG_CANNOT_SHOW_TITLE), failed);
        wake_log.result = "message";
    }

    wake_log_step(STEP_REFRESH);
    printf("EPD_Display\r\n");
    uint32_t start = to_ms_since_boot(get_absolute_time());
    panel_display(image); // runs panel_refreshing()
    prefetch_image = NULL;
    printf("refresh: %lu ms\r\n", (unsigned long)(to_ms_since_boot(get_absolute_time()) - start));

    wake_log_step(STEP_SAVE);
    sdcard_unmount();
    if (shown)
        images_save_shown();

    printf("Goto Sleep...\r\n\r\n");
    panel_sleep();
}

void display_message(const char *title, const char *hint)
{
    printf("e-Paper Init and Clear...\r\n");
    panel_init();

    UBYTE *image = frame;
    printf("Paint_NewImage\r\n");
    Paint_NewImage(image, PANEL_WIDTH, PANEL_HEIGHT, 0, PANEL_WHITE);
    Paint_SetScale(7);

    Paint_SelectImage(image);
    Paint_Clear(PANEL_WHITE);
    draw_message(title, hint);
    wake_log.result = "message";

    wake_log_step(STEP_REFRESH);
    printf("EPD_Display\r\n");
    panel_display(image);

    printf("Goto Sleep...\r\n\r\n");
    panel_sleep();
}
