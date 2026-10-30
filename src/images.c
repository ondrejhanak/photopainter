#include "images.h"
#include "image_order.h"
#include "sdcard.h"
#include "settings.h"
#include "f_util.h"
#include "pico/rand.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // strcasecmp()

// Frame state, hidden from users: a dot-folder (hidden on macOS) with the FAT hidden attribute (Windows)
#define STATE_DIR "/.photopainter"
#define STATE_FILE STATE_DIR "/state.txt"

char images_path[FF_LFN_BUF + 1]; // file name of the image to display, in the card root
static uint32_t seed; // random order: identifies the current shuffle; persisted in the state file

// A displayable image: a .bmp file, not a folder, not hidden/system, and not a dot-file
// (skips macOS "._" companion files and similar)
static bool is_image(const FILINFO *fno)
{
    if (fno->fattrib & (AM_DIR | AM_HID | AM_SYS))
        return false;
    if (fno->fname[0] == '.')
        return false;
    size_t len = strlen(fno->fname);
    return len > 4 && strcasecmp(fno->fname + len - 4, ".bmp") == 0;
}

// Display order of two names under the current settings
static int order_compare(const char *a, const char *b)
{
    return image_order_compare(a, b, settings.order, seed);
}

// Read the state file: last shown image name (empty if none yet) and the shuffle seed (0 if none)
static void read_state(char *name, int len)
{
    char line[16];
    name[0] = '\0';
    seed = 0;
    FIL fil;
    if (f_open(&fil, STATE_FILE, FA_READ) != FR_OK)
        return;
    if (f_gets(name, len, &fil))
        name[strcspn(name, "\r\n")] = '\0';
    if (f_gets(line, sizeof(line), &fil))
        seed = strtoul(line, NULL, 16);
    f_close(&fil);
}

static void write_state(const char *name)
{
    char line[16];
    if (f_mkdir(STATE_DIR) == FR_OK)
        f_chmod(STATE_DIR, AM_HID, AM_HID);

    FIL fil;
    FRESULT fr = f_open(&fil, STATE_FILE, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK)
    {
        printf("cannot write %s: %s (%d)\r\n", STATE_FILE, FRESULT_str(fr), fr);
        return;
    }
    snprintf(line, sizeof(line), "\n%08lx\n", (unsigned long)seed);
    f_puts(name, &fil);
    f_puts(line, &fil);
    f_close(&fil);
}

// One pass over the card root: the first image in display order, and the first one after `last`
static void scan_images(const char *last, char *first, char *next)
{
    first[0] = next[0] = '\0';

    DIR dir;
    FILINFO fno;
    FRESULT fr = f_opendir(&dir, "/");
    if (fr != FR_OK)
    {
        printf("f_opendir error: %s (%d)\r\n", FRESULT_str(fr), fr);
        return;
    }
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
    {
        if (!is_image(&fno))
            continue;
        if (!first[0] || order_compare(fno.fname, first) < 0)
            strcpy(first, fno.fname);
        if (last[0] && order_compare(fno.fname, last) > 0 && (!next[0] || order_compare(fno.fname, next) < 0))
            strcpy(next, fno.fname);
    }
    f_closedir(&dir);
}

static uint32_t new_seed(void)
{
    return get_rand_32() | 1; // never 0, which means "no seed yet"
}

/*
    function:
        Pick the image after the last shown one into images_path, in the order set by settings.ini.
        Alphabetical: natural name order, wrapping around. Random: order of the shuffle key, so every
        image is shown once per round; at the end of a round a new seed reshuffles.
        Adding or removing images never causes skips or repeats within a round.
    return:
        false if the card root contains no images
*/
bool images_select_next(void)
{
    static char last[FF_LFN_BUF + 1];
    static char first[FF_LFN_BUF + 1];
    static char next[FF_LFN_BUF + 1];

    sdcard_mount();
    read_state(last, sizeof(last));
    if (settings.order == ORDER_RANDOM && seed == 0)
        seed = new_seed();

    scan_images(last, first, next);
    if (first[0] && !next[0] && settings.order == ORDER_RANDOM && last[0])
    {
        // End of a round: reshuffle. If the new shuffle starts with the image just shown, draw
        // another seed (gives up after a few tries, e.g. when there is only one image).
        for (int tries = 0; tries < 8; tries++)
        {
            seed = new_seed();
            scan_images("", first, next);
            if (strcmp(first, last) != 0)
                break;
        }
        printf("random order: new round\r\n");
    }
    sdcard_unmount();

    if (!first[0])
    {
        images_path[0] = '\0';
        printf("no images in the card root\r\n");
        return false;
    }
    strcpy(images_path, next[0] ? next : first);
    printf("next image: %s (last shown: %s, seed %08lx)\r\n", images_path, last[0] ? last : "-", (unsigned long)seed);
    return true;
}

// Remember images_path as shown, so the next selection continues after it
void images_save_shown(void)
{
    sdcard_mount();
    write_state(images_path);
    sdcard_unmount();
}
