#include "cache.h"
#include "cache_format.h"
#include "palette.h"
#include "sdcard.h"
#include "work_memory.h"
#include "f_util.h"
#include "ff.h"
#include "hardware/watchdog.h"

#include <stdio.h>
#include <string.h>

#define CACHE_DIR SDCARD_FRAME_DIR "/cache"
#define LANDSCAPE_DIR CACHE_DIR "/landscape" // one folder per frame orientation
#define CHUNK_ROWS (WORK_MEMORY_SIZE / CACHE_ROW_BYTES) // rows per card read or write
#define PATH_LEN sizeof(LANDSCAPE_DIR "/" "0123456789abcdef.bmp")

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
    cache_name(key, fno.fname, (uint32_t)fno.fsize, fno.fdate, fno.ftime);
    snprintf(path, PATH_LEN, LANDSCAPE_DIR "/%s", key);
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

bool cache_load(const char *name, uint8_t *frame)
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
              br == CACHE_HEADER_SIZE && cache_header_ok(work_memory);
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
        printf("cache file %s unreadable, converting again\r\n", path);
    return ok;
}

bool cache_save(const char *name, const uint8_t *frame)
{
    char tmp[PATH_LEN], path[PATH_LEN];
    if (!cache_path(path, name))
        return false;
    strcpy(tmp, path);
    strcat(tmp, ".tmp");
    strcat(path, ".bmp");
    sdcard_make_frame_dir();
    f_mkdir(CACHE_DIR);
    f_mkdir(LANDSCAPE_DIR);

    FIL fil;
    FRESULT fr = f_open(&fil, tmp, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK)
    {
        printf("cannot write %s: %s (%d)\r\n", tmp, FRESULT_str(fr), fr);
        return false;
    }
    UINT bw;
    cache_header(work_memory, &palette_7color);
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
