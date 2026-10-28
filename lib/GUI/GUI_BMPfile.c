/*****************************************************************************
* | File      	:   GUI_BMPfile.h
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*                   Used to shield the underlying layers of each master
*                   and enhance portability
*----------------
* |	This version:   V1.0
* | Date        :   2022-10-11
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
#include "GUI_BMPfile.h"
#include "GUI_Paint.h"
#include "DEV_Config.h"
#include "Debug.h"

#include <stdlib.h>	//exit()
#include <string.h> //memset()
#include <math.h> //memset()

#include "f_util.h"
#include "ff.h"

/******************************************************************************
function:	Read a 24-bit uncompressed BMP and draw it into the selected image
parameter:
    path   :   File path on the SD card
    Xstart :   X offset
    Ystart :   Y offset
return:
    0 on success, 1 on any error (the file is always closed)
******************************************************************************/
UBYTE GUI_ReadBmp_RGB_7Color(const char *path, UWORD Xstart, UWORD Ystart)
{
    BMPFILEHEADER bmpFileHeader;  //Define a bmp file header structure
    BMPINFOHEADER bmpInfoHeader;  //Define a bmp info header structure

    FRESULT fr;
    FIL fil;
    UINT br;
    printf("open %s\r\n", path);
    fr = f_open(&fil, path, FA_READ);
    if (FR_OK != fr && FR_EXIST != fr) {
        printf("f_open(%s) error: %s (%d)\r\n", path, FRESULT_str(fr), fr);
        return 1; // nothing to close
    }

    fr = f_read(&fil, &bmpFileHeader, sizeof(BMPFILEHEADER), &br);   // sizeof(BMPFILEHEADER) must be 14
    if (fr != FR_OK || br != sizeof(BMPFILEHEADER)) {
        printf("f_read bmpFileHeader error\r\n");
        goto error;
    }
    if (bmpFileHeader.bType != 0x4D42) { // "BM"
        printf("not a BMP file\r\n");
        goto error;
    }
    fr = f_read(&fil, &bmpInfoHeader, sizeof(BMPINFOHEADER), &br);   // sizeof(BMPINFOHEADER) must be 40
    if (fr != FR_OK || br != sizeof(BMPINFOHEADER)) {
        printf("f_read bmpInfoHeader error\r\n");
        goto error;
    }

    printf("pixel = %d * %d\r\n", bmpInfoHeader.biWidth, bmpInfoHeader.biHeight);

    if (bmpInfoHeader.biBitCount != 24) {
        printf("BMP is %d-bit, only 24-bit is supported\r\n", bmpInfoHeader.biBitCount);
        goto error;
    }
    if (bmpInfoHeader.biCompression != 0) {
        printf("compressed BMP is not supported\r\n");
        goto error;
    }

    if(bmpInfoHeader.biWidth > bmpInfoHeader.biHeight)
        Paint_SetRotate(0);
    else
        Paint_SetRotate(90);

    // Paint.Width/Height follow the rotation; also rejects top-down BMPs (negative height reads as huge)
    if (bmpInfoHeader.biWidth > Paint.Width || bmpInfoHeader.biHeight > Paint.Height) {
        printf("BMP is larger than the display (%d * %d)\r\n", Paint.Width, Paint.Height);
        goto error;
    }

    // Read image data into the cache
    UWORD x, y;
    UBYTE Rdata[3];

    if (f_lseek(&fil, bmpFileHeader.bOffset) != FR_OK) {
        printf("f_lseek to pixel data error\r\n");
        goto error;
    }

    UBYTE color;
    printf("read data\n");

    for(y = 0; y < bmpInfoHeader.biHeight; y++) {//Total display column
        for(x = 0; x < bmpInfoHeader.biWidth ; x++) {//Show a line in the line
            if(f_read(&fil, (char *)Rdata, 1, &br) != FR_OK || br != 1 ||
               f_read(&fil, (char *)Rdata+1, 1, &br) != FR_OK || br != 1 ||
               f_read(&fil, (char *)Rdata+2, 1, &br) != FR_OK || br != 1) {
                printf("pixel data read error at %d, %d\r\n", x, y);
                goto error;
            }

			if(Rdata[0] == 0 && Rdata[1] == 0 && Rdata[2] == 0){
				// Image[x+(y* bmpInfoHeader.biWidth )] =  0;//Black
                color = 0;
			}else if(Rdata[0] == 255 && Rdata[1] == 255 && Rdata[2] == 255){
				// Image[x+(y* bmpInfoHeader.biWidth )] =  1;//White
                color = 1;
			}else if(Rdata[0] == 0 && Rdata[1] == 255 && Rdata[2] == 0){
				// Image[x+(y* bmpInfoHeader.biWidth )] =  2;//Green
                color = 2;
			}else if(Rdata[0] == 255 && Rdata[1] == 0 && Rdata[2] == 0){
				// Image[x+(y* bmpInfoHeader.biWidth )] =  3;//Blue
                color = 3;
			}else if(Rdata[0] == 0 && Rdata[1] == 0 && Rdata[2] == 255){
				// Image[x+(y* bmpInfoHeader.biWidth )] =  4;//Red
                color = 4;
			}else if(Rdata[0] == 0 && Rdata[1] == 255 && Rdata[2] == 255){
				// Image[x+(y* bmpInfoHeader.biWidth )] =  5;//Yellow
                color = 5;
			}else if(Rdata[0] == 0 && Rdata[1] == 128 && Rdata[2] == 255){
				// Image[x+(y* bmpInfoHeader.biWidth )] =  6;//Orange
                color = 6;
			}
            Paint_SetPixel(Xstart + bmpInfoHeader.biWidth-1-x, Ystart + y, color);
        }
        watchdog_update();
    }
    printf("close file\n");
    f_close(&fil);

    return 0;

error:
    f_close(&fil);
    return 1;
}


