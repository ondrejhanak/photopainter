// Host tests of the photo selection (normal order, new photos first after drive mode) and the cache
// cleanup, on the real FatFs over a RAM disk. Failures go to stderr, the exit code is their count.
#include "cache.h"
#include "cache_format.h"
#include "ff.h"
#include "diskio.h" // after ff.h, which defines its types
#include "image_order.h"
#include "images.h"
#include "sdcard.h"
#include "settings.h"
#include "work_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTORS 8192 // 4 MB
#define CACHE_DIR SDCARD_FRAME_DIR "/cache/landscape"
#define PORTRAIT_DIR SDCARD_FRAME_DIR "/cache/portrait"
#define PHOTOS_FILE SDCARD_FRAME_DIR "/photos.bin"
#define NEW_FILE SDCARD_FRAME_DIR "/new.bin"
#define MAX_KEYS (WORK_MEMORY_SIZE / sizeof(uint64_t))

static uint8_t disk[SECTORS * 512];
static FATFS fs;
static uint32_t rand_state;
static const char *test_name;
static int failures;

alignas(8) uint8_t work_memory[WORK_MEMORY_SIZE];
Settings settings;

uint32_t get_rand_32(void)
{
    rand_state = rand_state * 1664525u + 1013904223u;
    return rand_state;
}

DWORD get_fattime(void)
{
    return (DWORD)(45 << 25 | 1 << 21 | 1 << 16); // 2025-01-01
}

DSTATUS disk_initialize(BYTE pdrv)
{
    return 0;
}

DSTATUS disk_status(BYTE pdrv)
{
    return 0;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    memcpy(buff, disk + sector * 512, count * 512);
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    memcpy(disk + sector * 512, buff, count * 512);
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    switch (cmd)
    {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_COUNT:
        *(LBA_t *)buff = SECTORS;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *(DWORD *)buff = 1;
        return RES_OK;
    }
    return RES_PARERR;
}

void sdcard_mount(void)
{
    f_mount(&fs, "", 1);
}

void sdcard_unmount(void)
{
    f_mount(NULL, "", 0);
}

void sdcard_make_frame_dir(void)
{
    f_mkdir(SDCARD_FRAME_DIR);
}

// The prefetch's guess of the next photo, checked by show(); a card change makes it stale
static char peeked[FF_LFN_BUF + 1];
static bool has_peek;

static void fail(const char *fmt, const char *a, const char *b)
{
    fprintf(stderr, "FAIL %s: ", test_name);
    fprintf(stderr, fmt, a, b);
    fprintf(stderr, "\n");
    failures++;
}

static void new_card(const char *name, ImageOrder order)
{
    static uint8_t work[FF_MAX_SS * 4];
    MKFS_PARM opt = {FM_FAT, 0, 0, 0, 0};
    test_name = name;
    memset(disk, 0, sizeof(disk));
    if (f_mkfs("", &opt, work, sizeof(work)) != FR_OK)
        fail("%s%s", "f_mkfs failed", "");
    settings.order = order;
    rand_state = 1;
    has_peek = false;
    printf("--- %s\n", name);
}

// Add or replace a photo; another size gives it another cache key
static void put(const char *name, unsigned size)
{
    static const uint8_t zeros[512];
    has_peek = false;
    sdcard_mount();
    FIL fil;
    UINT bw;
    f_open(&fil, name, FA_CREATE_ALWAYS | FA_WRITE);
    for (unsigned done = 0; done < size; done += bw)
        f_write(&fil, zeros, size - done < sizeof(zeros) ? size - done : sizeof(zeros), &bw);
    f_close(&fil);
    sdcard_unmount();
}

static void put_old5(void)
{
    put("a1.jpg", 100);
    put("a3.jpg", 100);
    put("a5.jpg", 100);
    put("a7.jpg", 100);
    put("a9.jpg", 100);
}

static void del(const char *path)
{
    has_peek = false;
    sdcard_mount();
    f_unlink(path);
    sdcard_unmount();
}

static bool exists(const char *path)
{
    FILINFO fno;
    sdcard_mount();
    bool found = f_stat(path, &fno) == FR_OK;
    sdcard_unmount();
    return found;
}

