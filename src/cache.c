#include "cache.h"
#include "cache_format.h"
#include "images.h"
#include "palette.h"
#include "sdcard.h"
#include "settings.h"
#include "work_memory.h"
#include "f_util.h"
#include "ff.h"
#include "hardware/watchdog.h"

#include <stdio.h>
#include <stdlib.h> // qsort(), bsearch()
#include <string.h>

#define CACHE_DIR SDCARD_FRAME_DIR "/cache"
#define LANDSCAPE_DIR CACHE_DIR "/landscape" // one folder per frame orientation, same file layout
#define PORTRAIT_DIR CACHE_DIR "/portrait"
#define CHUNK_ROWS (WORK_MEMORY_SIZE / CACHE_ROW_BYTES) // rows per card read or write
#define PATH_LEN sizeof(LANDSCAPE_DIR "/" "0123456789abcdef.bmp") // the longer folder name

static const char *orientation_dir(void)
{
    return settings.orientation == ORIENTATION_PORTRAIT ? PORTRAIT_DIR : LANDSCAPE_DIR;
}

// Cache file path of photo `name` without extension; false if the photo cannot be found
static bool cache_path(char path[PATH_LEN], const char *name)
{
    FILINFO fno;
    FRESULT fr = f_stat(name, &fno);
    if (fr != FR_OK)
    {
        printf("f_stat(%s) error: %s (%d)\r\n", name, FRESULT_str(fr), fr);
        return false;
    }
    char key[CACHE_NAME_LEN + 1];
    cache_name(key, cache_key(fno.fname, (uint32_t)fno.fsize, fno.fdate, fno.ftime));
    snprintf(path, PATH_LEN, "%s/%s", orientation_dir(), key);
    return true;
}

bool cache_exists(const char *name)
{
    char path[PATH_LEN];
    FILINFO fno;
    if (!cache_path(path, name))
        return false;
    strcat(path, ".bmp");
    return f_stat(path, &fno) == FR_OK;
}

bool cache_load(const char *name, uint8_t *frame, const Palette *palette)
{
    char path[PATH_LEN];
    if (!cache_path(path, name))
        return false;
    strcat(path, ".bmp");
    FIL fil;
    if (f_open(&fil, path, FA_READ) != FR_OK)
        return false; // not converted yet
    UINT br;
    bool ok = f_size(&fil) == CACHE_FILE_SIZE && f_read(&fil, work_memory, CACHE_HEADER_SIZE, &br) == FR_OK &&
              br == CACHE_HEADER_SIZE && cache_header_ok(work_memory, palette);
    for (int y = 0; ok && y < CACHE_HEIGHT; y += CHUNK_ROWS)
    {
        int rows = CACHE_HEIGHT - y < CHUNK_ROWS ? CACHE_HEIGHT - y : CHUNK_ROWS;
        UINT want = (UINT)(rows * CACHE_ROW_BYTES);
        ok = f_read(&fil, work_memory, want, &br) == FR_OK && br == want;
        for (int i = 0; ok && i < rows; i++)
            cache_row_to_frame(frame, work_memory + i * CACHE_ROW_BYTES, y + i);
        watchdog_update();
    }
    f_close(&fil);
    if (!ok)
        printf("cache file %s unreadable or from another palette, converting again\r\n", path);
    return ok;
}

size_t cache_root_keys(uint64_t *keys, size_t max)
{
    size_t count = 0;
    DIR dir;
    FILINFO fno;
    if (f_opendir(&dir, "/") != FR_OK)
        return SIZE_MAX;
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
    {
        if (!images_is_image(&fno))
            continue;
        if (count == max)
        {
            f_closedir(&dir);
            return SIZE_MAX;
        }
        keys[count++] = cache_key(fno.fname, (uint32_t)fno.fsize, fno.fdate, fno.ftime);
    }
    f_closedir(&dir);
    qsort(keys, count, sizeof(keys[0]), cache_compare_keys);
    return count;
}

// Delete the cache files in dir_path whose key is not in keys (sorted), and .tmp leftovers
static int prune_dir(const char *dir_path, const uint64_t *keys, size_t count)
{
    int removed = 0;
    DIR dir;
    FILINFO fno;
    if (f_opendir(&dir, dir_path) != FR_OK)
        return 0; // no cache yet
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
    {
        uint64_t key;
        bool tmp;
        if (!cache_parse_name(fno.fname, &key, &tmp))
            continue; // not ours
        if (!tmp && bsearch(&key, keys, count, sizeof(keys[0]), cache_compare_keys))
            continue;
        char path[PATH_LEN];
        // cache_parse_name() accepted it: 16 hex digits and an extension, so it fits
        snprintf(path, sizeof(path), "%s/%.*s", dir_path, CACHE_NAME_LEN + 4, fno.fname);
        if (f_unlink(path) == FR_OK)
            removed++;
        watchdog_update();
    }
    f_closedir(&dir);
    return removed;
}

int cache_prune(void)
{
    // Keys of the photos in the card root, in the work memory; an incomplete list would delete cache
    // files still in use
    uint64_t *keys = (uint64_t *)(void *)work_memory;
    size_t max = WORK_MEMORY_SIZE / sizeof(uint64_t);
    size_t count = cache_root_keys(keys, max);
    if (count == SIZE_MAX)
    {
        printf("cache cleanup skipped: card root unreadable or more than %u photos\r\n", (unsigned)max);
        return 0;
    }

    int removed = prune_dir(LANDSCAPE_DIR, keys, count) + prune_dir(PORTRAIT_DIR, keys, count);
    printf("cache cleanup: %d photos, %d cache files removed\r\n", (int)count, removed);
    return removed;
}

bool cache_save(const char *name, const uint8_t *frame, const Palette *palette)
{
    char tmp[PATH_LEN], path[PATH_LEN];
    if (!cache_path(path, name))
        return false;
    strcpy(tmp, path);
    strcat(tmp, ".tmp");
    strcat(path, ".bmp");
    sdcard_make_frame_dir();
    f_mkdir(CACHE_DIR);
    f_mkdir(orientation_dir());

    FIL fil;
    FRESULT fr = f_open(&fil, tmp, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK)
    {
        printf("cannot write %s: %s (%d)\r\n", tmp, FRESULT_str(fr), fr);
        return false;
    }
    UINT bw;
    cache_header(work_memory, palette);
    bool ok = f_write(&fil, work_memory, CACHE_HEADER_SIZE, &bw) == FR_OK && bw == CACHE_HEADER_SIZE;
    for (int y = 0; ok && y < CACHE_HEIGHT; y += CHUNK_ROWS)
    {
        int rows = CACHE_HEIGHT - y < CHUNK_ROWS ? CACHE_HEIGHT - y : CHUNK_ROWS;
        for (int i = 0; i < rows; i++)
            cache_row_from_frame(work_memory + i * CACHE_ROW_BYTES, frame, y + i);
        UINT want = (UINT)(rows * CACHE_ROW_BYTES);
        ok = f_write(&fil, work_memory, want, &bw) == FR_OK && bw == want;
        watchdog_update();
    }
    ok = f_close(&fil) == FR_OK && ok;
    if (ok)
    {
        f_unlink(path); // an unreadable old one; f_rename does not replace
        ok = f_rename(tmp, path) == FR_OK;
    }
    if (!ok)
    {
        printf("cannot write %s (card full?)\r\n", path);
        f_unlink(tmp);
    }
    return ok;
}
