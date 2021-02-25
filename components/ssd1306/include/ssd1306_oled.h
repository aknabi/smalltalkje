/******************************************************************************
 SSD1306 OLED driver for CCS PIC C compiler (SSD1306OLED.c)                   *
 Reference: Adafruit Industries SSD1306 OLED driver and graphics library.     *
                                                                              *
 The driver is for I2C mode only.                                             *
                                                                              *
 https://simple-circuit.com/                                                   *
                                                                              *
*******************************************************************************
*******************************************************************************
 This is a library for our Monochrome OLEDs based on SSD1306 drivers          *
                                                                              *
  Pick one up today in the adafruit shop!                                     *
  ------> http://www.adafruit.com/category/63_98                              *
                                                                              *
 Adafruit invests time and resources providing this open source code,         *
 please support Adafruit and open-source hardware by purchasing               *
 products from Adafruit!                                                      *
                                                                              *
 Written by Limor Fried/Ladyada  for Adafruit Industries.                     *
 BSD license, check license.txt for more information                          *
 All text above, and the splash screen must be included in any redistribution *
*******************************************************************************/

#include "driver/i2c.h"
#include <stdint.h>

//------------------------------ Definitions to Mod for Your Setup---------------------------------//

/* ESP32 GPIO PINS THAT DISPLAY IS CONNECTED TO (used when init of I2C Master is done) */
#define SDA_PIN GPIO_NUM_21
#define SCL_PIN GPIO_NUM_22

// SLA (0x3C) + WRITE_MODE (0x00) =  0x78 (0b01111000)
#define OLED_I2C_ADDRESS 0x3C

#define SSD1306_128_64

#define oled_color_white true
#define oled_color_black false

typedef bool oled_color;

//------------------------------ OLED Library Functions ---------------------------------//

// void SSD1306_Begin(uint8_t vccstate, uint8_t i2caddr);
void SSD1306_Begin(void);
void SSD1306_DrawPixel(uint8_t x, uint8_t y, oled_color color);
void SSD1306_StartScrollRight(uint8_t start, uint8_t stop);
void SSD1306_StartScrollLeft(uint8_t start, uint8_t stop);
void SSD1306_StartScrollDiagRight(uint8_t start, uint8_t stop);
void SSD1306_StartScrollDiagLeft(uint8_t start, uint8_t stop);
void SSD1306_StartScrollVertical(bool isDown);
void SSD1306_StopScroll(void);
void SSD1306_SetBrightness(uint8_t brightness);
void SSD1306_Display(void);
void SSD1306_ClearDisplay(void);
void SSD1306_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, oled_color color);
void SSD1306_DrawFastHLine(uint8_t x, uint8_t y, uint8_t w, oled_color color);
void SSD1306_DrawFastVLine(uint8_t x, uint8_t y, uint8_t h, oled_color color);
void SSD1306_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, oled_color color);
void SSD1306_FillScreen(bool color);
void SSD1306_DrawCircle(int16_t x0, int16_t y0, int16_t r);
void SSD1306_DrawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername);
void SSD1306_FillCircle(int16_t x0, int16_t y0, int16_t r, oled_color color);
void SSD1306_FillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, oled_color color);
void SSD1306_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void SSD1306_DrawRoundRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r);
void SSD1306_FillRoundRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r, bool color);
void SSD1306_DrawTriangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
void SSD1306_FillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, oled_color color);

/*
‘\a’: set cursor position to upper left (0, 0)
‘\b’: move back one position
‘\n’: go to start of current line
‘\r’: go to line below.
*/
void SSD1306_DrawChar(uint8_t x, uint8_t y, uint8_t c, uint8_t size);
void SSD1306_DrawText(uint8_t x, uint8_t y, char *_text, uint8_t size);

/*
 * Text size is height (size * 8). So 1 is 8px high characters, 8 is 64px high
 */
void SSD1306_TextSize(uint8_t t_size);

void SSD1306_GotoXY(uint8_t x, uint8_t y);
void SSD1306_Print(uint8_t c);
void SSD1306_PutCustomC(uint8_t *c);
void SSD1306_SetTextWrap(bool w);
void SSD1306_InvertDisplay(bool i);
void SSD1306_DrawBMP(uint8_t x, uint8_t y, uint8_t *bitmap, uint8_t w, uint8_t h);
