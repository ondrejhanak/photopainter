#include "settings.h"
#include "sdcard.h"
#include "ff.h"
#include "ini.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // strcasecmp()

#define SETTINGS_FILE "settings.ini"
#define DEFAULT_INTERVAL_HOURS 12
#define DEFAULT_ORDER ORDER_ALPHABETICAL
#define DEFAULT_LANGUAGE LANG_EN

Settings settings = {
    .interval_hours = DEFAULT_INTERVAL_HOURS,
    .order = DEFAULT_ORDER,
    .language = DEFAULT_LANGUAGE,
};

// Written when settings.ini is missing, so users can see what can be changed. Comments are English
// except for language (Czech too), since the language is only known after reading this file
static const char *default_settings =
    "; PhotoPainter settings\r\n"
    "; Change a value after \"=\" and save the file. Changes apply with the next image change\r\n"
    "; (scheduled, or press KEY). Lines starting with ; are comments.\r\n"
    "\r\n"
    "; Hours between image changes (1 to 47)\r\n"
    "interval_hours = 12\r\n"
    "\r\n"
    "; Image order: alphabetical or random (random shows every image once before reshuffling)\r\n"
    "order = alphabetical\r\n"
    "\r\n"
    "; Language of on-screen messages: en (English) or cs (Czech)\r\n"
    "; Jazyk zpráv na displeji: en (angličtina) nebo cs (čeština)\r\n"
    "language = en\r\n";

// Parse a whole number in [min, max]; returns false for anything else (text, decimals, out of range)
static bool parse_int(const char *value, int min, int max, int *out)
{
    char *end;
    long n = strtol(value, &end, 10);
    if (end == value || *end != '\0' || n < min || n > max)
        return false;
    *out = (int)n;
    return true;
}

static int handler(void *user, const char *section, const char *name, const char *value)
{
    (void)user;
    (void)section; // sections are allowed but not used

    if (strcasecmp(name, "interval_hours") == 0)
    {
        if (!parse_int(value, 1, 47, &settings.interval_hours))
            printf("settings: invalid interval_hours \"%s\", using %d\r\n", value, settings.interval_hours);
    }
    else if (strcasecmp(name, "order") == 0)
    {
        if (strcasecmp(value, "alphabetical") == 0)
            settings.order = ORDER_ALPHABETICAL;
        else if (strcasecmp(value, "random") == 0)
            settings.order = ORDER_RANDOM;
        else
            printf("settings: invalid order \"%s\", using %s\r\n", value,
                   settings.order == ORDER_RANDOM ? "random" : "alphabetical");
    }
    else if (strcasecmp(name, "language") == 0)
    {
        int i = 0;
        while (i < LANG_COUNT && strcasecmp(value, language_codes[i]) != 0)
            i++;
        if (i < LANG_COUNT)
            settings.language = (Language)i;
        else
            printf("settings: invalid language \"%s\", using %s\r\n", value, language_codes[settings.language]);
    }
    else
    {
        printf("settings: unknown key \"%s\" ignored\r\n", name);
    }
    return 1; // keep parsing
}

// ini_reader adapter for FatFs
static char *read_line(char *str, int num, void *stream)
{
    return f_gets(str, num, (FIL *)stream);
}

static void write_default(void)
{
    FIL fil;
    UINT bw;
    if (f_open(&fil, SETTINGS_FILE, FA_CREATE_NEW | FA_WRITE) != FR_OK)
    {
        printf("settings: cannot create %s\r\n", SETTINGS_FILE);
        return;
    }
    f_write(&fil, default_settings, strlen(default_settings), &bw);
    f_close(&fil);
    printf("settings: created default %s\r\n", SETTINGS_FILE);
}

void settings_load(void)
{
    settings.interval_hours = DEFAULT_INTERVAL_HOURS;
    settings.order = DEFAULT_ORDER;
    settings.language = DEFAULT_LANGUAGE;

    sdcard_mount();

    FIL fil;
    FRESULT fr = f_open(&fil, SETTINGS_FILE, FA_READ);
    if (fr == FR_NO_FILE)
    {
        write_default();
    }
    else if (fr != FR_OK)
    {
        printf("settings: cannot open %s (%d), using defaults\r\n", SETTINGS_FILE, fr);
    }
    else
    {
        int err = ini_parse_stream(read_line, &fil, handler, NULL);
        if (err > 0)
            printf("settings: syntax error on line %d (other lines still applied)\r\n", err);
        f_close(&fil);
    }

    sdcard_unmount();

    printf("settings: interval_hours = %d, order = %s, language = %s\r\n", settings.interval_hours,
           settings.order == ORDER_RANDOM ? "random" : "alphabetical", language_codes[settings.language]);
}
