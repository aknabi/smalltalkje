/**
 * m5display.h
 *
 * (C) 2019 - Pablo Bacho <pablo@pablobacho.com>
 * This code is licensed under the MIT License.
 */

#ifndef _M5DISPLAY_H_
#define _M5DISPLAY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_system.h"
#include "esp_log.h"
#include "../util/spi_master_lobo.h"
#include "../util/tftspi.h"
#include "../util/tft.h"
#include "m5power.h"
#include "m5button.h"
#include "m5event.h"

#define M5DISPLAY_TYPE DISP_TYPE_ST7735S  /*!< Display type for display driver */
#define M5DISPLAY_WIDTH 160               /*!< Display width in pixels after rotation */
#define M5DISPLAY_HEIGHT 80               /*!< Display height in pixels after rotation */

// Defines for global variables of the TFT Library
#define TFT_ORIENTATION orientation
#define TFT_FONT_ROTATE font_rotate
#define TFT_TEXT_WRAP text_wrap
#define TFT_FONT_TRANSPARENT font_transparent
#define TFT_FONT_FORCEFIXED font_forceFixed
#define TFT_GRAY_SCALE gray_scale
#define TFT_FONT_BACKGROUND _bg
#define TFT_FONT_FOREGROUND _fg

extern spi_lobo_device_handle_t m5display_spi;    /*!< SPI device handle */

/**
 * @brief   Initialize display
 *
 * @return  ESP_OK success
 *          ESP_FAIL failed
 */
esp_err_t m5display_init();

/**
 * @brief   Set display backlight level
 *
 * @param   backlight_level Backlight level from 0 (lowest) to 7 (brightest)
 *
 * @return  ESP_OK success
 *          ESP_FAIL failed
 */
esp_err_t m5display_set_backlight_level(uint8_t backlight_level);

/**
 * @brief   Turn display off
 *
 * @return  ESP_OK success
 *          ESP_FAIL failed
 */
esp_err_t m5display_off();

/**
 * @brief   Turn display on
 *
 * @return  ESP_OK success
 *          ESP_FAIL failed
 */
esp_err_t m5display_on();

/**
 * @brief   Sets a timeout to turn display off
 *
 *          Display turns back on with button events or m5display_wakeup() function call.
 *
 * @param   timeout timeout in seconds
 * @return  ESP_OK success
 *          ESP_FAIL failed
 */
esp_err_t m5display_timeout(uint32_t timeout);

/**
 * @brief   Turns display on and resets timeout timer
 */
void m5display_wakeup();

/**
 * @brief   Callback for timeout to turn display off
 */
void m5display_sleep();

/**
 * @brief   Event handler for display. Not meant to be used by user program.
 */
void m5display_event_handler(void * handler_arg, esp_event_base_t base, int32_t id, void * event_data);

#ifdef __cplusplus
}
#endif

#endif // _M5DISPLAY_H_