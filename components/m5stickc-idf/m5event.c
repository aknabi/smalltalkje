/**
 * m5event.c
 *
 * (C) 2019 - Pablo Bacho <pablo@pablobacho.com>
 * This code is licensed under the MIT License.
 */

#include "m5event.h"

static const char *TAG = "m5event";

esp_event_loop_handle_t m5_event_loop;

esp_err_t m5event_init()
{
    esp_event_loop_args_t loop_args = {
        .queue_size = 5,
        .task_name = "m5_event_loop",
        .task_priority = 10,
        .task_stack_size = 2048,
        .task_core_id = 0
    };

    esp_err_t e = esp_event_loop_create(&loop_args, &m5_event_loop);
    if(e == ESP_OK) {
        ESP_LOGD(TAG, "Event loop created");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Error creating event loop: %s", esp_err_to_name(e));
        return ESP_FAIL;
    }
}
