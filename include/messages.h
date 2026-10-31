#ifndef MESSAGES_H
#define MESSAGES_H

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
    MSG_CANNOT_DISPLAY,
    MSG_LOW_VOLTAGE,
    MSG_BATTERY,
    MSG_DRIVE_TITLE,
    MSG_DRIVE_HINT,
    MSG_CONNECT_TITLE,
    MSG_CONNECT_HINT,
    MSG_COUNT
} MsgId;

// settings.ini value of each language ("en", "cs")
extern const char *const language_codes[LANG_COUNT];

// Text in the language from settings.ini (UTF-8)
const char *msg(MsgId id);

#endif
