#ifndef VOLUME_FLAG_H
#define VOLUME_FLAG_H

#include <stdbool.h>
#include <stdint.h>

// The flag a computer sets while it has the card mounted and clears when it unmounts it cleanly:
// FAT32's clean-shutdown bit in FAT entry 1, exFAT's VolumeDirty bit. Seeing it cleared tells an
// unmount without an eject (e.g. ForkLift) from a drive that is merely idle.
typedef struct
{
    uint32_t lba; // sector holding the flag
    uint16_t offset; // byte of the flag within that sector
    uint8_t mask; // the flag's bit within that byte
    bool set_when_clean; // FAT32 sets its bit on unmount, exFAT clears its bit
} VolumeFlag;

// Sector of the volume's boot sector, from the card's first sector: 0 if it is one itself, else the
// first partition of its MBR; false if neither
bool volume_boot_lba(const uint8_t sector0[512], uint32_t *lba);
// Where the flag of a FAT32 or exFAT volume is, from its boot sector; false for other file systems
bool volume_flag_locate(const uint8_t boot[512], uint32_t boot_lba, VolumeFlag *flag);
// The flag in written data (len bytes at offset within sector lba): 1 clean, 0 in use, -1 not in it
int volume_flag_clean(const VolumeFlag *flag, uint32_t lba, uint32_t offset, const uint8_t *data, uint32_t len);

#endif
