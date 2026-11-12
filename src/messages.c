#include "messages.h"
#include "settings.h"

#include <assert.h>
#include <stdio.h>


const char *const language_codes[] = {
    [LANG_EN] = "en",
    [LANG_CS] = "cs",
};
static_assert(sizeof(language_codes) / sizeof(language_codes[0]) == LANG_COUNT, "language code missing");

// UTF-8; letters missing from the font draw as their base letter (č -> c).
// Titles without a full stop, other lines are sentences with one. Message screens fit about 48
// characters for a title and 61 for other lines
static const char *const messages[][MSG_COUNT] = {
    [LANG_EN] = {
        [MSG_NO_CARD_TITLE] = "SD Card Not Found or Unreadable",
        [MSG_NO_CARD_HINT] = "Insert an SD card or try another one.",
        [MSG_NO_IMAGES_TITLE] = "No Images Found",
        [MSG_NO_IMAGES_HINT] = "Copy JPEG photos directly onto the card.",
        [MSG_CANNOT_SHOW_TITLE] = "Photos That Cannot Be Shown",
        [MSG_BATTERY_EMPTY_TITLE] = "Battery Empty",
        [MSG_BATTERY_EMPTY_HINT] = "Charge the frame with a USB cable.",
        [MSG_DRIVE_TITLE] = "USB Drive",
        [MSG_DRIVE_HINT] = "Copy JPEG photos to the drive, then eject it.",
        [MSG_FIRMWARE] = "Firmware",
    },
    [LANG_CS] = {
        [MSG_NO_CARD_TITLE] = "SD karta nenalezena nebo nečitelná",
        [MSG_NO_CARD_HINT] = "Vložte SD kartu nebo zkuste jinou.",
        [MSG_NO_IMAGES_TITLE] = "Nenalezeny žádné obrázky",
        [MSG_NO_IMAGES_HINT] = "Nakopírujte fotky JPEG přímo na kartu.",
        [MSG_CANNOT_SHOW_TITLE] = "Fotky, které nelze zobrazit",
        [MSG_BATTERY_EMPTY_TITLE] = "Baterie je vybitá",
        [MSG_BATTERY_EMPTY_HINT] = "Nabijte rámeček USB kabelem.",
        [MSG_DRIVE_TITLE] = "USB disk",
        [MSG_DRIVE_HINT] = "Nakopírujte fotky JPEG na disk a pak ho vysuňte.",
        [MSG_FIRMWARE] = "Firmware",
    },
};
static_assert(sizeof(messages) / sizeof(messages[0]) == LANG_COUNT, "language without messages");

const char *msg(MsgId id)
{
    const char *text = messages[settings.language][id];
    return text ? text : messages[LANG_EN][id]; // a forgotten translation shows English, not a crash
}

void msg_date(char *out, size_t len, int year, int month, int day)
{
    static const char *const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (settings.language == LANG_CS)
        snprintf(out, len, "%d. %d. %d", day, month, year);
    else // the month as a word: no day/month order to guess
        snprintf(out, len, "%d %s %d", day, months[(month - 1) % 12], year);
}
