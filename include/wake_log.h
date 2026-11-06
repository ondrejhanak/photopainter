#ifndef WAKE_LOG_H
#define WAKE_LOG_H

#include "ff.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// One CSV line per event in /.photopainter/log.csv, rotated to log.old.csv; columns and values are
// described in docs/wake-log.md (a host test checks that the header there matches)
#define WAKE_LOG_HEADER "event,reason,version,battery_v,usb,charging,interval_h,boot_ms,card_ms,select_ms,load_ms,refresh_ms," \
                        "photo,result,photos,failed"

// Steps of a wake or photo change; the current one is kept in watchdog scratch 0, so a watchdog
// reset can name it on the next boot. Numbers are in docs/wake-log.md.
typedef enum
{
    STEP_IDLE,
    STEP_BOOT,
    STEP_CARD,
    STEP_SELECT,
    STEP_LOAD,
    STEP_REFRESH,
    STEP_SAVE,
    STEP_DRIVE,
    STEP_COUNT
} WakeStep;

// Filled during a wake or photo change, written and cleared by wake_log_write()
typedef struct
{
    const char *reason; // rtc, key, key_long, serial, eject, drive_timeout, usb_plugged, watchdog:<step>
    float battery_v;
    bool usb, charging;
    int interval_h; // interval_hours from settings.ini; 0 until it is read
    uint32_t ms[STEP_COUNT]; // time per step
    char photo[FF_LFN_BUF + 1];
    const char *result; // cache, converted, message
    int photos, failed;
} WakeLog;

extern WakeLog wake_log;

// Start a step: the time since the previous one is added to that step
void wake_log_step(WakeStep step);
// The reason of this boot: watchdog:<step> after a watchdog reset, else `otherwise`
const char *wake_log_boot_reason(const char *otherwise);
// One CSV line (without newline) for event; returns false if it does not fit
bool wake_log_format(char *out, size_t len, const char *event, const WakeLog *w);
// Append the line for event to the log (rotating it when full), then clear the per-change fields.
// Mounts the card itself.
void wake_log_write(const char *event);

#endif
