#ifndef MESSAGES_H
#define MESSAGES_H

#include <stddef.h>

typedef enum
{
    LANG_EN,
    LANG_CS,
    LANG_COUNT
} Language;

// User-visible texts, translated in messages.c
typedef enum
{
    MSG_NO_CARD_TITLE,
    MSG_NO_CARD_HINT,
    MSG_NO_IMAGES_TITLE,
    MSG_NO_IMAGES_HINT,
    MSG_CANNOT_SHOW_TITLE,
    MSG_BATTERY_EMPTY_TITLE,
    MSG_BATTERY_EMPTY_HINT,
    MSG_DRIVE_TITLE,
    MSG_DRIVE_HINT,
    MSG_FIRMWARE, // followed by the version on the USB Drive screen
    MSG_COUNT
} MsgId;

// settings.ini value of each language ("en", "cs")
extern const char *const language_codes[LANG_COUNT];

// Text in the language from settings.ini (UTF-8)
const char *msg(MsgId id);
// A date as the language writes it: "24. 12. 1985" (cs), "24 Dec 1985" (en); month is 1..12
void msg_date(char *out, size_t len, int year, int month, int day);

#endif
