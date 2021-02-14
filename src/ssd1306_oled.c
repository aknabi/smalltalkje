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

#include "ssd1306_oled.h"
#include "ssd1306_font.h"

// Following definitions are borrowed from
// http://robotcantalk.blogspot.com/2015/03/interfacing-arduino-with-ssd1306-driven.html

// Control byte
#define OLED_CONTROL_BYTE_CMD_SINGLE 0x80
#define OLED_CONTROL_BYTE_CMD_STREAM 0x00
#define OLED_CONTROL_BYTE_DATA_STREAM 0x40

// Fundamental commands (pg.28)
#define OLED_CMD_SET_CONTRAST 0x81 // follow with 0x7F
#define OLED_CMD_DISPLAY_RAM 0xA4
#define OLED_CMD_DISPLAY_ALLON 0xA5
#define OLED_CMD_DISPLAY_NORMAL 0xA6
#define OLED_CMD_DISPLAY_INVERTED 0xA7
#define OLED_CMD_DISPLAY_OFF 0xAE
#define OLED_CMD_DISPLAY_ON 0xAF

// Addressing Command Table (pg.30)
#define OLED_CMD_SET_MEMORY_ADDR_MODE 0x20 // follow with 0x00 = HORZ mode = Behave like a KS108 graphic LCD
#define OLED_CMD_SET_COLUMN_RANGE 0x21     // can be used only in HORZ/VERT mode - follow with 0x00 and 0x7F = COL127
#define OLED_CMD_SET_PAGE_RANGE 0x22       // can be used only in HORZ/VERT mode - follow with 0x00 and 0x07 = PAGE7

// Hardware Config (pg.31)
#define OLED_CMD_SET_DISPLAY_START_LINE 0x40
#define OLED_CMD_SET_SEGMENT_REMAP 0xA1
#define OLED_CMD_SET_MUX_RATIO 0xA8 // follow with 0x3F = 64 MUX
#define OLED_CMD_SET_COM_SCAN_MODE 0xC8
#define OLED_CMD_SET_DISPLAY_OFFSET 0xD3 // follow with 0x00
#define OLED_CMD_SET_COM_PIN_MAP 0xDA    // follow with 0x12
#define OLED_CMD_NOP 0xE3                // NOP

// Timing and Driving Scheme (pg.32)
#define OLED_CMD_SET_DISPLAY_CLK_DIV 0xD5 // follow with 0x80
#define OLED_CMD_SET_PRECHARGE 0xD9       // follow with 0xF1
#define OLED_CMD_SET_VCOMH_DESELCT 0xDB   // follow with 0x30

// Charge Pump (pg.62)
#define OLED_CMD_SET_CHARGE_PUMP 0x8D // follow with 0x14

#define int1 uint8_t

#if !defined SSD1306_128_32 && !defined SSD1306_96_16
#define SSD1306_128_64
#endif

#if defined SSD1306_128_32 && defined SSD1306_96_16
#error "Only one SSD1306 display can be specified at once"
#endif

#if defined SSD1306_128_64
#define SSD1306_LCDWIDTH 128
#define SSD1306_LCDHEIGHT 64
#endif
#if defined SSD1306_128_32
#define SSD1306_LCDWIDTH 128
#define SSD1306_LCDHEIGHT 32
#endif
#if defined SSD1306_96_16
#define SSD1306_LCDWIDTH 96
#define SSD1306_LCDHEIGHT 16
#endif

#define SSD1306_SETCONTRAST 0x81
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON 0xA5
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY_ 0xA7
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF
#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETCOMPINS 0xDA
#define SSD1306_SETVCOMDETECT 0xDB
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9
#define SSD1306_SETMULTIPLEX 0xA8
#define SSD1306_SETLOWCOLUMN 0x00
#define SSD1306_SETHIGHCOLUMN 0x10
#define SSD1306_SETSTARTLINE 0x40
#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_PAGEADDR 0x22
#define SSD1306_COMSCANINC 0xC0
#define SSD1306_COMSCANDEC 0xC8
#define SSD1306_SEGREMAP 0xA0
#define SSD1306_CHARGEPUMP 0x8D
#define SSD1306_EXTERNALVCC 0x01
#define SSD1306_SWITCHCAPVCC 0x02

