#include "volume_flag.h"

#include <string.h>

static uint32_t le32(const uint8_t *p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

static bool is_fat32(const uint8_t *s)
{
    return memcmp(s + 82, "FAT32   ", 8) == 0;
}

static bool is_exfat(const uint8_t *s)
{
    return memcmp(s + 3, "EXFAT   ", 8) == 0;
}

bool volume_boot_lba(const uint8_t sector0[512], uint32_t *lba)
{
    if (is_fat32(sector0) || is_exfat(sector0))
    {
        *lba = 0;
        return true;
    }
    if (sector0[510] != 0x55 || sector0[511] != 0xAA) // no MBR
        return false;
    *lba = le32(sector0 + 446 + 8);
    return true;
}

bool volume_flag_locate(const uint8_t boot[512], uint32_t boot_lba, VolumeFlag *flag)
{
    if (is_fat32(boot)) // entry 1 of the first FAT, bit 27 of its 32-bit value
    {
        uint16_t reserved = boot[14] | boot[15] << 8;
        *flag = (VolumeFlag){.lba = boot_lba + reserved, .offset = 4 + 3, .mask = 0x08, .set_when_clean = true};
        return true;
    }
    if (is_exfat(boot)) // VolumeFlags, bit 1
    {
        *flag = (VolumeFlag){.lba = boot_lba, .offset = 106, .mask = 0x02, .set_when_clean = false};
        return true;
    }
    return false;
}

int volume_flag_clean(const VolumeFlag *flag, uint32_t lba, uint32_t offset, const uint8_t *data, uint32_t len)
{
    if (lba != flag->lba || flag->offset < offset || flag->offset >= offset + len)
        return -1;
    bool set = data[flag->offset - offset] & flag->mask;
    return set == flag->set_when_clean;
}
