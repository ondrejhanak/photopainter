#include "display.h"
#include "EPD_7in3f.h" // EPD_7IN3F_BusyWait()
#include "images.h"
#include "messages.h"
#include "sdcard.h"
#include "led.h"
#include "waveshare_PCF85063.h" // RTC
#include "DEV_Config.h"
#include "settings.h"
#include "usb_serial.h"
#include "usb_drive.h"
#include <time.h>

// State shared by setup() and loop()
// Arbitrary reference time (2000-01-01 00:00:00): the RTC is reset to it on every run and the
// alarm is set relative to it, so only the interval matters; the date is never read or shown
static Time_data reference_time = {0, 1, 1, 0, 0, 0};
static bool card_present = false;
static bool charging_mode = false; // powered over USB at boot; loop() only acts in this mode

// KEY on USB: short press (released before LONG_PRESS_MS) = next image, long press = drive mode on/off
#define LONG_PRESS_MS 3000
static bool key_down = false;
static uint32_t key_down_ms; // when KEY went down
static bool long_press_done = false; // long press already handled, wait for release

// Drive mode: the computer owns the SD card, the frame must not touch it (no display, no state.txt)
static bool drive_mode = false;

// Red LED: steady on in drive mode and once a long press is reached (until release), otherwise lit
// while the SD card is in use, so it shows the frame is busy
static void set_led(bool card_active)
{
    DEV_Digital_Write(LED_ACT, card_active || drive_mode || (key_down && long_press_done));
}

// Called by the SD driver on every card access
void sd_activity(bool active)
{
    set_led(active);
}

static float measure_battery_voltage(void)
{
    float voltage = 0.0;
    const float conversion_factor = 3.3f / (1 << 12);
    uint16_t result = adc_read();
    voltage = result * conversion_factor * 3;
    printf("Raw value: 0x%03x, voltage: %f V\n", result, voltage);
    return voltage;
}

static void charge_state_callback()
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

// Reset the RTC to the reference time and set the alarm interval_hours (settings.ini) later
static void arm_alarm(void)
{
    Time_data alarm_time = reference_time;
    alarm_time.hours += settings.interval_hours;
    rtcRunAlarm(reference_time, alarm_time);
}

static void show_next(void)
{
    if (!card_present)
        display_message(msg(MSG_NO_CARD_TITLE), msg(MSG_NO_CARD_HINT));
    else if (!images_select_next())
        display_message(msg(MSG_NO_IMAGES_TITLE), msg(MSG_NO_IMAGES_HINT));
    else
        display_image(images_path, measure_battery_voltage());

    PCF85063_clear_alarm_flag(); // clear RTC alarm flag
    arm_alarm();
}

// Detect the SD card and read settings.ini, so a card inserted or changed while on USB is picked up
static void scan_card(void)
{
    card_present = sdcard_present();
    if (card_present)
        settings_load();
}

// On USB: re-scan the card and show the next image
static void next_image(const char *reason)
{
    printf("%s: next image\r\n", reason);
    scan_card();
    show_next();
    usb_serial_discard(); // commands sent during the refresh would queue more refreshes
}

// Offer the card to the computer, then show the drive-mode screen; the computer is answered during
// the refresh (EPD_7IN3F_BusyWait()), so the drive appears without waiting for it
static void enter_drive_mode(const char *reason)
{
    printf("%s: drive mode\r\n", reason);
    scan_card(); // card check and language for the screen
    if (!usb_drive_computer_connected())
        display_message(msg(MSG_CONNECT_TITLE), msg(MSG_CONNECT_HINT));
    else if (!card_present)
        show_next(); // "no card" screen
    else if (!usb_drive_start())
        show_next();
    else
    {
        drive_mode = true;
        set_led(false);
        display_message(msg(MSG_DRIVE_TITLE), msg(MSG_DRIVE_HINT));
    }
    usb_serial_discard(); // commands sent during the refresh
}

// Take the card back, then re-read it and settings.ini (the computer may have changed both) and
// show the next image
static void leave_drive_mode(const char *reason)
{
    usb_drive_stop();
    drive_mode = false;
    set_led(false);
    next_image(reason);
}