// Scrolling #defines
#define SSD1306_ACTIVATE_SCROLL 0x2F
#define SSD1306_DEACTIVATE_SCROLL 0x2E
#define SSD1306_SET_VERTICAL_SCROLL_AREA 0xA3
#define SSD1306_RIGHT_HORIZONTAL_SCROLL 0x26
#define SSD1306_LEFT_HORIZONTAL_SCROLL 0x27
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 0x2A

#define ssd1306_swap(a, b) \
  {                        \
    int16_t t = a;         \
    a = b;                 \
    b = t;                 \
  }

// The SSD1306 datasheet (pg.20) says that a control byte has to be sent before sending a command
// Control byte consists of
// bit 7		: Co   : Continuation bit - If 0, then it assumes all the next bytes are data (no more control bytes).
//				:		 You can send a stream of data, ie: gRAM dump - if Co=0
//				:        For Command, you'd prolly wanna set this - one at a time. Hence, Co=1 for commands
//				:		 For Data stream, Co=0 :)
// bit 6    	: D/C# : Data/Command Selection bit, Data=1/Command=0
// bit [5-0] 	: lower 6 bits have to be 0
#define SSD1306_CONTROL_BYTE_CMD_SINGLE 0x80
#define SSD1306_CONTROL_BYTE_CMD_STREAM 0x00
#define SSD1306_CONTROL_BYTE_DATA_STREAM 0x40

#define tag "SSD1306"

// TODO: If we import the correct header we can remove this (and not have a warning.)
#define bit_test(byte, bit) (byte & (1 << bit))

// uint8_t _i2caddr, _vccstate, x_pos, y_pos, text_size;
uint8_t x_pos, y_pos, text_size;
bool wrap;

uint8_t ssd1306_buffer[SSD1306_LCDHEIGHT * SSD1306_LCDWIDTH / 8];
uint8_t x_pos, y_pos, text_size;

/*
 * Init the I2C Driver as a Master with the pins, clock, etc defined.
 * This needs to be called before and I2C other functions.
 */
void i2c_master_init()
{
  i2c_config_t i2c_config = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = SDA_PIN,
      .scl_io_num = SCL_PIN,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = 1000000};
  i2c_param_config(I2C_NUM_0, &i2c_config);
  i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

bool ssd1306_command(uint8_t command)
{

  esp_err_t espRc;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();

  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
  i2c_master_write_byte(cmd, command, true);
  i2c_master_stop(cmd);
  espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return espRc == ESP_OK;
}

bool ssd1306_command_with_arg(uint8_t command, uint8_t argument)
{

  esp_err_t espRc;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();

  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
  i2c_master_write_byte(cmd, command, true);
  i2c_master_write_byte(cmd, argument, true);
  i2c_master_stop(cmd);
  espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return espRc == ESP_OK;
}

void SSD1306_SetBrightness(uint8_t brightness)
{
  ssd1306_command_with_arg(OLED_CMD_SET_CONTRAST, brightness);
}

