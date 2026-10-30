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

#if 1
    sdcard_mount();

    printf("Display BMP\r\n");
    Paint_SelectImage(image);
    Paint_Clear(EPD_7IN3F_WHITE);

    if (GUI_ReadBmp_RGB_7Color(path, 0, 0) != 0)
    {
        // Show what failed instead of a partial image; the index still advances below
        Paint_SetRotate(180);
        Paint_Clear(EPD_7IN3F_WHITE);
        Paint_DrawString_EN(10, 42, "Cannot display image:", &Font16, EPD_7IN3F_BLACK, EPD_7IN3F_WHITE);
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
        Paint_DrawString_EN(10, 10, "Low voltage, please charge in time.", &Font16, EPD_7IN3F_BLACK, EPD_7IN3F_WHITE);
        Paint_DrawString_EN(10, 26, voltage_text, &Font16, EPD_7IN3F_BLACK, EPD_7IN3F_WHITE);
    }

    printf("EPD_Display\r\n");
    EPD_7IN3F_Display(image);

    sdcard_unmount();
#endif
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
    char voltage_text[21] = {0};
    sprintf(voltage_text, "Battery: %.2fV", vol);
    Paint_DrawString_EN(250, 254, voltage_text, &Font16, EPD_7IN3F_WHITE, EPD_7IN3F_BLACK);

    printf("EPD_Display\r\n");
    EPD_7IN3F_Display(image);

    printf("Goto Sleep...\r\n\r\n");
    EPD_7IN3F_Sleep();
    free(image);
    image = NULL;

    return 0;
}

int display_test_pattern(void)
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

#if 1 // Drawing on the image
    // 1.Select Image
    printf("SelectImage:image\r\n");
    Paint_SelectImage(image);
    Paint_Clear(EPD_7IN3F_WHITE);

    int h_number, h_width, v_number, v_width;
    h_number = 20;
    h_width = EPD_7IN3F_HEIGHT / h_number; // 800/20
    v_number = 10;
    v_width = EPD_7IN3F_WIDTH / v_number; // 480/10

    // 2.Drawing on the image
    printf("Drawing:image\r\n");
    for (int i = 0; i < v_number; i++)
    {
        Paint_DrawRectangle(1, 1 + i * v_width, 800, v_width * (i + 1), EPD_7IN3F_GREEN + (i % 5), DOT_PIXEL_1X1, DRAW_FILL_FULL);
    }
    for (int i = 0, j = 0; i < h_number; i++)
    {
        if (i % 2)
        {
            j++;
            Paint_DrawRectangle(1 + i * h_width, 1, h_width * (1 + i), 480, j % 2 ? EPD_7IN3F_BLACK : EPD_7IN3F_WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        }
    }

    printf("EPD_Display\r\n");
    EPD_7IN3F_Display(image);
#endif

    printf("Goto Sleep...\r\n");
    EPD_7IN3F_Sleep();
    free(image);
    image = NULL;

    return 0;
}
