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

// Take the language of the last settings.ini read (saved in flash), for screens shown before or
// without reading the card
void settings_init(void);
// Load settings.ini (creating a commented default file if it is missing).
// Missing or invalid values keep their defaults; the language is kept if the file cannot be read.
// Expects the SD card to be present.
void settings_load(void);

#endif