// void SSD1306_Begin(uint8_t vccstate = SSD1306_SWITCHCAPVCC)
void SSD1306_Begin(void)
{
  i2c_master_init();

  // Init sequence
  ssd1306_command(SSD1306_DISPLAYOFF);                                   // 0xAE
  ssd1306_command_with_arg(SSD1306_SETDISPLAYCLOCKDIV, 0x80);            // CMD 0xD5, the suggested ratio 0x80
  ssd1306_command_with_arg(SSD1306_SETMULTIPLEX, SSD1306_LCDHEIGHT - 1); // 0xA8
  ssd1306_command_with_arg(SSD1306_SETDISPLAYOFFSET, 0);                 // 0xD3, no offset
  ssd1306_command(SSD1306_SETSTARTLINE | 0x0);                           // line #0

  ssd1306_command_with_arg(SSD1306_MEMORYMODE, 0x0); // 0x20, 0x0 act like ks0108
  ssd1306_command(SSD1306_SEGREMAP | 0x1);
  ssd1306_command(SSD1306_COMSCANDEC);

  ssd1306_command_with_arg(SSD1306_CHARGEPUMP, 0x14); // 0x8D

#if defined SSD1306_128_32
  ssd1306_command_with_arg(SSD1306_SETCOMPINS, 0x02); // 0x8D
  ssd1306_command(SSD1306_SETCONTRAST);               // 0x81
  ssd1306_command(0x8F);

#elif defined SSD1306_128_64
  ssd1306_command_with_arg(SSD1306_SETCOMPINS, 0x12); // 0x8D
  // TODO: We have to have the set contrast here, but if we add an arg it screws up init
  // Also if we take out the contrast command it screws up. Don't know about other display res.
  ssd1306_command(SSD1306_SETCONTRAST); // 0x81
                                        // ssd1306_command(0x9F);

#elif defined SSD1306_96_16
  ssd1306_command_with_arg(SSD1306_SETCOMPINS, 0x02); // 0x8D
  ssd1306_command(SSD1306_SETCONTRAST);               // 0x81
  ssd1306_command(0xAF);

#endif

  ssd1306_command(SSD1306_SETPRECHARGE); // 0xd9

  ssd1306_command_with_arg(SSD1306_SETVCOMDETECT, 0x30); // 0x8D
  ssd1306_command_with_arg(SSD1306_CHARGEPUMP, 0x14);    // 0x8D
  ssd1306_command(SSD1306_DEACTIVATE_SCROLL);
  ssd1306_command(SSD1306_DISPLAYALLON_RESUME); // 0xA4
  ssd1306_command(SSD1306_NORMALDISPLAY);       // 0xA6

  SSD1306_ClearDisplay();
  SSD1306_Display();

  ssd1306_command(SSD1306_DISPLAYON); //--turn on oled panel

  // set cursor to (0, 0)
  x_pos = 0;
  y_pos = 0;
  // set text size to 1
  text_size = 1;
}

void SSD1306_DrawPixel(uint8_t x, uint8_t y, oled_color color)
{
  if ((x >= SSD1306_LCDWIDTH) || (y >= SSD1306_LCDHEIGHT))
    return;
  if (color)
    ssd1306_buffer[x + (uint16_t)(y / 8) * SSD1306_LCDWIDTH] |= (1 << (y & 7));
  else
    ssd1306_buffer[x + (uint16_t)(y / 8) * SSD1306_LCDWIDTH] &= ~(1 << (y & 7));
}

void SSD1306_StartScrollRight(uint8_t start, uint8_t stop)
{
  ssd1306_command(SSD1306_RIGHT_HORIZONTAL_SCROLL);
  ssd1306_command(0X00);
  ssd1306_command(start);
  ssd1306_command(0X00);
  ssd1306_command(stop);
  ssd1306_command(0X00);
  ssd1306_command(0XFF);
  ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}

void SSD1306_StartScrollLeft(uint8_t start, uint8_t stop)
{
  ssd1306_command(SSD1306_LEFT_HORIZONTAL_SCROLL);
  ssd1306_command(0X00);
  ssd1306_command(start);
  ssd1306_command(0X00);
  ssd1306_command(stop);
  ssd1306_command(0X00);
  ssd1306_command(0XFF);
  ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}

void SSD1306_StartScrollDiagRight(uint8_t start, uint8_t stop)
{
  ssd1306_command(SSD1306_SET_VERTICAL_SCROLL_AREA);
  ssd1306_command(0X00);
  ssd1306_command(SSD1306_LCDHEIGHT);
  ssd1306_command(SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL);
  ssd1306_command(0X00);
  ssd1306_command(start);
  ssd1306_command(0X00);
  ssd1306_command(stop);
  ssd1306_command(0X01);
  ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}

void SSD1306_StartScrollDiagLeft(uint8_t start, uint8_t stop)
{
  ssd1306_command(SSD1306_SET_VERTICAL_SCROLL_AREA);
  ssd1306_command(0X00);
  ssd1306_command(SSD1306_LCDHEIGHT);
  ssd1306_command(SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL);
  ssd1306_command(0X00);
  ssd1306_command(start);
  ssd1306_command(0X00);
  ssd1306_command(stop);
  ssd1306_command(0X01);
  ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}

