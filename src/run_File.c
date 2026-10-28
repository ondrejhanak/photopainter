#include "run_File.h"
#include "f_util.h"
#include "pico/stdlib.h"
#include "hw_config.h"
#include "diskio.h" // STA_NOINIT

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h> // strcasecmp()

// Frame state, hidden from users: a dot-folder (hidden on macOS) with the FAT hidden attribute (Windows)
#define STATE_DIR "/.photopainter"
#define STATE_FILE STATE_DIR "/state.txt"

char pathName[FF_LFN_BUF + 1]; // file name of the image to display, in the card root

static sd_card_t *sd_get_by_name(const char *const name)
{
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (0 == strcmp(sd_get_by_num(i)->pcName, name))
            return sd_get_by_num(i);
    // DBG_PRINTF("%s: unknown name %s\n", __func__, name);
    return NULL;
}

static FATFS *sd_get_fs_by_name(const char *name)
{
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (0 == strcmp(sd_get_by_num(i)->pcName, name))
            return &sd_get_by_num(i)->fatfs;
    // DBG_PRINTF("%s: unknown name %s\n", __func__, name);
    return NULL;
}

/*
    function:
        Mount an sd card
    parameter:
        none
*/
void run_mount()
{
    const char *arg1 = strtok(NULL, " ");
    if (!arg1)
        arg1 = sd_get_by_num(0)->pcName;
    FATFS *p_fs = sd_get_fs_by_name(arg1);
    if (!p_fs)
    {
        printf("Unknown logical drive number: \"%s\"\n", arg1);
        return;
    }
    FRESULT fr = f_mount(p_fs, arg1, 1);
    if (FR_OK != fr)
    {
        printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    sd_card_t *pSD = sd_get_by_name(arg1);
    // myASSERT(pSD);
    pSD->mounted = true;
}

/*
    function:
        Uninstalling an sd card
    parameter:
        none
*/
void run_unmount()
{
    const char *arg1 = strtok(NULL, " ");
    if (!arg1)
        arg1 = sd_get_by_num(0)->pcName;
    FATFS *p_fs = sd_get_fs_by_name(arg1);
    if (!p_fs)
    {
        printf("Unknown logical drive number: \"%s\"\n", arg1);
        return;
    }
    FRESULT fr = f_unmount(arg1);
    if (FR_OK != fr)
    {
        printf("f_unmount error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    sd_card_t *pSD = sd_get_by_name(arg1);
    // myASSERT(pSD);
    pSD->mounted = false;
}

/*
    function:
        TF card mounting test
    parameter:
        none
*/
char sdTest(void)
{
    sd_card_t *pSD = sd_get_by_num(0);
    // The board has no card-detect pin, so the driver never notices a removed card and
    // would keep skipping initialization. Force it, so a re-inserted card works again.
    pSD->m_Status |= STA_NOINIT;
    FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
    if (FR_OK != fr)
    {
        return 1;
    }
    else
    {
        f_unmount(pSD->pcName);
        return 0;
    }
}

// A displayable image: a .bmp file, not a folder, not hidden/system, and not a dot-file
// (skips macOS "._" companion files and similar)
static bool isImage(const FILINFO *fno)
{
    if (fno->fattrib & (AM_DIR | AM_HID | AM_SYS))
        return false;
    if (fno->fname[0] == '.')
        return false;
    size_t len = strlen(fno->fname);
    return len > 4 && strcasecmp(fno->fname + len - 4, ".bmp") == 0;
}

// Natural, case-insensitive order: "Photo 2" < "Photo 10", "beach" next to "Beach".
// Names that only differ in case or leading zeros fall back to a plain compare, so the order is stable.
static int naturalCompare(const char *a, const char *b)
{
    const char *pa = a, *pb = b;
    while (*pa && *pb)
    {
        if (isdigit((unsigned char)*pa) && isdigit((unsigned char)*pb))
        {
            while (*pa == '0')
                pa++;
            while (*pb == '0')
                pb++;
            const char *ea = pa, *eb = pb;
            while (isdigit((unsigned char)*ea))
                ea++;
            while (isdigit((unsigned char)*eb))
                eb++;
            if (ea - pa != eb - pb)
                return (ea - pa) < (eb - pb) ? -1 : 1; // more digits = larger number
            int c = strncmp(pa, pb, ea - pa);
            if (c)
                return c;
            pa = ea;
            pb = eb;
        }
        else
        {
            int ca = tolower((unsigned char)*pa), cb = tolower((unsigned char)*pb);
            if (ca != cb)
                return ca - cb;
            pa++;
            pb++;
        }
    }
    if (*pa || *pb)
        return *pa ? 1 : -1;
    return strcmp(a, b);
}

// Read the last shown image name from the state file; empty if there is none yet
static void readLastShown(char *name, int len)
{
    name[0] = '\0';
    FIL fil;
    if (f_open(&fil, STATE_FILE, FA_READ) != FR_OK)
        return;
    if (f_gets(name, len, &fil))
        name[strcspn(name, "\r\n")] = '\0';
    f_close(&fil);
}

static void writeLastShown(const char *name)
{
    if (f_mkdir(STATE_DIR) == FR_OK)
        f_chmod(STATE_DIR, AM_HID, AM_HID);

    FIL fil;
    FRESULT fr = f_open(&fil, STATE_FILE, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK)
    {
        printf("cannot write %s: %s (%d)\r\n", STATE_FILE, FRESULT_str(fr), fr);
        return;
    }
    f_puts(name, &fil);
    f_puts("\n", &fil);
    f_close(&fil);
}

/*
    function:
        Pick the image after the last shown one (natural order, wrapping around) into pathName.
        One pass over the card root; adding or removing images never causes skips or repeats.
    return:
        false if the card root contains no images
*/
bool selectNextImage(void)
{
    static char last[FF_LFN_BUF + 1];
    static char first[FF_LFN_BUF + 1];
    static char next[FF_LFN_BUF + 1];
    first[0] = '\0';
    next[0] = '\0';

    run_mount();
    readLastShown(last, sizeof(last));

    DIR dir;
    FILINFO fno;
    FRESULT fr = f_opendir(&dir, "/");
    if (fr == FR_OK)
    {
        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
        {
            if (!isImage(&fno))
                continue;
            if (!first[0] || naturalCompare(fno.fname, first) < 0)
                strcpy(first, fno.fname);
            if (last[0] && naturalCompare(fno.fname, last) > 0 && (!next[0] || naturalCompare(fno.fname, next) < 0))
                strcpy(next, fno.fname);
        }
        f_closedir(&dir);
    }
    else
    {
        printf("f_opendir error: %s (%d)\r\n", FRESULT_str(fr), fr);
    }
    run_unmount();

    if (!first[0])
    {
        pathName[0] = '\0';
        printf("no images in the card root\r\n");
        return false;
    }
    strcpy(pathName, next[0] ? next : first);
    printf("next image: %s (last shown: %s)\r\n", pathName, last[0] ? last : "-");
    return true;
}

// Remember pathName as shown, so the next selection continues after it
void saveShownImage(void)
{
    run_mount();
    writeLastShown(pathName);
    run_unmount();
}
