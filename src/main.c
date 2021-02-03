#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "m5stickc.h"

static const char *TAG = "MAIN";

void my_m5_event_handler(void * handler_arg, esp_event_base_t base, int32_t id, void * event_data) {
    if(base == m5button_a.esp_event_base) {
        switch(id) {
            case M5BUTTON_BUTTON_CLICK_EVENT:
                TFT_resetclipwin();
                TFT_fillScreen(TFT_WHITE);
                TFT_print("Click A!", CENTER, (M5DISPLAY_HEIGHT-24)/2);
                vTaskDelay(1000/portTICK_PERIOD_MS);
                TFT_fillScreen(TFT_WHITE);
            break;
            case M5BUTTON_BUTTON_HOLD_EVENT:
                TFT_resetclipwin();
                TFT_fillScreen(TFT_WHITE);
                TFT_print("Hold A!", CENTER, (M5DISPLAY_HEIGHT-24)/2);
                vTaskDelay(1000/portTICK_PERIOD_MS);
                TFT_fillScreen(TFT_WHITE);
            break;
        }
    }
    if(base == m5button_b.esp_event_base) {
        switch(id) {
            case M5BUTTON_BUTTON_CLICK_EVENT:
                TFT_resetclipwin();
                TFT_fillScreen(TFT_WHITE);
                TFT_print("Click B!", CENTER, (M5DISPLAY_HEIGHT-24)/2);
                vTaskDelay(1000/portTICK_PERIOD_MS);
                TFT_fillScreen(TFT_WHITE);
            break;
            case M5BUTTON_BUTTON_HOLD_EVENT:
                TFT_resetclipwin();
                TFT_fillScreen(TFT_WHITE);
                TFT_print("Hold B!", CENTER, (M5DISPLAY_HEIGHT-24)/2);
                vTaskDelay(1000/portTICK_PERIOD_MS);
                TFT_fillScreen(TFT_WHITE);
            break;
        }
    }
}

void app_main(void)
{
    // Initialize M5StickC
    // This initializes the event loop, power, button and display
    m5stickc_config_t m5config = M5STICKC_CONFIG_DEFAULT();
    m5config.power.lcd_backlight_level = 3; // Set starting backlight level
    m5_init(&m5config);

    // Register for button events
    esp_event_handler_register_with(m5_event_loop, M5BUTTON_A_EVENT_BASE, M5BUTTON_BUTTON_CLICK_EVENT, my_m5_event_handler, NULL);
    esp_event_handler_register_with(m5_event_loop, M5BUTTON_A_EVENT_BASE, M5BUTTON_BUTTON_HOLD_EVENT, my_m5_event_handler, NULL);
    esp_event_handler_register_with(m5_event_loop, M5BUTTON_B_EVENT_BASE, M5BUTTON_BUTTON_CLICK_EVENT, my_m5_event_handler, NULL);
    esp_event_handler_register_with(m5_event_loop, M5BUTTON_B_EVENT_BASE, M5BUTTON_BUTTON_HOLD_EVENT, my_m5_event_handler, NULL);

    font_rotate = 0;
    text_wrap = 0;
    font_transparent = 0;
    font_forceFixed = 0;
    gray_scale = 0;
    TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
    TFT_setRotation(LANDSCAPE);
    TFT_setFont(DEFAULT_FONT, NULL);
    TFT_resetclipwin();
    TFT_fillScreen(TFT_WHITE);
    _bg = TFT_WHITE;
    _fg = TFT_MAGENTA;
    char backlight_str[6];
    vTaskDelay(3000/portTICK_PERIOD_MS);

    ESP_LOGD(TAG, "Turning backlight off");
    m5display_off();
    vTaskDelay(3000/portTICK_PERIOD_MS);
    ESP_LOGD(TAG, "Turning backlight on");
    m5display_on();
    vTaskDelay(1000/portTICK_PERIOD_MS);

    // Backlight level test
    for(uint8_t i=7; i>0; --i) {
        m5display_set_backlight_level(i);
        TFT_fillScreen(TFT_WHITE);
        sprintf(backlight_str, "%d", i);
        TFT_print("Backlight test", CENTER, (M5DISPLAY_HEIGHT-24)/2 +12);
        TFT_print(backlight_str, CENTER, (M5DISPLAY_HEIGHT-24)/2 -12);
        ESP_LOGD(TAG, "Backlight: %d", i);
        vTaskDelay(250/portTICK_PERIOD_MS);
    }
    for(uint8_t i=0; i<=7; ++i) {
        m5display_set_backlight_level(i);
        TFT_fillScreen(TFT_WHITE);
        sprintf(backlight_str, "%d", i);
        TFT_print("Backlight test", CENTER, (M5DISPLAY_HEIGHT-24)/2 +12);
        TFT_print(backlight_str, CENTER, (M5DISPLAY_HEIGHT-24)/2 -12);
        ESP_LOGD(TAG, "Backlight: %d", i);
        vTaskDelay(250/portTICK_PERIOD_MS);
    }

    // Test buttons
    TFT_fillScreen(TFT_WHITE);
    TFT_print("Press or hold button", CENTER, (M5DISPLAY_HEIGHT-24)/2);

    m5display_timeout(15000);
}