void SSD1306_StartScrollVertical(bool isDown)
{
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);

  i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);

  i2c_master_write_byte(cmd, 0x29, true); // vertical and horizontal scroll (p29)
  i2c_master_write_byte(cmd, 0x00, true);
  i2c_master_write_byte(cmd, 0x00, true);
  i2c_master_write_byte(cmd, 0x07, true);
  i2c_master_write_byte(cmd, 0x01, true);
  i2c_master_write_byte(cmd, 0x3F, true);

  i2c_master_write_byte(cmd, 0xA3, true); // set vertical scroll area (p30)
  i2c_master_write_byte(cmd, 0x20, true);
  i2c_master_write_byte(cmd, 0x40, true);

  i2c_master_write_byte(cmd, 0x2F, true); // activate scroll (p29)

  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
}

void SSD1306_StopScroll(void)
{
  ssd1306_command(SSD1306_DEACTIVATE_SCROLL);
}

void SSD1306_Display(void)
{
  ssd1306_command(SSD1306_COLUMNADDR);
  ssd1306_command(0);                    // Column start address (0 = reset)
  ssd1306_command(SSD1306_LCDWIDTH - 1); // Column end address (127 = reset)

  ssd1306_command(SSD1306_PAGEADDR);
  ssd1306_command(0); // Page start address (0 = reset)
#if SSD1306_LCDHEIGHT == 64
  ssd1306_command(7); // Page end address
#endif
#if SSD1306_LCDHEIGHT == 32
  ssd1306_command(3); // Page end address
#endif
#if SSD1306_LCDHEIGHT == 16
  ssd1306_command(1); // Page end address
#endif

  i2c_cmd_handle_t cmd;
  for (uint16_t k = 0; k < (SSD1306_LCDWIDTH * SSD1306_LCDHEIGHT / 8); k++)
  {
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

    i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);

    for (uint8_t x = 0; x < 16; ++x)
    {
      i2c_master_write_byte(cmd, ssd1306_buffer[k], true);
      k++;
    }
    k--;

    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
  }
}

void SSD1306_ClearDisplay(void)
{
  for (uint16_t i = 0; i < (SSD1306_LCDWIDTH * SSD1306_LCDHEIGHT / 8); i++)
    ssd1306_buffer[i] = 0;
}

void SSD1306_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, oled_color color)
{
  int1 steep;
  int8_t ystep;
  uint8_t dx, dy;
  int16_t err;
  steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep)
  {
    ssd1306_swap(x0, y0);
    ssd1306_swap(x1, y1);
  }
  if (x0 > x1)
  {
    ssd1306_swap(x0, x1);
    ssd1306_swap(y0, y1);
  }
  dx = x1 - x0;
  dy = abs(y1 - y0);

  err = dx / 2;
  if (y0 < y1)
    ystep = 1;
  else
    ystep = -1;

  for (; x0 <= x1; x0++)
  {
    if (steep)
    {
      SSD1306_DrawPixel(y0, x0, color);
      // if(color) SSD1306_DrawPixel(y0, x0, true);
      // else      SSD1306_DrawPixel(y0, x0, false);
    }
    else
    {
      SSD1306_DrawPixel(x0, y0, color);
      // if(color) SSD1306_DrawPixel(x0, y0, true);
      // else      SSD1306_DrawPixel(x0, y0, false);
    }
    err -= dy;
    if (err < 0)
    {
      y0 += ystep;
      err += dx;
    }
  }
}

void SSD1306_DrawFastHLine(uint8_t x, uint8_t y, uint8_t w, oled_color color)
{
  SSD1306_DrawLine(x, y, x + w - 1, y, color);
}

void SSD1306_DrawFastVLine(uint8_t x, uint8_t y, uint8_t h, oled_color color)
{
  SSD1306_DrawLine(x, y, x, y + h - 1, color);
}

void SSD1306_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, oled_color color)
{
  for (int16_t i = x; i < x + w; i++)
    SSD1306_DrawFastVLine(i, y, h, color);
}

