/*****************************************************************************
* | File      	:   display.c
* | Author      :   Waveshare team
* | Function    :   Image and message screens on the 7.3" e-Paper (F) panel
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
#include "EPD_7in3f.h"
#include "GUI_Paint.h"
#include "cache.h"
#include "jpeg_convert.h"
#include "palette.h"
#include "work_memory.h"
#include "f_util.h"

#include <stdbool.h>
#include <stdlib.h> // malloc() free()
#include <string.h>

#define MESSAGE_X 125 // message screen text position
#define MESSAGE_Y 160
#define NOTE_X 10 // note strip at the bottom of a photo
#define NOTE_PADDING 6
#define MAX_TRIES 5 // photos tried per change: bounds the awake time, a broken big file decodes for long

// Draw a message screen (title and a smaller hint below it) into the selected, cleared image
static void draw_message(const char *title, const char *hint)
{
    printf("Display message: %s\r\n", title);
    Paint_SetRotate(180);
    UWORD y = Paint_DrawString_EN(MESSAGE_X, MESSAGE_Y, title, &Font28, EPD_7IN3F_WHITE, EPD_7IN3F_BLACK);
    Paint_DrawString_EN(MESSAGE_X, y + 14, hint, &Font22, EPD_7IN3F_WHITE, EPD_7IN3F_BLACK);
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

// A JPEG into the selected image (landscape frame): from its cache file, or decoded, scaled and
// dithered and then cached
static bool draw_jpeg(const char *path, uint8_t *image)
{
    uint32_t start = to_ms_since_boot(get_absolute_time());
    if (cache_load(path, image))
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
    Paint_SetRotate(180); // the panel is mounted upside down
    JpegInput in = {.read = fat_read, .rewind = fat_rewind, .ctx = &fil};
    JpegOutput out = {.width = EPD_7IN3F_WIDTH, .height = EPD_7IN3F_HEIGHT, .palette = &palette_7color,
                      .put_pixel = put_pixel, .progress = feed_watchdog};
    JpegResult r = jpeg_convert(&in, &out, work_memory, sizeof(work_memory));
    uint32_t ms = to_ms_since_boot(get_absolute_time()) - start;
    printf("jpeg: %s, %lu ms, read %lu of %lu bytes\r\n", jpeg_result_str(r), (unsigned long)ms,
           (unsigned long)f_tell(&fil), (unsigned long)f_size(&fil));
    f_close(&fil);
    if (r != JPEG_OK)
        return false;
    start = to_ms_since_boot(get_absolute_time());
    if (cache_save(path, image))
        printf("jpeg: cached, %lu ms\r\n", (unsigned long)(to_ms_since_boot(get_absolute_time()) - start));
    return true;
}

// Set by display_image() for EPD_7IN3F_Refreshing(): the next image to convert and the frame buffer
static char prefetch_name[FF_LFN_BUF + 1];
static uint8_t *prefetch_image;

// While the panel refreshes an image, convert the image due next if it has no cache file yet, in
// the frame buffer the panel no longer needs
void EPD_7IN3F_Refreshing(void)
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
static void draw_failed_note(void)
{
    UWORD top = EPD_7IN3F_HEIGHT - 2 * Font22.Height - 2 * NOTE_PADDING;
    printf("Display note: %s\r\n", failed);
    Paint_SetRotate(180);
    Paint_ClearWindows(0, top, EPD_7IN3F_WIDTH, EPD_7IN3F_HEIGHT, EPD_7IN3F_WHITE);
    UWORD y = Paint_DrawString_EN(NOTE_X, top + NOTE_PADDING, msg(MSG_CANNOT_SHOW_TITLE), &Font22,
                                  EPD_7IN3F_WHITE, EPD_7IN3F_BLACK);
    Paint_DrawString_EN(NOTE_X, y, failed, &Font22, EPD_7IN3F_WHITE, EPD_7IN3F_BLACK);
}

// Draw images_path into the image; a photo that fails is saved as shown, so the order moves past
// it, and the next one is tried. Stops after MAX_TRIES or when the first failed photo comes round
// again (a card with few photos). Expects the card mounted and leaves it mounted.
static bool draw_photo(uint8_t *image)
{
    char first_failed[FF_LFN_BUF + 1];
    failed[0] = '\0';
    for (int tries = 1;; tries++)
    {
        Paint_Clear(EPD_7IN3F_WHITE);
        if (draw_jpeg(images_path, image))
            return true;
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

int display_image(float vol)
{
    printf("e-Paper Init and Clear...\r\n");
    EPD_7IN3F_Init();

    UBYTE *image;
    UDOUBLE image_size = ((EPD_7IN3F_WIDTH % 2 == 0) ? (EPD_7IN3F_WIDTH / 2) : (EPD_7IN3F_WIDTH / 2 + 1)) * EPD_7IN3F_HEIGHT;
    if ((image = (UBYTE *)malloc(image_size)) == NULL)
    {
        printf("Failed to apply for black memory...\r\n");
        return -1;
    }
    printf("Paint_NewImage\r\n");
    Paint_NewImage(image, EPD_7IN3F_WIDTH, EPD_7IN3F_HEIGHT, 0, EPD_7IN3F_WHITE);
    Paint_SetScale(7);

    sdcard_mount();
    Paint_SelectImage(image);

    bool shown = draw_photo(image);
    if (shown)
    {
        // The panel must never get the placeholder: an unused code leaves an afterimage
        palette_replace(image, image_size, palette_7color.bars,
                        settings.bars == BARS_BLACK ? EPD_7IN3F_BLACK : EPD_7IN3F_WHITE);
        Paint_SetRotate(Paint_GetRotate() == 90 ? 270 : 180);
        if (vol < 3.3)
        {
            char voltage_text[21] = {0};
            sprintf(voltage_text, "%f V", vol);
            UWORD y = Paint_DrawString_EN(10, 10, msg(MSG_LOW_VOLTAGE), &Font22, EPD_7IN3F_BLACK, EPD_7IN3F_WHITE);
            Paint_DrawString_EN(10, y, voltage_text, &Font22, EPD_7IN3F_BLACK, EPD_7IN3F_WHITE);
        }
        if (failed[0])
            draw_failed_note();
        if (images_peek_next(prefetch_name))
            prefetch_image = image;
    }
    else
    { // every try failed, all of them already saved as shown
        Paint_Clear(EPD_7IN3F_WHITE);
        draw_message(msg(MSG_CANNOT_SHOW_TITLE), failed);
    }

    printf("EPD_Display\r\n");
    uint32_t start = to_ms_since_boot(get_absolute_time());
    EPD_7IN3F_Display(image); // runs EPD_7IN3F_Refreshing()
    prefetch_image = NULL;
    printf("refresh: %lu ms\r\n", (unsigned long)(to_ms_since_boot(get_absolute_time()) - start));

    sdcard_unmount();
    if (shown)
        images_save_shown();

    printf("Goto Sleep...\r\n\r\n");
    EPD_7IN3F_Sleep();
    free(image);
    image = NULL;

    return 0;
}

int display_message(const char *title, const char *hint)
{
    printf("e-Paper Init and Clear...\r\n");
    EPD_7IN3F_Init();

    UBYTE *image;
    UDOUBLE image_size = ((EPD_7IN3F_WIDTH % 2 == 0) ? (EPD_7IN3F_WIDTH / 2) : (EPD_7IN3F_WIDTH / 2 + 1)) * EPD_7IN3F_HEIGHT;
    if ((image = (UBYTE *)malloc(image_size)) == NULL)
    {
        printf("Failed to apply for black memory...\r\n");
        return -1;
    }
    printf("Paint_NewImage\r\n");
    Paint_NewImage(image, EPD_7IN3F_WIDTH, EPD_7IN3F_HEIGHT, 0, EPD_7IN3F_WHITE);
    Paint_SetScale(7);

    Paint_SelectImage(image);
    Paint_Clear(EPD_7IN3F_WHITE);
    draw_message(title, hint);

    printf("EPD_Display\r\n");
    EPD_7IN3F_Display(image);

    printf("Goto Sleep...\r\n\r\n");
    EPD_7IN3F_Sleep();
    free(image);
    image = NULL;

    return 0;
}
