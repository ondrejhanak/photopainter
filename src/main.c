#include "EPD_Test.h" // Examples
#include "run_File.h"
#include "led.h"
#include "waveshare_PCF85063.h" // RTC
#include "DEV_Config.h"
#include <time.h>

extern const char *fileList;
extern char pathName[];

/*
Mode 0: Automatically get pic folder names and sort them
Mode 1: Automatically get pic folder names but not sorted
Mode 2: pic folder name is not automatically obtained, users need to create fileList.txt file and write the picture name in TF card by themselves
*/
#define Mode 0

// State shared by setup() and loop()
static Time_data Time = {2024 - 2000, 3, 31, 0, 0, 0};
static Time_data alarmTime;
static char isCard = 0;
static bool chargingMode = false; // powered over USB at boot; loop() only acts in this mode

float measureVBAT(void)
{
    float Voltage = 0.0;
    const float conversion_factor = 3.3f / (1 << 12);
    uint16_t result = adc_read();
    Voltage = result * conversion_factor * 3;
    printf("Raw value: 0x%03x, voltage: %f V\n", result, Voltage);
    return Voltage;
}

void chargeState_callback()
{
    if (DEV_Digital_Read(VBUS))
    {
        if (!DEV_Digital_Read(CHARGE_STATE))
        { // is charging
            ledCharging();
        }
        else
        { // charge complete
            ledCharged();
        }
    }
}

void run_display(void)
{
    if (isCard)
    {
        setFilePath();
        EPD_7in3f_display_BMP(pathName, measureVBAT()); // display bmp
    }
    else
    {
        EPD_7in3f_display(measureVBAT());
    }

    PCF85063_clear_alarm_flag();  // clear RTC alarm flag
    rtcRunAlarm(Time, alarmTime); // RTC run alarm
}

// Detect the SD card and (re)build the image list, so a card inserted or changed while on USB is picked up
static void scanCard(void)
{
    if (!sdTest())
    {
        isCard = 1;
        if (Mode == 0)
        {
            sdScanDir();
            file_sort();
        }
        if (Mode == 1)
        {
            sdScanDir();
        }
        if (Mode == 2)
        {
            file_cat();
        }
    }
    else
    {
        isCard = 0;
    }
}

/*
Arduino-Pico entry points. The core's own main() starts USB serial and then calls
setup()/loop(), so the firmware runs from setup() instead of defining main().
*/
void setup(void)
{
    alarmTime = Time;
    // alarmTime.seconds += 10;
    // alarmTime.minutes += 15;
    alarmTime.hours += 12;

    printf("Init...\r\n");
    if (DEV_Module_Init() != 0)
    { // DEV init
        return;
    }

    watchdog_enable(8 * 1000, 1); // 8s
    DEV_Delay_ms(1000);
    PCF85063_init();              // RTC init
    rtcRunAlarm(Time, alarmTime); // RTC run alarm
    gpio_set_irq_enabled_with_callback(CHARGE_STATE, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, chargeState_callback);

    if (measureVBAT() < 3.1)
    { // battery power is low
        printf("low power ...\r\n");
        PCF85063_alarm_Time_Disable();
        ledLowPower(); // LED flash for Low power
        powerOff();    // BAT off
        return;
    }
    else
    {
        printf("work ...\r\n");
        ledPowerOn();
    }

    scanCard();

    if (!DEV_Digital_Read(VBUS))
    { // on battery: show the next image and cut power until the RTC alarm wakes us up
        run_display();
        printf("power off ...\r\n");
        powerOff(); // BAT off
        return;
    }

    // on USB power: stay awake, loop() handles KEY presses until USB is unplugged
    chargeState_callback();
    chargingMode = true;
}

void loop(void)
{
    if (!chargingMode)
        return;

    if (!DEV_Digital_Read(VBUS))
    { // USB unplugged
        printf("power off ...\r\n");
        powerOff(); // BAT off
        chargingMode = false;
        return;
    }

    // KEY and the RTC alarm both pull BAT_STATE low on this board (both feed the power-on
    // circuit that wakes the device on battery), so this also rotates on the RTC schedule
    if (!DEV_Digital_Read(BAT_STATE))
    {
        printf("key or rtc interrupt\r\n");
        scanCard();
        run_display();
    }
    DEV_Delay_ms(200);
}
