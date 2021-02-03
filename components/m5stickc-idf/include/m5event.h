/**
 * m5event.h
 *
 * (C) 2019 - Pablo Bacho <pablo@pablobacho.com>
 * This code is licensed under the MIT License.
 */

#ifndef _M5EVENT_H_
#define _M5EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_log.h"

#ifdef CONFIG_ESP_IDF_VERSION_BEFORE_V3_3
#include "backported/esp_event/esp_event.h"
#else
#include "esp_event.h"
#endif // CONFIG_ESP_IDF_VERSION_BEFORE_V3_3

extern esp_event_loop_handle_t m5_event_loop;   /*!< Event loop for M5 device-specific events */

esp_err_t m5event_init();

#ifdef __cplusplus
}
#endif

#endif // _M5EVENT_H_
