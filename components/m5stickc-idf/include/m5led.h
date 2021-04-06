/**
 * m5led.h
 *
 * (C) 2019 - Timothee Cruse (teuteuguy)
 * This code is licensed under the MIT License.
 */

#ifndef _M5LED_H_
#define _M5LED_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"

// TODO: Either move build.h or figure out how to use in in components (though breaks component modularity)
#include "../../../include/build.h"

#define M5LED_ON 0
#define M5LED_OFF 1

#define M5LED_DEFAULT_STATE M5LED_OFF

#if TARGET_DEVICE == DEVICE_M5STICKC

#define M5LED_GPIO          GPIO_NUM_10

#elif TARGET_DEVICE == DEVICE_M5SATOM_LITE

#define M5LED_GPIO          GPIO_NUM_27

#elif TARGET_DEVICE == DEVICE_T_WRISTBAND

#define M5LED_GPIO          GPIO_NUM_4

#endif

/**
 * @brief   Initialize led
 * *
 * @return  ESP_OK success
 *          ESP_FAIL failed
 */
esp_err_t m5led_init();

/**
 * @brief   Check if led is on
 *
 * @return  false not on
 *          true otherwise on
 */
bool m5led_is_on(void);

/**
 * @brief   Set led.
 *
 * @param   state led state to set
 * @return  ESP_OK success
 *          ESP_FAIL failed
 */
esp_err_t m5led_set(bool state);

/**
 * @brief   Toggle led.
 *
 * @return  ESP_OK success
 *          ESP_FAIL failed
 */
esp_err_t m5led_toggle(void);

#ifdef __cplusplus
}
#endif

#endif // _M5BUTTON_H_