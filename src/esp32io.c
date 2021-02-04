// Target defines (e.g. mac, esp32)
#include "target.h"

#ifdef TARGET_ESP32

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "esp_vfs_dev.h"
#include "esp_spi_flash.h"
#include "esp_partition.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "m5stickc.h"

#endif // TARGET_ESP32

#ifdef TARGET_ESP32

static const char *ESP_TAG = "ESP32";

#define TEST_M5STICK

#ifdef TEST_M5STICK

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

void m5StickTask(void* arg)
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

    // ESP_LOGD(TAG, "Turning backlight off");
    m5display_off();
    vTaskDelay(3000/portTICK_PERIOD_MS);
    // ESP_LOGD(TAG, "Turning backlight on");
    m5display_on();
    vTaskDelay(1000/portTICK_PERIOD_MS);

    // Backlight level test
    for(uint8_t i=7; i>0; --i) {
        m5display_set_backlight_level(i);
        TFT_fillScreen(TFT_WHITE);
        sprintf(backlight_str, "%d", i);
        TFT_print("Backlight test", CENTER, (M5DISPLAY_HEIGHT-24)/2 +12);
        TFT_print(backlight_str, CENTER, (M5DISPLAY_HEIGHT-24)/2 -12);
        // ESP_LOGE(TAG, "Backlight: %d", i);
        vTaskDelay(250/portTICK_PERIOD_MS);
    }
    for(uint8_t i=0; i<=7; ++i) {
        m5display_set_backlight_level(i);
        TFT_fillScreen(TFT_WHITE);
        sprintf(backlight_str, "%d", i);
        TFT_print("Backlight test", CENTER, (M5DISPLAY_HEIGHT-24)/2 +12);
        TFT_print(backlight_str, CENTER, (M5DISPLAY_HEIGHT-24)/2 -12);
        // ESP_LOGE(TAG, "Backlight: %d", i);
        vTaskDelay(250/portTICK_PERIOD_MS);
    }

    // Test buttons
    TFT_fillScreen(TFT_WHITE);
    // ESP_LOGE(TAG, "Button test start");
    TFT_print("Press or hold button", CENTER, (M5DISPLAY_HEIGHT-24)/2);

    m5display_timeout(15000);

    while(true) {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }

   vTaskDelete( NULL );
}

#endif // TEST_M5STICK

void app_main(void)
{
     /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK( uart_driver_install(0,
            256, 0, 0, NULL, 0) );


    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(0);

    /* Disable buffering on stdin and stdout */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);


    ESP_LOGI(ESP_TAG, "Starting M5StickC Test\n");
    xTaskCreate(
        m5StickTask, /* Task function. */
        "m5StickC", /* name of task. */
        4096, /* Stack size of task */
        NULL, /* parameter of the task */
        1, /* priority of the task */
        NULL); /* Task handle to keep track of created task */

    ESP_LOGI(ESP_TAG, "Fresh free heap size: %d", esp_get_free_heap_size());

    startup();

    while(true) {
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }

}

void initFileSystem()
{
    initSPIFFSPartition("storage", "/spiffs");
    // initSPIFFSPartition("objects", "/objects");
#ifndef WRITE_OBJECT_PARTITION 
    setupObjectData();
#endif
}

void initSPIFFSPartition(char *partitionName, char *basePath)
{
    ESP_LOGI(ESP_TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = basePath,
      .partition_label = partitionName,
      .max_files = 5,
      .format_if_mount_failed = false
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(ESP_TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(ESP_TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(ESP_TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(ESP_TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(ESP_TAG, "Partition size: total: %d, used: %d", total, used);
    }
}

spi_flash_mmap_handle_t objectDataHandle;
extern void* objectData;

void setupObjectData()
{
    const esp_partition_t* part;
    esp_err_t err;

    part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "objects");
    esp_partition_find(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY, "objects");
    err = esp_partition_mmap(part, 0, part->size, SPI_FLASH_MMAP_DATA, (const void**)&objectData, &objectDataHandle);
  
    if (err != ESP_OK) {
        ESP_LOGE(ESP_TAG, "esp_partition_mmap failed. (%d)\n", err);
        //delay(10000);
        // abort();
    } else {
        ESP_LOGE(ESP_TAG, "Mapped Objects Partition to address %d", objectData);
    }

}

#ifdef WRITE_OBJECT_PARTITION

void writeObjectDataPartition()
{
    // open the object data file in SPIFFS
    char *pOD, buffer[32];
    FILE *fpObjData;
    strcpy(buffer, "/spiffs/objectData");
    pOD = buffer;
   	fpObjData = fopen(pOD, "r");
	if (fpObjData == NULL) {
		sysError("cannot open object data", pOD);
		exit(1);
	}

    const esp_partition_t* part;
    part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "objects");
    eraseObjectDataPartition(part);

    size_t offset = 0;
    char *fileBuf;
    int readBytes = -1;
    esp_err_t err;

    fileBuf = heap_caps_malloc(4096, MALLOC_CAP_DEFAULT);

    printf("Writing objects partition\n");
    while (readBytes != 0) {
        readBytes = fread(fileBuf, 1, 4096, fpObjData);
//       if (readBytes && (readBytes != 1))
//	        sysError("objectData read count error", "");
        // ESP_LOGI(TAG, "Read object data bytes: (%d)\n", readBytes);
        if (readBytes > 0) {
            err = esp_partition_write(part, offset, fileBuf, (size_t) readBytes);
            if (err != ESP_OK) {
                ESP_LOGE(ESP_TAG, "esp_partition_write failed. (%d)\n", err);
            } else {
                printf(readBytes < 4096 ? "o" : "O");
            }
            offset += readBytes;
        } else {
            printf("\n");
        }

    }

    fclose(fpObjData);
    ESP_LOGI(ESP_TAG, "Done writing objects partition");

}
#endif // WRITE_OBJECT_PARTITION

void eraseObjectDataPartition(esp_partition_t* part)
{
    esp_err_t err;
    err = esp_partition_erase_range(part, 0, part->size);
    if (err != ESP_OK) {
        ESP_LOGE(ESP_TAG, "esp_partition_erase_range failed. (%d)\n", err);
    } else {
        ESP_LOGI(ESP_TAG, "Erased Objects Partition\n");
    }
}

#endif