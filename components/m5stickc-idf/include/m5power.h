/**
 * m5power.h
 *
 * (C) 2019 - Pablo Bacho <pablo@pablobacho.com>
 * This code is licensed under the MIT License.
 */

#ifndef _M5POWER_H_
#define _M5POWER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"

#include "../util/axp192_reg.h"

#define AXP192_I2C_ADDR 0x34
#define I2C_SDA_GPIO ((gpio_num_t) 21)    /*!< SDA pin of I²C bus */
#define I2C_SCL_GPIO ((gpio_num_t) 22)    /*!< SCL pin of I²C bus */

typedef struct {
    bool enable_lcd_backlight;
    uint8_t lcd_backlight_level;
} m5power_config_t;

#define M5POWER_CONFIG_DEFAULT() { \
    .enable_lcd_backlight = true, \
    .lcd_backlight_level = 7 \
    }

/**
 * @brief   Initialize power configuring AXP192 IC
 *
 * @return  ESP_OK success
 *          ESP_FAIL failed
 */
esp_err_t m5power_init(m5power_config_t * config);

/**
 * @brief   Read content of register
 *
 * @param   register_address
 * @param   register_content The content of the register gets stored in this variable
 *
 * @return  ESP_OK success
 *          ESP_FAIL failed
 *          ESP_ERR_NO_MEM out of memory
 */
esp_err_t m5power_register_read(uint8_t register_address, uint8_t * register_content);

/**
 * @brief   Write content of register. Will overwrite any previous content.
 *
 * @param   register_address
 * @param   register_content New register content
 *
 * @return  ESP_OK success
 *          ESP_FAIL failed
 *          ESP_ERR_NO_MEM out of memory
 */
esp_err_t m5power_register_write(uint8_t register_address, uint8_t register_content);

/**
 * @brief   Set to 1 a selection of bits in a register. Will not change other bits.
 *
 * @param   register_address
 * @param   bits_to_set bits to be set
 *
 * @return  ESP_OK success
 *          ESP_FAIL failed
 *          ESP_ERR_NO_MEM out of memory
 */
esp_err_t m5power_register_set_bits(uint8_t register_address, uint8_t bits_to_set);

/**
 * @brief   Clear a selection of bits in a register. Will not change other bits.
 *
 * @param   register_address
 * @param   bits_to_clear bits to clear (pass them as 1s)
 *
 * @return  ESP_OK success
 *          ESP_FAIL failed
 *          ESP_ERR_NO_MEM out of memory
 */
esp_err_t m5power_register_clear_bits(uint8_t register_address, uint8_t bits_to_clear);

/**
 * @brief   Get VBat in V.
 *
 * @param   vbat The content of the vbat read gets stored in this variable
 *
 * @return  ESP_OK success
 *          ESP_FAIL failed
 */
esp_err_t m5power_get_vbat(uint16_t *vbat);

/**
 * @brief   Get VAps in V.
 *
 * @param   vaps The content of the vaps read gets stored in this variable
 *
 * @return  ESP_OK success
 *          ESP_FAIL failed
 */
esp_err_t m5power_get_vaps(uint16_t *vbat);

/**
 * @brief   Set Sleep.
 *
 * @return  ESP_OK success
 *          ESP_FAIL failed
 */
esp_err_t m5power_set_sleep(void);

#ifdef __cplusplus
}
#endif

#endif // _M5POWER_H_