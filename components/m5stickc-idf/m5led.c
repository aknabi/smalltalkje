/**
 * m5led.c
 *
 * (C) 2019 - Timothee Cruse (teuteuguy)
 * This code is licensed under the MIT License.
 */

#include "include/m5led.h"

static const char * TAG = "m5led";

static bool current_state = M5LED_DEFAULT_STATE;

esp_err_t m5led_init()
{
    esp_err_t e;

    gpio_config_t io_conf;
    // Setup the LED
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE; //disable interrupt
    io_conf.mode = GPIO_MODE_OUTPUT; //set as output mode
    io_conf.pin_bit_mask = ((1ULL << M5LED_GPIO)); // bit mask of the pins that you want to set, e.g.GPIO10
    io_conf.pull_down_en = 0; //disable pull-down mode
    io_conf.pull_up_en = 0; //disable pull-up mode
    e = gpio_config(&io_conf); //configure GPIO with the given settings
    if (e != ESP_OK)
    {
        ESP_LOGE(TAG, "Error setting up LED: %u", e);
        return e;
    }

    e = gpio_set_level(M5LED_GPIO, M5LED_DEFAULT_STATE);
    if (e != ESP_OK)
    {
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "LED G10 enabled");
    return ESP_OK;
}

bool m5led_is_on(void)
{
    return current_state;
}

esp_err_t m5led_set(bool state)
{
    esp_err_t e;

    current_state = state;
    e = gpio_set_level(M5LED_GPIO, current_state);
    if (e != ESP_OK)
    {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t m5led_toggle(void)
{
    esp_err_t e;

    e = m5led_set(1 - current_state);
    if (e != ESP_OK)
    {
        return ESP_FAIL;
    }

    return ESP_OK;
}
