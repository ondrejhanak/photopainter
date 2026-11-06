#include "images.h"
#include "cache.h"
#include "cache_format.h"
#include "image_order.h"
#include "sdcard.h"
#include "settings.h"
#include "work_memory.h"
#include "f_util.h"
#include "pico/rand.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // strcasecmp()

#define STATE_FILE SDCARD_FRAME_DIR "/state.txt"
#define PHOTOS_FILE SDCARD_FRAME_DIR "/photos.bin" // cache keys of the photos when drive mode started
#define NEW_FILE SDCARD_FRAME_DIR "/new.bin" // cache keys of the photos added in the latest drive mode
#define NEW_TMP SDCARD_FRAME_DIR "/new.tmp"
#define MAX_KEYS (WORK_MEMORY_SIZE / sizeof(uint64_t))

char images_path[FF_LFN_BUF + 1]; // file name of the image to display, in the card root
int images_count;
static char first[FF_LFN_BUF + 1], next[FF_LFN_BUF + 1]; // results of scan_images()

// State file: the normal order continues after `last`; while new photos are shown first, `cursor` is
// the last of them shown (empty before the first). `seed` identifies the random order's shuffle.
static char last[FF_LFN_BUF + 1], cursor[FF_LFN_BUF + 1];
static uint32_t seed;

// images_path is one of the new photos; also_normal: it is the normal order's next one as well
static bool new_photo, also_normal;
// Keys of the new photos (NEW_FILE), sorted, in the work memory
static uint64_t *const new_keys = (uint64_t *)(void *)work_memory;
static size_t new_count;

static bool has_extension(const char *name, const char *ext)
{
    size_t len = strlen(name), ext_len = strlen(ext);
    return len > ext_len && strcasecmp(name + len - ext_len, ext) == 0;
}

bool images_is_image(const FILINFO *fno)
{
    if (fno->fattrib & (AM_DIR | AM_HID | AM_SYS))
        return false;
    if (fno->fname[0] == '.')
        return false;
    return has_extension(fno->fname, ".jpg") || has_extension(fno->fname, ".jpeg");
}

// Display order of two names under the current settings
static int order_compare(const char *a, const char *b)
{
    return image_order_compare(a, b, settings.order, seed);
}

static uint64_t entry_key(const FILINFO *fno)
{
    return cache_key(fno->fname, (uint32_t)fno->fsize, fno->fdate, fno->ftime);
}

static void read_line(FIL *fil, char *line, int len)
{
    line[0] = '\0';
    if (f_gets(line, len, fil))
        line[strcspn(line, "\r\n")] = '\0';
}

// Read the state file; all empty (seed 0) if there is none, and a missing cursor line reads as empty
static void read_state(void)
{
    char line[16];
    last[0] = cursor[0] = '\0';
    seed = 0;
    FIL fil;
    if (f_open(&fil, STATE_FILE, FA_READ) != FR_OK)
        return;
    read_line(&fil, last, sizeof(last));
    read_line(&fil, line, sizeof(line));
    seed = strtoul(line, NULL, 16);
    read_line(&fil, cursor, sizeof(cursor));
    f_close(&fil);
}

static void write_state(void)
{
    char line[16];
    sdcard_make_frame_dir();

    FIL fil;
    FRESULT fr = f_open(&fil, STATE_FILE, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK)
    {
        printf("cannot write %s: %s (%d)\r\n", STATE_FILE, FRESULT_str(fr), fr);
        return;
    }
    snprintf(line, sizeof(line), "\n%08lx\n", (unsigned long)seed);
    f_puts(last, &fil);
    f_puts(line, &fil);
    f_puts(cursor, &fil);
    f_puts("\n", &fil);
    f_close(&fil);
}

// Load NEW_FILE into new_keys; false if there are no new photos
static bool load_new_keys(void)
{
    new_count = 0;
    FIL fil;
    if (f_open(&fil, NEW_FILE, FA_READ) != FR_OK)
        return false;
    UINT br;
    if (f_read(&fil, new_keys, MAX_KEYS * sizeof(uint64_t), &br) == FR_OK)
        new_count = br / sizeof(uint64_t);
    f_close(&fil);
    qsort(new_keys, new_count, sizeof(new_keys[0]), cache_compare_keys);
    return new_count > 0;
}

// One pass over the card root: the first image in display order, and the first one after `from`.
// only_new: consider only the photos in new_keys.
static void scan_images(const char *from, bool only_new)
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
    images_count = 0;
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
    {
        if (!images_is_image(&fno))
            continue;
        images_count++;
        if (only_new)
        {
            uint64_t key = entry_key(&fno);
            if (!bsearch(&key, new_keys, new_count, sizeof(new_keys[0]), cache_compare_keys))
                continue;
        }
        if (!first[0] || order_compare(fno.fname, first) < 0)
            strcpy(first, fno.fname);
        if (from[0] && order_compare(fno.fname, from) > 0 && (!next[0] || order_compare(fno.fname, next) < 0))
            strcpy(next, fno.fname);
    }
    f_closedir(&dir);
}

// The image the normal order shows after `from` (the first one if from is empty); NULL if there is
// none, or at the end of a random round, whose next shuffle is drawn when it starts
static const char *normal_after(const char *from)
{
    scan_images(from, false);
    if (next[0])
        return next;
    if (first[0] && (!from[0] || settings.order == ORDER_ALPHABETICAL))
        return first; // alphabetical wraps around
    return NULL;
}

static uint32_t new_seed(void)
{
    return get_rand_32() | 1; // never 0, which means "no seed yet"
}