void SSD1306_FillScreen(oled_color color)
{
  SSD1306_FillRect(0, 0, SSD1306_LCDWIDTH, SSD1306_LCDHEIGHT, color);
}

void SSD1306_DrawCircle(int16_t x0, int16_t y0, int16_t r)
{
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  SSD1306_DrawPixel(x0, y0 + r, true);
  SSD1306_DrawPixel(x0, y0 - r, true);
  SSD1306_DrawPixel(x0 + r, y0, true);
  SSD1306_DrawPixel(x0 - r, y0, true);

  while (x < y)
  {
    if (f >= 0)
    {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    SSD1306_DrawPixel(x0 + x, y0 + y, true);
    SSD1306_DrawPixel(x0 - x, y0 + y, true);
    SSD1306_DrawPixel(x0 + x, y0 - y, true);
    SSD1306_DrawPixel(x0 - x, y0 - y, true);
    SSD1306_DrawPixel(x0 + y, y0 + x, true);
    SSD1306_DrawPixel(x0 - y, y0 + x, true);
    SSD1306_DrawPixel(x0 + y, y0 - x, true);
    SSD1306_DrawPixel(x0 - y, y0 - x, true);
  }
}

void SSD1306_DrawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername)
{
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  while (x < y)
  {
    if (f >= 0)
    {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
    if (cornername & 0x4)
    {
      SSD1306_DrawPixel(x0 + x, y0 + y, true);
      SSD1306_DrawPixel(x0 + y, y0 + x, true);
    }
    if (cornername & 0x2)
    {
      SSD1306_DrawPixel(x0 + x, y0 - y, true);
      SSD1306_DrawPixel(x0 + y, y0 - x, true);
    }
    if (cornername & 0x8)
    {
      SSD1306_DrawPixel(x0 - y, y0 + x, true);
      SSD1306_DrawPixel(x0 - x, y0 + y, true);
    }
    if (cornername & 0x1)
    {
      SSD1306_DrawPixel(x0 - y, y0 - x, true);
      SSD1306_DrawPixel(x0 - x, y0 - y, true);
    }
  }
}

void SSD1306_FillCircle(int16_t x0, int16_t y0, int16_t r, oled_color color)
{
  SSD1306_DrawFastVLine(x0, y0 - r, 2 * r + 1, color);
  SSD1306_FillCircleHelper(x0, y0, r, 3, 0, color);
}

// Used to do circles and roundrects
void SSD1306_FillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, bool color)
{
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  while (x < y)
  {
    if (f >= 0)
    {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    if (cornername & 0x01)
    {
      SSD1306_DrawFastVLine(x0 + x, y0 - y, 2 * y + 1 + delta, color);
      SSD1306_DrawFastVLine(x0 + y, y0 - x, 2 * x + 1 + delta, color);
    }
    if (cornername & 0x02)
    {
      SSD1306_DrawFastVLine(x0 - x, y0 - y, 2 * y + 1 + delta, color);
      SSD1306_DrawFastVLine(x0 - y, y0 - x, 2 * x + 1 + delta, color);
    }
  }
}

// Draw a rectangle
void SSD1306_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
  SSD1306_DrawFastHLine(x, y, w, true);
  SSD1306_DrawFastHLine(x, y + h - 1, w, true);
  SSD1306_DrawFastVLine(x, y, h, true);
  SSD1306_DrawFastVLine(x + w - 1, y, h, true);
}

// Draw a rounded rectangle
void SSD1306_DrawRoundRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r)
{
  // smarter version
  SSD1306_DrawFastHLine(x + r, y, w - 2 * r, true);         // Top
  SSD1306_DrawFastHLine(x + r, y + h - 1, w - 2 * r, true); // Bottom
  SSD1306_DrawFastVLine(x, y + r, h - 2 * r, true);         // Left
  SSD1306_DrawFastVLine(x + w - 1, y + r, h - 2 * r, true); // Right
  // draw four corners
  SSD1306_DrawCircleHelper(x + r, y + r, r, 1);
  SSD1306_DrawCircleHelper(x + w - r - 1, y + r, r, 2);
  SSD1306_DrawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4);
  SSD1306_DrawCircleHelper(x + r, y + h - r - 1, r, 8);
}

