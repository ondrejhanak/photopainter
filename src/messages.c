#include "messages.h"
#include "settings.h"

#include <assert.h>

// Keep this file in UTF-8.

const char *const language_codes[] = {
    [LANG_EN] = "en",
    [LANG_CS] = "cs",
};
static_assert(sizeof(language_codes) / sizeof(language_codes[0]) == LANG_COUNT, "language code missing");

// UTF-8; letters missing from the font draw as their base letter (č -> c).
// Message screens fit about 39 characters for a title and 50 for other lines
static const char *const messages[][MSG_COUNT] = {
    [LANG_EN] = {
        [MSG_NO_CARD_TITLE] = "SD Card Not Found or Unreadable",
        [MSG_NO_CARD_HINT] = "Insert an SD card or try another one",
        [MSG_NO_IMAGES_TITLE] = "No Images Found",
        [MSG_NO_IMAGES_HINT] = "Copy .bmp images directly onto the card",
        [MSG_CANNOT_DISPLAY] = "Cannot display image:",
        [MSG_LOW_VOLTAGE] = "Low voltage, please charge in time",
        [MSG_BATTERY] = "Battery:",
    },
    [LANG_CS] = {
        [MSG_NO_CARD_TITLE] = "SD karta nenalezena nebo nečitelná",
        [MSG_NO_CARD_HINT] = "Vložte SD kartu nebo zkuste jinou",
        [MSG_NO_IMAGES_TITLE] = "Nenalezeny žádné obrázky",
        [MSG_NO_IMAGES_HINT] = "Nakopírujte obrázky .bmp přímo na kartu",
        [MSG_CANNOT_DISPLAY] = "Nelze zobrazit obrázek:",
        [MSG_LOW_VOLTAGE] = "Slabá baterie, prosím brzy nabijte",
        [MSG_BATTERY] = "Baterie:",
    },
};
static_assert(sizeof(messages) / sizeof(messages[0]) == LANG_COUNT, "language without messages");

const char *msg(MsgId id)
{
    const char *text = messages[settings.language][id];
    return text ? text : messages[LANG_EN][id]; // a forgotten translation shows English, not a crash
}
