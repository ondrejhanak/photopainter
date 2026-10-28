#ifndef _SETTINGS_H_
#define _SETTINGS_H_

// User settings, read from settings.ini in the SD card root
typedef struct
{
    int interval_hours; // hours between image changes (RTC wake-up), 1..47
} Settings;

extern Settings settings;

// Load settings.ini (creating a commented default file if it is missing).
// Missing or invalid values keep their defaults. Expects the SD card to be present.
void settings_load(void);

#endif
