/**
 * m5stickc.c - ESP-IDF component to work with M5
 *
 * (C) 2019 - Pablo Bacho <pablo@pablobacho.com>
 * This code is licensed under the MIT License.
 * 
 * Modified for Smalltalkje by Abdul Nabi <abdul@codekrafters.com
 */

#include "m5stickc.h"

static const char *TAG = "m5stickc";

esp_event_loop_handle_t m5_event_loop;

#define I2C_PORT_0_CLK_SPEED 1000000 /*!< The M5StickC display is on this I2C port 0 and can run fast */
#define I2C_PORT_1_CLK_SPEED 100000 /*!< I2C port 1 is GPIO 0/26 and with the CardKB Hat needs to run slow (400K works) */

#define I2C_PORT_1_SDA_GPIO_PIN 0 /*!< Assign SDA I2C port 1 to GPIO 0 (on the M5StickC 8-pin connector) */
#define I2C_PORT_1_SCL_GPIO_PIN 26 /*!< Assign SCL I2C port 1 to GPIO 0 (on the M5StickC 8-pin connector) */

esp_err_t m5_init(m5stickc_config_t * config) {
    esp_err_t e;
    uint8_t error_count = 0;

    m5event_init();

    // Init I²C
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_SDA_GPIO;
    conf.scl_io_num = I2C_SCL_GPIO;
    conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
    conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
    conf.master.clk_speed = I2C_PORT_0_CLK_SPEED;
    ESP_LOGD(TAG, "Setting up I2C");
    e = i2c_param_config(I2C_NUM_0, &conf);
    if(e == ESP_OK) {
        e = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
        if(e == ESP_OK) {
            // Init power management
            e = m5power_init(&config->power);
            if(e == ESP_OK) {
                ESP_LOGD(TAG, "Power manager initialized");
            } else {
                ESP_LOGE(TAG, "Error initializing power manager");
                ++error_count;
            }
        } else {
            ESP_LOGE(TAG, "Error during I2C driver installation: %s", esp_err_to_name(e));
            ++error_count;
        }
    } else {
        ESP_LOGE(TAG, "Error setting up I2C: %s", esp_err_to_name(e));
        ++error_count;
    }

    // Config I2C_NUM_1 begin(sda=-0, int scl=-26), the M5StickC 8-pin connector GPIO PINS
    conf.sda_io_num = I2C_PORT_1_SDA_GPIO_PIN;
    conf.scl_io_num = I2C_PORT_1_SCL_GPIO_PIN;
    conf.master.clk_speed = I2C_PORT_1_CLK_SPEED;
    e = i2c_param_config(I2C_NUM_1, &conf);
    if(e == ESP_OK) {
        e = i2c_driver_install(I2C_NUM_1, I2C_MODE_MASTER, 0, 0, 0);
        if(e != ESP_OK) {
            ESP_LOGE(TAG, "Error during I2C 1 driver install: %s", esp_err_to_name(e));
        }
    } else {
        ESP_LOGE(TAG, "Error during I2C 1 param config installation: %s", esp_err_to_name(e));
    }


    // Init led
    e = m5led_init();
    if(e == ESP_OK) {
        ESP_LOGD(TAG, "Led initialized");
    } else {
        ESP_LOGE(TAG, "Error initializing led");
        ++error_count;
    }

    // Init button
    e = m5button_init();
    if(e == ESP_OK) {
        ESP_LOGD(TAG, "Button initialized");
    } else {
        ESP_LOGE(TAG, "Error initializing button");
        ++error_count;
    }

    // Init display
    e = m5display_init();
    if(e == ESP_OK) {
        ESP_LOGD(TAG, "Display initialized");
    } else {
        ESP_LOGE(TAG, "Error initializing display");
        ++error_count;
    }

    // Init RTC
    // e = m5rtc_init();
    // if(e == ESP_OK) {
    //     ESP_LOGD(TAG, "RTC initialized");
    // } else {
    //     ESP_LOGE(TAG, "Error initializing RTC");
    //     ++error_count;
    // }

    if(error_count == 0) {
        ESP_LOGD(TAG, "M5StickC initialized successfully");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "%d errors found while initializing M5StickC", error_count);
        return ESP_FAIL;
    }
}