// The next new photo after the cursor into images_path; false when all were shown (or deleted),
// which ends the new photos
static bool select_new_photo(void)
{
    also_normal = false;
    if (!load_new_keys())
    {
        cursor[0] = '\0';
        return false;
    }
    scan_images(cursor, true);
    const char *pick = cursor[0] ? next : first;
    if (!pick[0])
    {
        printf("new photos: all shown\r\n");
        f_unlink(NEW_FILE);
        cursor[0] = '\0';
        return false;
    }
    strcpy(images_path, pick);
    // Shown as the normal next anyway: counts for both, so it doesn't come again right away
    const char *normal = normal_after(last);
    also_normal = normal && strcmp(normal, images_path) == 0;
    return true;
}

// The image after `last` in the normal order into images_path; false if the card root has none.
// Alphabetical wraps around; random follows the shuffle key, so every image is shown once per round
// and a new seed reshuffles after it. Adding or removing images never causes skips or repeats.
static bool select_normal(void)
{
    scan_images(last, false);
    if (first[0] && !next[0] && settings.order == ORDER_RANDOM && last[0])
    {
        // End of a round: reshuffle. If the new shuffle starts with the image just shown, draw
        // another seed (gives up after a few tries, e.g. when there is only one image).
        for (int tries = 0; tries < 8; tries++)
        {
            seed = new_seed();
            scan_images("", false);
            if (strcmp(first, last) != 0)
                break;
        }
        printf("random order: new round\r\n");
    }
    if (!first[0])
        return false;
    strcpy(images_path, next[0] ? next : first);
    return true;
}

bool images_select_next(void)
{
    sdcard_mount();
    read_state();
    if (settings.order == ORDER_RANDOM && seed == 0)
        seed = new_seed();

    new_photo = select_new_photo();
    bool found = new_photo || select_normal();
    sdcard_unmount();

    if (!found)
    {
        images_path[0] = '\0';
        printf("no images in the card root\r\n");
        return false;
    }
    printf("next image: %s%s (last shown: %s, seed %08lx)\r\n", images_path, new_photo ? " (new)" : "",
           last[0] ? last : "-", (unsigned long)seed);
    return true;
}

bool images_peek_next(char *name)
{
    const char *peeked = NULL;
    if (new_photo && load_new_keys()) // loaded again: the conversion reused the work memory
    {
        scan_images(images_path, true);
        if (next[0])
            peeked = next;
    }
    if (!peeked)
        peeked = normal_after(new_photo && !also_normal ? last : images_path);
    if (!peeked)
        return false;
    strcpy(name, peeked);
    return strcmp(name, images_path) != 0;
}

// Remember images_path as shown, so the next selection continues after it
void images_save_shown(void)
{
    if (new_photo)
        strcpy(cursor, images_path);
    if (!new_photo || also_normal)
        strcpy(last, images_path);
    sdcard_mount();
    write_state();
    sdcard_unmount();
}

void images_remember_photos(void)
{
    sdcard_mount();
    size_t count = cache_root_keys(new_keys, MAX_KEYS);
    bool ok = false;
    if (count != SIZE_MAX)
    {
        sdcard_make_frame_dir();
        FIL fil;
        if (f_open(&fil, PHOTOS_FILE, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK)
        {
            UINT bw, want = (UINT)(count * sizeof(uint64_t));
            ok = f_write(&fil, new_keys, want, &bw) == FR_OK && bw == want;
            ok = f_close(&fil) == FR_OK && ok;
        }
    }
    if (ok)
        printf("photo list: %u photos\r\n", (unsigned)count);
    else
    { // without a list nothing counts as new after drive mode
        f_unlink(PHOTOS_FILE);
        printf("photo list not saved\r\n");
    }
    sdcard_unmount();
}

int images_find_new(void)
{
    sdcard_mount();
    // The list of photos before drive mode (sorted), in the work memory; deleted once read, so a later
    // drive mode never compares against it
    uint64_t *before = new_keys;
    FIL fil;
    if (f_open(&fil, PHOTOS_FILE, FA_READ) != FR_OK)
    {
        sdcard_unmount();
        return 0;
    }
    UINT br;
    bool ok = f_read(&fil, before, MAX_KEYS * sizeof(uint64_t), &br) == FR_OK;
    size_t count = br / sizeof(uint64_t);
    f_close(&fil);
    f_unlink(PHOTOS_FILE);

    // Keys of the photos not in it, written under a temporary name: an unfinished or empty list must
    // not replace the new photos of an earlier drive mode
    int added = 0;
    DIR dir;
    FILINFO fno;
    ok = ok && f_open(&fil, NEW_TMP, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK;
    if (ok)
    {
        ok = f_opendir(&dir, "/") == FR_OK;
        while (ok && f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
        {
            if (!images_is_image(&fno))
                continue;
            uint64_t key = entry_key(&fno);
            if (bsearch(&key, before, count, sizeof(before[0]), cache_compare_keys))
                continue;
            UINT bw;
            ok = f_write(&fil, &key, sizeof(key), &bw) == FR_OK && bw == sizeof(key);
            added++;
        }
        if (ok)
            f_closedir(&dir);
        ok = f_close(&fil) == FR_OK && ok;
        if (ok && added)
        {
            f_unlink(NEW_FILE); // f_rename does not replace
            ok = f_rename(NEW_TMP, NEW_FILE) == FR_OK;
        }
        f_unlink(NEW_TMP);
    }
    if (ok && added)
    { // shown from the first new photo on; the normal order stays where it was
        read_state();
        cursor[0] = '\0';
        write_state();
    }
    printf("new photos: %d%s\r\n", added, ok ? "" : " (list not saved)");
    sdcard_unmount();
    return ok ? added : 0;
}