static void expect_file(const char *path, bool want)
{
    if (exists(path) != want)
        fail("%s %s", path, want ? "missing" : "left over");
}

// One photo change as display_image() does it: select, convert (reuses the work memory), peek the
// next one for the prefetch, show, save. Checks that the previous peek named this photo.

static const char *show(void)
{
    static char shown[FF_LFN_BUF + 1];
    bool peek_before = has_peek;
    has_peek = false;
    if (!images_select_next())
        return "";
    strcpy(shown, images_path);
    if (peek_before && strcmp(peeked, shown) != 0)
        fail("prefetch peeked %s, but %s is shown", peeked, shown);
    memset(work_memory, 0xA5, sizeof(work_memory));
    sdcard_mount();
    has_peek = images_peek_next(peeked);
    sdcard_unmount();
    images_save_shown();
    return shown;
}

static void expect_shows(const char *const *want)
{
    for (int i = 0; want[i]; i++)
    {
        const char *got = show();
        if (strcmp(got, want[i]) != 0)
        {
            fail("shown %s, want %s", got, want[i]);
            return;
        }
    }
}

#define SHOWS(...) expect_shows((const char *const[]){__VA_ARGS__, NULL})

static void test_no_list(void)
{
    new_card("no photo list: nothing is new", ORDER_ALPHABETICAL);
    put_old5();
    SHOWS("a1.jpg", "a3.jpg");
    put("a2.jpg", 100);
    images_find_new();
    SHOWS("a5.jpg");
    expect_file(NEW_FILE, false);
}

static void test_no_change(void)
{
    new_card("drive mode without changes", ORDER_ALPHABETICAL);
    put_old5();
    SHOWS("a1.jpg", "a3.jpg");
    images_remember_photos();
    expect_file(PHOTOS_FILE, true);
    images_find_new();
    expect_file(PHOTOS_FILE, false);
    expect_file(NEW_FILE, false);
    SHOWS("a5.jpg", "a7.jpg");
}

static void test_batch_alphabetical(void)
{
    new_card("new photos first, then where it was (alphabetical)", ORDER_ALPHABETICAL);
    put_old5();
    SHOWS("a1.jpg", "a3.jpg");
    images_remember_photos();
    put("a8.jpg", 100);
    put("a10.jpg", 100);
    put("a2.jpg", 100);
    images_find_new();
    SHOWS("a2.jpg", "a8.jpg", "a10.jpg");
    expect_file(NEW_FILE, true);
    SHOWS("a5.jpg");
    expect_file(NEW_FILE, false);
    SHOWS("a7.jpg", "a8.jpg", "a9.jpg", "a10.jpg", "a1.jpg");
}

static void test_new_is_normal_next(void)
{
    new_card("a new photo that is also the normal next", ORDER_ALPHABETICAL);
    put_old5();
    SHOWS("a1.jpg", "a3.jpg");
    images_remember_photos();
    put("a4.jpg", 100);
    put("a6.jpg", 100);
    images_find_new();
    SHOWS("a4.jpg", "a6.jpg", "a5.jpg", "a6.jpg", "a7.jpg");
}

static void test_empty_card(void)
{
    new_card("first photos on an empty card", ORDER_ALPHABETICAL);
    images_remember_photos();
    expect_file(PHOTOS_FILE, true);
    put("b2.jpg", 100);
    put("b1.jpg", 100);
    put("b3.jpg", 100);
    images_find_new();
    SHOWS("b1.jpg", "b2.jpg", "b3.jpg", "b1.jpg", "b2.jpg");
}

static uint32_t state_seed(void);

// All photos are new: the batch counts as the round, so the next photo starts a new shuffle
static void test_empty_card_random(void)
{
    new_card("first photos on an empty card (random)", ORDER_RANDOM);
    images_remember_photos();
    put("b1.jpg", 100);
    put("b2.jpg", 100);
    put("b3.jpg", 100);
    images_find_new();
    for (int i = 0; i < 3; i++)
        show();
    uint32_t seed = state_seed();
    show();
    if (state_seed() == seed)
        fail("%s%s", "no new round after the new photos", "");
}

