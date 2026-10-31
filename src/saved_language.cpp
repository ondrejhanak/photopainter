// The language kept in the CPU flash (Arduino-Pico's EEPROM emulation in the last flash sector,
// which flashing new firmware leaves alone), so screens shown without a readable card use it too
#include "saved_language.h"
#include <EEPROM.h>
#include <stdio.h>

#define MAGIC 0x4C // marks a saved value; an erased sector reads 0xFF

int saved_language_load(void)
{
    EEPROM.begin(256);
    return EEPROM.read(0) == MAGIC ? EEPROM.read(1) : -1;
}

void saved_language_save(int language)
{
    if (saved_language_load() == language)
        return;
    EEPROM.write(0, MAGIC);
    EEPROM.write(1, language);
    EEPROM.commit();
    printf("settings: language saved to flash\r\n");
}