// Fill a rounded rectangle
void SSD1306_FillRoundRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r, oled_color color)
{
  // smarter version
  SSD1306_FillRect(x + r, y, w - 2 * r, h, color);
  // draw four corners
  SSD1306_FillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color);
  SSD1306_FillCircleHelper(x + r, y + r, r, 2, h - 2 * r - 1, color);
}

// Draw a triangle
void SSD1306_DrawTriangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
  SSD1306_DrawLine(x0, y0, x1, y1, true);
  SSD1306_DrawLine(x1, y1, x2, y2, true);
  SSD1306_DrawLine(x2, y2, x0, y0, true);
}

// Fill a triangle
void SSD1306_FillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, oled_color color)
{
  int16_t a, b, y, last;
  // Sort coordinates by Y order (y2 >= y1 >= y0)
  if (y0 > y1)
  {
    ssd1306_swap(y0, y1);
    ssd1306_swap(x0, x1);
  }
  if (y1 > y2)
  {
    ssd1306_swap(y2, y1);
    ssd1306_swap(x2, x1);
  }
  if (y0 > y1)
  {
    ssd1306_swap(y0, y1);
    ssd1306_swap(x0, x1);
  }

  if (y0 == y2)
  { // Handle awkward all-on-same-line case as its own thing
    a = b = x0;
    if (x1 < a)
      a = x1;
    else if (x1 > b)
      b = x1;
    if (x2 < a)
      a = x2;
    else if (x2 > b)
      b = x2;
    SSD1306_DrawFastHLine(a, y0, b - a + 1, color);
    return;
  }

  int16_t
      dx01 = x1 - x0,
      dy01 = y1 - y0,
      dx02 = x2 - x0,
      dy02 = y2 - y0,
      dx12 = x2 - x1,
      dy12 = y2 - y1;
  int32_t sa = 0, sb = 0;

  // For upper part of triangle, find scanline crossings for segments
  // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
  // is included here (and second loop will be skipped, avoiding a /0
  // error there), otherwise scanline y1 is skipped here and handled
  // in the second loop...which also avoids a /0 error here if y0=y1
  // (flat-topped triangle).
  if (y1 == y2)
    last = y1; // Include y1 scanline
  else
    last = y1 - 1; // Skip it

  for (y = y0; y <= last; y++)
  {
    a = x0 + sa / dy01;
    b = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;
    /* longhand:
    a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if (a > b)
      ssd1306_swap(a, b);
    SSD1306_DrawFastHLine(a, y, b - a + 1, color);
  }

  // For lower part of triangle, find scanline crossings for segments
  // 0-2 and 1-2.  This loop is skipped if y1=y2.
  sa = dx12 * (y - y1);
  sb = dx02 * (y - y0);
  for (; y <= y2; y++)
  {
    a = x1 + sa / dy12;
    b = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;
    /* longhand:
    a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if (a > b)
      ssd1306_swap(a, b);
    SSD1306_DrawFastHLine(a, y, b - a + 1, color);
  }
}

// invert the display
void SSD1306_InvertDisplay(bool i)
{
  if (i)
    ssd1306_command(SSD1306_INVERTDISPLAY_);
  else
    ssd1306_command(SSD1306_NORMALDISPLAY);
}

void SSD1306_SetTextWrap(bool w)
{
  wrap = w;
}

void SSD1306_DrawChar(uint8_t x, uint8_t y, uint8_t c, uint8_t size)
{
  SSD1306_GotoXY(x, y);
  SSD1306_TextSize(size);
  SSD1306_Print(c);
}

void SSD1306_DrawText(uint8_t x, uint8_t y, char *_text, uint8_t size)
{
  SSD1306_GotoXY(x, y);
  SSD1306_TextSize(size);
  while (*_text != '\0')
    SSD1306_Print(*_text++);
}

// move cursor to position (x, y)
void SSD1306_GotoXY(uint8_t x, uint8_t y)
{
  if ((x >= SSD1306_LCDWIDTH) || (y >= SSD1306_LCDHEIGHT))
    return;
  x_pos = x;
  y_pos = y;
}

// set text size
void SSD1306_TextSize(uint8_t t_size)
{
  if (t_size < 1)
    t_size = 1;
  text_size = t_size;
}

/* print single char
    \a  Set cursor position to upper left (0, 0)
    \b  Move back one position
    \n  Go to start of current line
    \r  Go to line below
*/
void SSD1306_Print(uint8_t c)
{
  oled_color _color;
  uint8_t i, j, line;

  if (c == ' ' && x_pos == 0 && wrap)
    return;
  if (c == '\a')
  {
    x_pos = y_pos = 0;
    return;
  }
  if ((c == '\b') && (x_pos >= text_size * 6))
  {
    x_pos -= text_size * 6;
    return;
  }
  if (c == '\r')
  {
    x_pos = 0;
    return;
  }
  if (c == '\n')
  {
    y_pos += text_size * 8;
    if ((y_pos + text_size * 7) > SSD1306_LCDHEIGHT)
      y_pos = 0;
    return;
  }

  if ((c < ' ') || (c > '~'))
    c = '?';

  for (i = 0; i < 5; i++)
  {
    line = Font[(c - ' ') * 5 + i];
    for (j = 0; j < 7; j++, line >>= 1)
    {
      if (line & 0x01)
        _color = oled_color_white;
      else
        _color = oled_color_black;
      if (text_size == 1)
        SSD1306_DrawPixel(x_pos + i, y_pos + j, _color);
      else
        SSD1306_FillRect(x_pos + (i * text_size), y_pos + (j * text_size), text_size, text_size, _color);
    }
  }

  SSD1306_FillRect(x_pos + (5 * text_size), y_pos, text_size, 7 * text_size, false);

  x_pos += text_size * 6;

  if (x_pos > (SSD1306_LCDWIDTH + text_size * 6))
    x_pos = SSD1306_LCDWIDTH;

  if (wrap && (x_pos + (text_size * 5)) > SSD1306_LCDWIDTH)
  {
    x_pos = 0;
    y_pos += text_size * 8;
    if ((y_pos + text_size * 7) > SSD1306_LCDHEIGHT)
      y_pos = 0;
  }
}

// print custom char (dimension: 7x5 pixel)
void SSD1306_PutCustomC(uint8_t *c)
{
  oled_color _color;
  uint8_t i, j, line;

  for (i = 0; i < 5; i++)
  {
    line = c[i];

    for (j = 0; j < 7; j++, line >>= 1)
    {
      if (line & 0x01)
        _color = oled_color_white;
      else
        _color = oled_color_black;
      if (text_size == 1)
        SSD1306_DrawPixel(x_pos + i, y_pos + j, _color);
      else
        SSD1306_FillRect(x_pos + (i * text_size), y_pos + (j * text_size), text_size, text_size, _color);
    }
  }

  SSD1306_FillRect(x_pos + (5 * text_size), y_pos, text_size, 7 * text_size, false);

  x_pos += (text_size * 6);

  if (x_pos > (SSD1306_LCDWIDTH + text_size * 6))
    x_pos = SSD1306_LCDWIDTH;

  if (wrap && (x_pos + (text_size * 5)) > SSD1306_LCDWIDTH)
  {
    x_pos = 0;
    y_pos += text_size * 8;
    if ((y_pos + text_size * 7) > SSD1306_LCDHEIGHT)
      y_pos = 0;
  }
}

// draw bitmap
void SSD1306_DrawBMP(uint8_t x, uint8_t y, uint8_t *bitmap, uint8_t w, uint8_t h)
{
  for (uint16_t i = 0; i < h / 8; i++)
  {
    for (uint16_t j = 0; j < (uint16_t)w * 8; j++)
    {
      if (bit_test(bitmap[j / 8 + i * w], j % 8) == 1)
        SSD1306_DrawPixel(x + j / 8, y + i * 8 + (j % 8), oled_color_white);
      else
        SSD1306_DrawPixel(x + j / 8, y + i * 8 + (j % 8), oled_color_black);
    }
  }
}