static void test_replaced(void)
{
    new_card("a replaced photo counts as new", ORDER_ALPHABETICAL);
    put_old5();
    SHOWS("a1.jpg");
    images_remember_photos();
    put("a7.jpg", 200);
    images_find_new();
    SHOWS("a7.jpg", "a3.jpg", "a5.jpg", "a7.jpg");
}

static void test_second_batch(void)
{
    new_card("a new batch replaces an unfinished one", ORDER_ALPHABETICAL);
    put_old5();
    SHOWS("a1.jpg", "a3.jpg");
    images_remember_photos();
    put("a6.jpg", 100);
    put("a8.jpg", 100);
    images_find_new();
    SHOWS("a6.jpg");
    images_remember_photos();
    put("a2.jpg", 100);
    images_find_new();
    SHOWS("a2.jpg", "a5.jpg", "a6.jpg", "a7.jpg", "a8.jpg");
}

static void test_batch_kept(void)
{
    new_card("drive mode without additions keeps the batch", ORDER_ALPHABETICAL);
    put_old5();
    SHOWS("a1.jpg", "a3.jpg");
    images_remember_photos();
    put("a6.jpg", 100);
    put("a8.jpg", 100);
    images_find_new();
    SHOWS("a6.jpg");
    images_remember_photos();
    images_find_new();
    SHOWS("a8.jpg", "a5.jpg");
}

static void test_batch_deleted(void)
{
    new_card("a deleted new photo is skipped", ORDER_ALPHABETICAL);
    put_old5();
    SHOWS("a1.jpg", "a3.jpg");
    images_remember_photos();
    put("a6.jpg", 100);
    put("a8.jpg", 100);
    images_find_new();
    SHOWS("a6.jpg");
    images_remember_photos();
    del("a8.jpg");
    images_find_new();
    SHOWS("a5.jpg");
    expect_file(NEW_FILE, false);
    SHOWS("a6.jpg", "a7.jpg", "a9.jpg");
}

static uint32_t state_seed(void)
{
    char line[64] = "";
    sdcard_mount();
    FIL fil;
    if (f_open(&fil, SDCARD_FRAME_DIR "/state.txt", FA_READ) == FR_OK)
    {
        f_gets(line, sizeof(line), &fil);
        f_gets(line, sizeof(line), &fil);
        f_close(&fil);
    }
    sdcard_unmount();
    return (uint32_t)strtoul(line, NULL, 16);
}

static void test_batch_random(void)
{
    new_card("new photos first, then where it was (random)", ORDER_RANDOM);
    char name[16];
    for (int i = 1; i <= 12; i++)
    {
        snprintf(name, sizeof(name), "p%d.jpg", i);
        put(name, 100);
    }
    for (int i = 0; i < 4; i++)
        show();
    char resume[FF_LFN_BUF + 1];
    if (!has_peek)
        fail("%s%s", "no normal next to resume at", "");
    strcpy(resume, peeked);
    images_remember_photos();
    put("n1.jpg", 100);
    put("n2.jpg", 100);
    put("n3.jpg", 100);
    images_find_new();

    // The new photos in the shuffle's order
    const char *want[] = {"n1.jpg", "n2.jpg", "n3.jpg"};
    uint32_t seed = state_seed();
    for (int i = 0; i < 3; i++)
        for (int j = i + 1; j < 3; j++)
            if (image_order_compare(want[j], want[i], ORDER_RANDOM, seed) < 0)
            {
                const char *t = want[i];
                want[i] = want[j];
                want[j] = t;
            }
    SHOWS(want[0], want[1], want[2], resume);
}

static void put_many(int count)
{
    char name[16];
    for (int i = 0; i < count; i++)
    {
        snprintf(name, sizeof(name), "c%03d.jpg", i);
        put(name, 100);
    }
}