// Runs while the panel refreshes (~30 s): in drive mode the computer keeps being answered
void EPD_7IN3F_BusyWait(UDOUBLE ms)
{
    if (drive_mode)
        usb_drive_serve(ms);
    else
        DEV_Delay_ms(ms);
}

/*
Arduino-Pico entry points. The core's own main() starts USB serial and then calls
setup()/loop(), so the firmware runs from setup() instead of defining main().
*/
void setup(void)
{
    printf("Init...\r\n");
    if (DEV_Module_Init() != 0)
    { // DEV init
        return;
    }

    watchdog_enable(8 * 1000, 1); // 8s
    DEV_Delay_ms(1000);
    PCF85063_init(); // RTC init
    arm_alarm(); // default interval until settings.ini is read, in case anything below fails
    gpio_set_irq_enabled_with_callback(CHARGE_STATE, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, charge_state_callback);

    if (measure_battery_voltage() < 3.1)
    { // battery power is low
        printf("low power ...\r\n");
        PCF85063_alarm_Time_Disable();
        ledLowPower(); // LED flash for Low power
        powerOff(); // BAT off
        return;
    }
    else
    {
        printf("work ...\r\n");
        ledPowerOn();
    }

    scan_card();
    arm_alarm(); // re-arm with interval_hours from settings.ini

    if (!DEV_Digital_Read(VBUS))
    { // on battery: show the next image and cut power until the RTC alarm wakes us up
        show_next();
        printf("power off ...\r\n");
        powerOff(); // BAT off
        return;
    }

    // on USB power: stay awake, loop() handles KEY presses until USB is unplugged
    charge_state_callback();
    charging_mode = true;
}

void loop(void)
{
    if (!charging_mode)
        return;

    if (!DEV_Digital_Read(VBUS))
    { // USB unplugged
        printf("power off ...\r\n");
        powerOff(); // BAT off
        charging_mode = false;
        return;
    }

    // KEY and the RTC alarm both pull BAT_STATE low on this board (both feed the power-on circuit
    // that wakes the device on battery). The alarm holds the line until its flag is cleared, so
    // the flag tells it apart from a long KEY press
    bool line_low = !DEV_Digital_Read(BAT_STATE);
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    // The flag is checked while KEY is held too: an alarm during a press keeps the line low after
    // the release, which would otherwise look like KEY held forever
    if (line_low && PCF85063_get_alarm_flag())
    {
        key_down = false;
        long_press_done = false;
        set_led(false);
        if (drive_mode)
        { // only re-arm, the next image is due after leaving drive mode
            printf("rtc alarm in drive mode\r\n");
            PCF85063_clear_alarm_flag();
            arm_alarm();
        }
        else
            next_image("rtc alarm");
    }
    else if (line_low && !key_down)
    {
        key_down = true;
        key_down_ms = now_ms;
        printf("key down\r\n");
    }
    else if (line_low && !long_press_done && now_ms - key_down_ms >= LONG_PRESS_MS)
    {
        long_press_done = true;
        set_led(false); // lights up: the user can let go
        printf("long key press\r\n");
        if (drive_mode)
            leave_drive_mode("long key press");
        else
            enter_drive_mode("long key press");
    }
    else if (!line_low && key_down)
    {
        printf("key up after %lu ms\r\n", (unsigned long)(now_ms - key_down_ms));
        key_down = false;
        set_led(false);
        if (!long_press_done && !drive_mode)
            next_image("short key press");
        long_press_done = false;
    }

    // Test commands over USB serial, so log-only device tests need no KEY press. Read in drive mode
    // too, so nothing sent meanwhile runs after it
    int command = usb_serial_read();
    if (drive_mode)
    { // commands are ignored, eject or a long press leaves drive mode
        usb_drive_serve(200); // answers the computer instead of the 200 ms delay below
        if (usb_drive_ejected())
            leave_drive_mode("ejected");
        return;
    }
    if (command == 'n')
        next_image("serial command");
    else if (command == 'd')
        enter_drive_mode("serial command");

    DEV_Delay_ms(200);
}
