#include "sdcard.h"
#include "f_util.h"
#include "hw_config.h"
#include "diskio.h" // STA_NOINIT

#include <stdio.h>
#include <string.h>

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
void sdcard_mount(void)
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
void sdcard_unmount(void)
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
    return:
        true if the card mounts
*/
bool sdcard_present(void)
{
    sd_card_t *pSD = sd_get_by_num(0);
    // The board has no card-detect pin, so the driver never notices a removed card and
    // would keep skipping initialization. Force it, so a re-inserted card works again.
    pSD->m_Status |= STA_NOINIT;
    FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
    if (FR_OK != fr)
    {
        return false;
    }
    else
    {
        f_unmount(pSD->pcName);
        return true;
    }
}