static void test_photo_cap(void)
{
    new_card("photo list at the cap", ORDER_ALPHABETICAL);
    put_many(MAX_KEYS);
    images_remember_photos();
    expect_file(PHOTOS_FILE, true);
    put("c999.jpg", 100);
    images_remember_photos();
    expect_file(PHOTOS_FILE, false);
    put("c500.jpg", 100);
    images_find_new();
    expect_file(NEW_FILE, false);
}

static void put_cache_file_in(const char *dir, const char *photo, const char *ext)
{
    FILINFO fno;
    char key[CACHE_NAME_LEN + 1], path[64];
    sdcard_mount();
    f_stat(photo, &fno);
    cache_name(key, cache_key(fno.fname, (uint32_t)fno.fsize, fno.fdate, fno.ftime));
    snprintf(path, sizeof(path), "%s/%s%s", dir, key, ext);
    f_mkdir(SDCARD_FRAME_DIR);
    f_mkdir(SDCARD_FRAME_DIR "/cache");
    f_mkdir(dir);
    FIL fil;
    f_open(&fil, path, FA_CREATE_ALWAYS | FA_WRITE);
    f_close(&fil);
    sdcard_unmount();
}

static void put_cache_file(const char *photo, const char *ext)
{
    put_cache_file_in(CACHE_DIR, photo, ext);
}

static int count_files_in(const char *dir_path)
{
    int count = 0;
    DIR dir;
    FILINFO fno;
    sdcard_mount();
    if (f_opendir(&dir, dir_path) == FR_OK)
    {
        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
            count++;
        f_closedir(&dir);
    }
    sdcard_unmount();
    return count;
}

static int count_cache_files(void)
{
    return count_files_in(CACHE_DIR);
}

static int prune(void)
{
    sdcard_mount();
    int removed = cache_prune();
    sdcard_unmount();
    return removed;
}

static void test_prune(void)
{
    new_card("cache cleanup", ORDER_ALPHABETICAL);
    put("a1.jpg", 100);
    put("a3.jpg", 100);
    put("gone.jpg", 100);
    put_cache_file("a1.jpg", ".bmp");
    put_cache_file("a3.jpg", ".bmp");
    put_cache_file("a3.jpg", ".tmp");
    put_cache_file("gone.jpg", ".bmp");
    put_cache_file("a1.jpg", ".txt");
    put_cache_file_in(PORTRAIT_DIR, "a1.jpg", ".bmp");
    put_cache_file_in(PORTRAIT_DIR, "gone.jpg", ".bmp");
    del("gone.jpg");
    if (prune() != 3 || count_cache_files() != 3 || count_files_in(PORTRAIT_DIR) != 1)
        fail("%s%s", "wrong cache files removed", "");

    new_card("cache cleanup skipped above the cap", ORDER_ALPHABETICAL);
    put_many(MAX_KEYS + 1);
    put("gone.jpg", 100);
    put_cache_file("gone.jpg", ".bmp");
    del("gone.jpg");
    if (prune() != 0 || count_cache_files() != 1)
        fail("%s%s", "cleanup ran on an incomplete list", "");
}

static bool cached(const char *photo)
{
    sdcard_mount();
    bool found = cache_exists(photo);
    sdcard_unmount();
    return found;
}

static void test_cache_folder(void)
{
    new_card("cache folder of the frame orientation", ORDER_ALPHABETICAL);
    put("a1.jpg", 100);
    put_cache_file_in(PORTRAIT_DIR, "a1.jpg", ".bmp");
    if (cached("a1.jpg"))
        fail("%s%s", "landscape found the portrait cache file", "");
    settings.orientation = ORIENTATION_PORTRAIT;
    if (!cached("a1.jpg"))
        fail("%s%s", "portrait missed its cache file", "");
    settings.orientation = ORIENTATION_LANDSCAPE;
}

int main(void)
{
    test_no_list();
    test_no_change();
    test_batch_alphabetical();
    test_new_is_normal_next();
    test_empty_card();
    test_empty_card_random();
    test_replaced();
    test_second_batch();
    test_batch_kept();
    test_batch_deleted();
    test_batch_random();
    test_photo_cap();
    test_prune();
    test_cache_folder();
    fprintf(stderr, failures ? "%d failures\n" : "all passed\n", failures);
    return failures;
}
