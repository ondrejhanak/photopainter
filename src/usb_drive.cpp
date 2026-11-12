// The SD card as a USB drive next to the serial port: always attached, empty like a card reader until
// drive mode inserts the card. Don't re-enumerate the device instead: TinyUSB then panics now and then.
// In drive mode tud_task() runs only from the firmware, not the core's USB interrupt, as the SD driver
// waits for DMA interrupts; outside it the callbacks run in that interrupt and must not touch the card.
// The callbacks run with USB.mutex held, so a printf() in them is dropped: note it and print afterwards
#include "usb_drive.h"
#include "work_memory.h"
#include <Arduino.h>
#include <USB.h>
#include <tusb-msc.h> // links TinyUSB's MSC class; without it the core's empty stubs answer and no drive appears
#include <hardware/watchdog.h>

extern "C" {
#include "hw_config.h"
#include "diskio.h" // STA_NOINIT
#include "sd_card.h"
#include "volume_flag.h"
}

#define SECTOR_SIZE 512
// Sectors per card command; computers read and write files in long runs, and a card takes
// milliseconds per write command however short
#define READ_AHEAD 32
#define WRITE_BATCH 64

static volatile bool active = false; // drive mode: the medium is present
static volatile bool media_changed = false; // tell the computer once that the medium appeared
static volatile bool ejected = false;
static bool card_failed = false; // a write failed after its status was sent, fail everything after it
static uint8_t descriptor[TUD_MSC_DESC_LEN];
static uint32_t sector_count;
static volatile uint32_t last_command_ms; // millis() of the computer's last drive command
static volatile bool used = false; // the computer read the drive (mounting always does)
static VolumeFlag volume_flag;
static bool volume_flag_known = false; // a FAT32 or exFAT card, whose flag tells an unmount
static volatile bool unmounted = false; // the computer's last write of the flag marked the card clean

// In work_memory, which JPEG conversion overwrites outside drive mode
static uint8_t *const read_buffer = work_memory;
static uint32_t read_lba, read_sectors; // sectors in read_buffer
static uint8_t *const write_buffer = work_memory + READ_AHEAD * SECTOR_SIZE;
static_assert((READ_AHEAD + WRITE_BATCH) * SECTOR_SIZE <= WORK_MEMORY_SIZE, "drive buffers exceed work memory");
static uint32_t write_lba, write_sectors; // complete sectors in write_buffer, not yet on the card

static bool flush_writes(void)
{
    if (write_sectors > 0 &&
        sd_write_blocks(sd_get_by_num(0), write_buffer, write_lba, write_sectors) != SD_BLOCK_DEVICE_ERROR_NONE)
        card_failed = true;
    write_sectors = 0;
    return !card_failed;
}

static void run_usb(uint32_t ms)
{
    uint32_t start = millis();
    while (millis() - start < ms)
    {
        mutex_enter_blocking(&USB.mutex);
        tud_task();
        mutex_exit(&USB.mutex);
    }
    watchdog_update();
}

// Arduino-Pico hook right before USB starts, so the drive is part of the first enumeration
void initVariant(void)
{
    const uint8_t msc[] = {TUD_MSC_DESCRIPTOR(0 /* set by simpleInterface */, 0, USB.registerEndpointOut(),
                                              USB.registerEndpointIn(), 64)};
    memcpy(descriptor, msc, sizeof(descriptor));
    USB.registerInterface(1, USBClass::simpleInterface, descriptor, sizeof(descriptor), 2, 0);
}

bool usb_drive_computer_connected(void)
{
    return tud_ready(); // configured by a computer and not suspended (a sleeping computer counts as none)
}

bool usb_drive_start(void)
{
    if (active)
        return true;
    sd_card_t *sd = sd_get_by_num(0);
    sd->m_Status |= STA_NOINIT; // fresh init, the card may have been changed (no card-detect pin)
    if (sd_init_card(sd) & STA_NOINIT)
        return false;
    sector_count = sd_sectors(sd);
    if (sector_count == 0)
        return false;
    uint32_t boot_lba;
    volume_flag_known = sd_read_blocks(sd, read_buffer, 0, 1) == SD_BLOCK_DEVICE_ERROR_NONE &&
                        volume_boot_lba(read_buffer, &boot_lba) &&
                        sd_read_blocks(sd, read_buffer, boot_lba, 1) == SD_BLOCK_DEVICE_ERROR_NONE &&
                        volume_flag_locate(read_buffer, boot_lba, &volume_flag);
    unmounted = false;
    read_sectors = 0;
    write_sectors = 0;
    card_failed = false;
    ejected = false;
    used = false;
    last_command_ms = millis();

    irq_set_enabled(USB.usbTaskIRQ, false); // before active: the callbacks may use the card from now on
    media_changed = true;
    active = true;
    printf("usb drive: %lu sectors\r\n", (unsigned long)sector_count);
    return true;
}

