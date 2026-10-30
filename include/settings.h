#ifndef SETTINGS_H
#define SETTINGS_H

#include "messages.h" // Language

typedef enum
{
    ORDER_ALPHABETICAL, // natural name order
    ORDER_RANDOM, // shuffled; every image once per round, reshuffled after each round
} ImageOrder;

// User settings, read from settings.ini in the SD card root
typedef struct
{
    int interval_hours; // hours between image changes (RTC wake-up), 1..47
    ImageOrder order;
    Language language; // on-screen messages
} Settings;

extern Settings settings;

// Load settings.ini (creating a commented default file if it is missing).
// Missing or invalid values keep their defaults. Expects the SD card to be present.
void settings_load(void);

#endif
