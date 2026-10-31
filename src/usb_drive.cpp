// The SD card as a USB mass storage drive next to the serial port, so photos can be copied from a
// computer. TinyUSB calls the tud_msc_* callbacks below from tud_task(). In drive mode only the
// firmware runs tud_task() (usb_drive_serve(), Serial), never the core's periodic USB interrupt:
// the SD driver waits for DMA interrupts, which cannot arrive inside another interrupt
#include "usb_drive.h"
#include <Arduino.h>
#include <USB.h>
#include <hardware/watchdog.h>

extern "C" {
#include "hw_config.h"
#include "diskio.h" // STA_NOINIT
#include "sd_card.h"
}

#define SECTOR_SIZE 512
// Sectors per card command; computers read and write files in long runs, and a card takes
// milliseconds per write command however short
#define READ_AHEAD 32
#define WRITE_BATCH 64

static bool active = false;
static volatile bool ejected = false;
static volatile bool host_configured = false; // set by tud_mount_cb()
static bool card_failed = false; // a write failed after its status was sent, fail everything after it
static uint8_t interface_id, endpoint_in, endpoint_out;
static uint8_t descriptor[TUD_MSC_DESC_LEN];
static uint32_t sector_count;

static uint8_t read_buffer[READ_AHEAD * SECTOR_SIZE];
static uint32_t read_lba, read_sectors; // sectors in read_buffer
static uint8_t write_buffer[WRITE_BATCH * SECTOR_SIZE];
static uint32_t write_lba, write_sectors; // complete sectors in write_buffer, not yet on the card

static bool flush_writes(void)
{
    if (write_sectors > 0 &&
        sd_write_blocks(sd_get_by_num(0), write_buffer, write_lba, write_sectors) != SD_BLOCK_DEVICE_ERROR_NONE)
        card_failed = true;
    write_sectors = 0;
    return !card_failed;
}

// Run TinyUSB for ms milliseconds
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

// USB.disconnect() only drops the pull-up: TinyUSB keeps the old connection (configured, serial port
// open) until the computer resets the bus, and printf in between can stall. Reconnect and wait until
// the computer has configured the device again (up to 5 s, it may be slow or gone)
static void reconnect(void)
{
    host_configured = false;
    USB.connect();
    uint32_t start = millis();
    while (!host_configured && millis() - start < 5000)
        run_usb(10);
}

// Invoked by TinyUSB when the computer configures the device (after its bus reset)
extern "C" void tud_mount_cb(void)
{
    host_configured = true;
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
    read_sectors = 0;
    write_sectors = 0;
    card_failed = false;
    ejected = false;

    irq_set_enabled(USB.usbTaskIRQ, false);
    // The computer sees the new interface only after the device reconnects
    USB.disconnect();
    endpoint_in = USB.registerEndpointIn();
    endpoint_out = USB.registerEndpointOut();
    const uint8_t msc[] = {TUD_MSC_DESCRIPTOR(0 /* set by simpleInterface */, 0, endpoint_out, endpoint_in, 64)};
    memcpy(descriptor, msc, sizeof(descriptor));
    interface_id = USB.registerInterface(1, USBClass::simpleInterface, descriptor, sizeof(descriptor), 2, 0);
    active = true;
    reconnect();
    printf("usb drive: %lu sectors\r\n", (unsigned long)sector_count);
    return true;
}

void usb_drive_stop(void)
{
    if (!active)
        return;
    flush_writes();
    USB.disconnect();
    USB.unregisterInterface(interface_id);
    USB.unregisterEndpointIn(endpoint_in);
    USB.unregisterEndpointOut(endpoint_out);
    active = false;
    reconnect();
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

extern "C" void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
    (void)lun;
    memcpy(vendor_id, "Waveshar", 8);
    memcpy(product_id, "PhotoPainter    ", 16);
    memcpy(product_rev, "1.0 ", 4);
}

extern "C" bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
    (void)lun;
    return active && !ejected; // after an eject: "medium not present", so the computer lets go
}

extern "C" void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{
    (void)lun;
    *block_count = sector_count;
    *block_size = SECTOR_SIZE;
}

// TinyUSB hands over at most 64 bytes per call (CFG_TUD_MSC_EP_BUFSIZE), offset within the sector lba
extern "C" int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
    (void)lun;
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
    if (!active || card_failed || lba >= sector_count)
        return -1;
    read_sectors = 0; // the read-ahead may hold the old content
    if (offset == 0 && (write_sectors == WRITE_BATCH || (write_sectors > 0 && lba != write_lba + write_sectors)) &&
        !flush_writes())
        return -1;
    if (write_sectors == 0)
        write_lba = lba;
    memcpy(write_buffer + write_sectors * SECTOR_SIZE + offset, buffer, bufsize);
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
    (void)power_condition;
    if (load_eject && !start)
        ejected = true;
    return true;
}

// SCSI commands TinyUSB does not handle itself
extern "C" int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize)
{
    (void)buffer;
    (void)bufsize;
    if (scsi_cmd[0] == 0x35 && flush_writes()) // SYNCHRONIZE CACHE(10)
        return 0;
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00); // invalid command
    return -1;
}