void usb_drive_stop(void)
{
    if (!active)
        return;
    flush_writes();
    active = false; // the computer sees "medium not present" from its next command on
    irq_set_enabled(USB.usbTaskIRQ, true);
    if (card_failed)
        printf("usb drive: card write failed\r\n");
}

void usb_drive_serve(uint32_t ms)
{
    run_usb(ms);
}

bool usb_drive_ejected(void)
{
    return ejected;
}

bool usb_drive_used(void)
{
    return used;
}

bool usb_drive_unmounted(void)
{
    return unmounted;
}

uint32_t usb_drive_idle_ms(void)
{
    return millis() - last_command_ms;
}

extern "C" void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
    (void)lun;
    last_command_ms = millis();
    memcpy(vendor_id, "Waveshar", 8);
    memcpy(product_id, "PhotoPainter    ", 16);
    memcpy(product_rev, "1.0 ", 4);
}

extern "C" bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
    last_command_ms = millis();
    if (!active || ejected)
    { // empty card reader, also right after an eject
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00); // medium not present
        return false;
    }
    if (media_changed)
    { // the computer re-reads the capacity and mounts the drive
        media_changed = false;
        tud_msc_set_sense(lun, SCSI_SENSE_UNIT_ATTENTION, 0x28, 0x00); // medium may have changed
        return false;
    }
    return true;
}

extern "C" void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{
    (void)lun;
    last_command_ms = millis();
    *block_count = active ? sector_count : 0; // 0 = medium not present
    *block_size = SECTOR_SIZE;
}

// TinyUSB hands over at most 64 bytes per call (CFG_TUD_MSC_EP_BUFSIZE), offset within the sector lba
extern "C" int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
    (void)lun;
    last_command_ms = millis();
    used = true;
    if (!active || lba >= sector_count || !flush_writes())
        return -1;
    if (lba < read_lba || lba >= read_lba + read_sectors)
    {
        uint32_t count = sector_count - lba < READ_AHEAD ? sector_count - lba : READ_AHEAD;
        read_sectors = 0;
        if (sd_read_blocks(sd_get_by_num(0), read_buffer, lba, count) != SD_BLOCK_DEVICE_ERROR_NONE)
            return -1;
        read_lba = lba;
        read_sectors = count;
    }
    memcpy(buffer, read_buffer + (lba - read_lba) * SECTOR_SIZE + offset, bufsize);
    return bufsize;
}

// WRITE10 carries whole sectors. Consecutive sectors are collected and written to the card in one
// command when the batch is full or the computer's write command is complete
extern "C" int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
    (void)lun;
    last_command_ms = millis();
    if (!active || card_failed || lba >= sector_count)
        return -1;
    read_sectors = 0; // the read-ahead may hold the old content
    if (offset == 0 && (write_sectors == WRITE_BATCH || (write_sectors > 0 && lba != write_lba + write_sectors)) &&
        !flush_writes())
        return -1;
    if (write_sectors == 0)
        write_lba = lba;
    memcpy(write_buffer + write_sectors * SECTOR_SIZE + offset, buffer, bufsize);
    int clean = volume_flag_known ? volume_flag_clean(&volume_flag, lba, offset, buffer, bufsize) : -1;
    if (clean >= 0)
        unmounted = clean;
    if (offset + bufsize == SECTOR_SIZE)
        write_sectors++;
    return bufsize;
}

// After the status of a write command went to the computer, before its next command
extern "C" void tud_msc_write10_complete_cb(uint8_t lun)
{
    (void)lun;
    flush_writes();
}

// Eject on the computer
extern "C" bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
    (void)lun;
    last_command_ms = millis();
    (void)power_condition;
    if (load_eject && !start)
        ejected = true;
    return true;
}

extern "C" int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize)
{
    last_command_ms = millis();
    (void)buffer;
    (void)bufsize;
    if (scsi_cmd[0] == 0x35 && flush_writes()) // SYNCHRONIZE CACHE(10)
        return 0;
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00); // invalid command
    return -1;
}
