/*****************************************************************************
* | File      	:   DEV_Config.h
* | Author      :   Waveshare team
* | Function    :   Board pins and buses (GPIO, SPI, I2C, ADC)
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
#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"
#include "stdio.h"

#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

#define EPD_SPI_PORT 	spi1
#define SD_SPI_PORT 	spi0
#define RTC_I2C_PORT    i2c1
#define RTC_I2C_Address 0x51

// GPIO numbers on the PhotoPainter board ("V2": pins of the newer board revision)
#define EPD_RST_PIN     12
#define EPD_DC_PIN      8
#define EPD_CS_PIN      9
#define EPD_BUSY_PIN    13
#define EPD_CLK_PIN     10  
#define EPD_MOSI_PIN    11

#define RTC_SDA         14
#define RTC_SCL         15
#define RTC_INT         6

#define EPD_POWER_EN    16
#define CHARGE_STATE    17 // V2
#define BAT_OFF         18
#define BAT_STATE       19

#define LED_ACT         25
#define LED_PWR         26 // V2

#define SD_CS_PIN       5
#define SD_CLK_PIN      2
#define SD_MOSI_PIN     3
#define SD_MISO_PIN     4

#define VBUS            24
#define Power_Mode      23
#define VBAT            29

/*------------------------------------------------------------------------------------------------------*/
void DEV_Digital_Write(UWORD Pin, UBYTE Value);
UBYTE DEV_Digital_Read(UWORD Pin);

void DEV_SPI_WriteByte(UBYTE Value);
void DEV_SPI_Write_nByte(UBYTE *pData, UDOUBLE Len);

void I2C_Write_Byte(UBYTE Reg, UBYTE Value);
UBYTE I2C_Read_Byte(UBYTE Reg);

void DEV_Delay_ms(UDOUBLE xms);

UBYTE DEV_Module_Init(void);

#endif
