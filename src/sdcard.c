#include "sdcard.h"
#include "f_util.h"
#include "hw_config.h"
#include "diskio.h" // STA_NOINIT

#include <stdio.h>

// The frame has a single card slot: card 0 in hw_config.c
void sdcard_mount(void)
{
    sd_card_t *sd = sd_get_by_num(0);
    FRESULT fr = f_mount(&sd->fatfs, sd->pcName, 1);
    if (fr != FR_OK)
        printf("f_mount error: %s (%d)\r\n", FRESULT_str(fr), fr);
}

void sdcard_unmount(void)
{
    sd_card_t *sd = sd_get_by_num(0);
    FRESULT fr = f_unmount(sd->pcName);
    if (fr != FR_OK)
        printf("f_unmount error: %s (%d)\r\n", FRESULT_str(fr), fr);
}

bool sdcard_present(void)
{
    sd_card_t *sd = sd_get_by_num(0);
    // The board has no card-detect pin, so the driver never notices a removed card and
    // would keep skipping initialization. Force it, so a re-inserted card works again.
    sd->m_Status |= STA_NOINIT;
    if (f_mount(&sd->fatfs, sd->pcName, 1) != FR_OK)
        return false;
    f_unmount(sd->pcName);
    return true;
}
