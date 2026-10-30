/*****************************************************************************
* | File      	:   display.c
* | Author      :   Waveshare team
* | Function    :   7.3inch e-Paper (F) Demo
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2023-03-13
* | Info        :
* -----------------------------------------------------------------------------
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include "display.h"
#include "images.h"
#include "messages.h"
#include "sdcard.h"
#include "EPD_7in3f.h"
#include "GUI_Paint.h"
#include "GUI_BMPfile.h"

#include <stdlib.h> // malloc() free()
#include <string.h>

int display_image(const char *path, float vol)
{
    printf("e-Paper Init and Clear...\r\n");
    EPD_7IN3F_Init();

    // Create a new image cache
    UBYTE *image;
    UDOUBLE image_size = ((EPD_7IN3F_WIDTH % 2 == 0) ? (EPD_7IN3F_WIDTH / 2) : (EPD_7IN3F_WIDTH / 2 + 1)) * EPD_7IN3F_HEIGHT;
    if ((image = (UBYTE *)malloc(image_size)) == NULL)
    {
        printf("Failed to apply for black memory...\r\n");
        return -1;
    }
    printf("Paint_NewImage\r\n");
    Paint_NewImage(image, EPD_7IN3F_WIDTH, EPD_7IN3F_HEIGHT, 0, EPD_7IN3F_WHITE);
    Paint_SetScale(7);

    sdcard_mount();

    printf("Display BMP\r\n");
    Paint_SelectImage(image);
    Paint_Clear(EPD_7IN3F_WHITE);

    if (GUI_ReadBmp_RGB_7Color(path, 0, 0) != 0)
    {
        // Show what failed instead of a partial image; the index still advances below
        Paint_SetRotate(180);
        Paint_Clear(EPD_7IN3F_WHITE);
        Paint_DrawString_EN(10, 42, msg(MSG_CANNOT_DISPLAY), &Font16, EPD_7IN3F_BLACK, EPD_7IN3F_WHITE);
        Paint_DrawString_EN(10, 58, path, &Font16, EPD_7IN3F_BLACK, EPD_7IN3F_WHITE);
    }
    else if (Paint_GetRotate() == 90)
        Paint_SetRotate(270);
    else
        Paint_SetRotate(180);
    char voltage_text[21] = {0};
    sprintf(voltage_text, "%f V", vol);
    if (vol < 3.3)
    {
        Paint_DrawString_EN(10, 10, msg(MSG_LOW_VOLTAGE), &Font16, EPD_7IN3F_BLACK, EPD_7IN3F_WHITE);
        Paint_DrawString_EN(10, 26, voltage_text, &Font16, EPD_7IN3F_BLACK, EPD_7IN3F_WHITE);
    }

    printf("EPD_Display\r\n");
    EPD_7IN3F_Display(image);

    sdcard_unmount();
    images_save_shown();

    printf("Goto Sleep...\r\n\r\n");
    EPD_7IN3F_Sleep();
    free(image);
    image = NULL;

    return 0;
}

int display_message(float vol, const char *title, const char *hint)
{
    printf("e-Paper Init and Clear...\r\n");
    EPD_7IN3F_Init();

    // Create a new image cache
    UBYTE *image;
    UDOUBLE image_size = ((EPD_7IN3F_WIDTH % 2 == 0) ? (EPD_7IN3F_WIDTH / 2) : (EPD_7IN3F_WIDTH / 2 + 1)) * EPD_7IN3F_HEIGHT;
    if ((image = (UBYTE *)malloc(image_size)) == NULL)
    {
        printf("Failed to apply for black memory...\r\n");
        return -1;
    }
    printf("Paint_NewImage\r\n");
    Paint_NewImage(image, EPD_7IN3F_WIDTH, EPD_7IN3F_HEIGHT, 0, EPD_7IN3F_WHITE);
    Paint_SetScale(7);

    printf("Display message: %s\r\n", title);
    Paint_SelectImage(image);
    Paint_Clear(EPD_7IN3F_WHITE);

    Paint_SetRotate(180);

    // Display a message
    Paint_DrawString_EN(250, 200, title, &Font20, EPD_7IN3F_WHITE, EPD_7IN3F_BLACK);
    Paint_DrawString_EN(250, 224, hint, &Font16, EPD_7IN3F_WHITE, EPD_7IN3F_BLACK);

    // Display battery voltage
    char voltage_text[32];
    snprintf(voltage_text, sizeof(voltage_text), "%s %.2fV", msg(MSG_BATTERY), vol);
    Paint_DrawString_EN(250, 254, voltage_text, &Font16, EPD_7IN3F_WHITE, EPD_7IN3F_BLACK);

    printf("EPD_Display\r\n");
    EPD_7IN3F_Display(image);

    printf("Goto Sleep...\r\n\r\n");
    EPD_7IN3F_Sleep();
    free(image);
    image = NULL;

    return 0;
}
