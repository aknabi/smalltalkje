/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021
*/

#include "target.h"

#ifdef TARGET_ESP32

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_spiffs.h"

#include "esp_vfs_dev.h"
#include "esp_spi_flash.h"
#include "esp_partition.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "m5stickc.h"

#include "process.h"
#include "names.h"

#include "esp_log.h"

#include "driver/uart.h"

#endif // TARGET_ESP32

#ifdef TARGET_ESP32

static const char *ESP_TAG = "ESP32";

#if TARGET_DEVICE == DEVICE_M5STICKC

extern object buttonProcesses[4];
extern void runBlockAfter(object block, object arg, int ticks);
extern void uart_input_init();

void m5ButtonHandler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    object buttonBlock = nilobj;
    object eventDict = globalSymbol("EventHandlerBlocks");
    char *eventStr;
    if (eventDict == nilobj)
        return;

    if (base == m5button_a.esp_event_base)
    {
        switch (id)
        {
        case M5BUTTON_BUTTON_CLICK_EVENT:
            eventStr = "BigButtonClicked";
            break;
        case M5BUTTON_BUTTON_HOLD_EVENT:
            eventStr = "BigButtonHeld";
            break;
        }
    }
    else if (base == m5button_b.esp_event_base)
    {
        switch (id)
        {
        case M5BUTTON_BUTTON_CLICK_EVENT:
            eventStr = "LittleButtonClicked";
            break;
        case M5BUTTON_BUTTON_HOLD_EVENT:
            eventStr = "LittleButtonHeld";
            break;
        }
    }
    buttonBlock = nameTableLookup(eventDict, eventStr);
    if (buttonBlock != nilobj)
    {
        // runBlock(buttonBlock, nilobj);
        // Schedule as a Interrupt
        // runBlockAfter(buttonBlock, nilobj, 0);
        queueVMBlockToRun(buttonBlock);
    }
}

// To ensure we don't init M5StickC twice... causes big issues
bool isM5InitCalled = false;

void m5StickInit()
{
    if (isM5InitCalled)
        return;
    isM5InitCalled = true;

    // Initialize M5StickC
    // This initializes the event loop, power, button and display
    m5stickc_config_t m5config = M5STICKC_CONFIG_DEFAULT();
    m5config.power.lcd_backlight_level = 3; // Set starting backlight level
    m5_init(&m5config);

    font_rotate = 0;
    text_wrap = 0;
    font_transparent = 0;
    font_forceFixed = 0;
    gray_scale = 0;
    TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
    TFT_setRotation(LANDSCAPE);
    TFT_setFont(DEFAULT_FONT, NULL);
    TFT_resetclipwin();
    _bg = TFT_BLACK;
    _fg = TFT_WHITE;
    TFT_fillScreen(_bg);

    // Register for button events
    esp_event_handler_register_with(m5_event_loop, M5BUTTON_A_EVENT_BASE, M5BUTTON_BUTTON_CLICK_EVENT, m5ButtonHandler, NULL);
    esp_event_handler_register_with(m5_event_loop, M5BUTTON_A_EVENT_BASE, M5BUTTON_BUTTON_HOLD_EVENT, m5ButtonHandler, NULL);
    esp_event_handler_register_with(m5_event_loop, M5BUTTON_B_EVENT_BASE, M5BUTTON_BUTTON_CLICK_EVENT, m5ButtonHandler, NULL);
    esp_event_handler_register_with(m5_event_loop, M5BUTTON_B_EVENT_BASE, M5BUTTON_BUTTON_HOLD_EVENT, m5ButtonHandler, NULL);
}

#endif // DEVICE_M5STICKC

void app_main(void)
{
    uart_input_init();

    ESP_LOGI(ESP_TAG, "Fresh free heap size: %d", esp_get_free_heap_size());

    startup();

    while (true)
    {
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

void initFileSystem()
{
    initSPIFFSPartition("storage", "/spiffs");
    // initSPIFFSPartition("objects", "/objects");
    // #ifndef WRITE_OBJECT_PARTITION
    // setupObjectData();
    // #endif
}

void initSPIFFSPartition(char *partitionName, char *basePath)
{
    ESP_LOGI(ESP_TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = basePath,
        .partition_label = partitionName,
        .max_files = 5,
        .format_if_mount_failed = false};

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(ESP_TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(ESP_TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(ESP_TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(ESP_TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(ESP_TAG, "Partition size: total: %d, used: %d", total, used);
    }
}

spi_flash_mmap_handle_t objectDataHandle;
extern void *objectData;

void setupObjectData()
{
    const esp_partition_t *part;
    esp_err_t err;

    part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "objects");
    esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "objects");
    err = esp_partition_mmap(part, 0, part->size, SPI_FLASH_MMAP_DATA, (const void **)&objectData, &objectDataHandle);

    if (err != ESP_OK)
    {
        ESP_LOGE(ESP_TAG, "esp_partition_mmap failed. (%d)\n", err);
        //delay(10000);
        // abort();
    }
    else
    {
        // ESP_LOGE(ESP_TAG, "Mapped Objects Partition to address %d", objectData);
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
    if (fpObjData == NULL)
    {
        sysError("cannot open object data", pOD);
        return;
    }

    char c = -1;
    while (!(c != 0 && (c == 89 || c == 121 || c == 78 || c == 110)))
    {
        if (c != 0) {
            ESP_LOGI(ESP_TAG, "Write objects partition? (Yy/Nn) >");
            fflush(stdout);
        }
        c = getInputCharacter();
    }

    puts("\n");
    if (c != 89 && c != 121)
    {
        ESP_LOGI(ESP_TAG, "Okay, skipping objects partition... Launch Smalltalk");
        return;
    }

    const esp_partition_t *part;
    part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "objects");
    eraseObjectDataPartition(part);

    size_t offset = 0;
    char *fileBuf;
    int readBytes = -1;
    esp_err_t err;

    fileBuf = heap_caps_malloc(4096, MALLOC_CAP_DEFAULT);

    printf("Writing objects partition\n");
    fflush(stdout);
    while (readBytes != 0)
    {
        readBytes = fread(fileBuf, 1, 4096, fpObjData);
        if (readBytes > 0)
        {
            err = esp_partition_write(part, offset, fileBuf, (size_t)readBytes);
            if (err != ESP_OK)
            {
                ESP_LOGE(ESP_TAG, "esp_partition_write failed. (%d)\n", err);
                fflush(stdout);
            }
            else
            {
                printf(readBytes < 4096 ? "o" : "O");
                fflush(stdout);
            }
            offset += readBytes;
        }
        else
        {
            printf("\n");
        }
    }

    fclose(fpObjData);
    ESP_LOGI(ESP_TAG, "Done writing objects partition. Hit <Return> to start smalltalk");
    fflush(stdout);
    fgetc(stdin);
}

#endif // WRITE_OBJECT_PARTITION

void eraseObjectDataPartition(esp_partition_t *part)
{
    esp_err_t err;
    err = esp_partition_erase_range(part, 0, part->size);
    if (err != ESP_OK)
    {
        ESP_LOGE(ESP_TAG, "esp_partition_erase_range failed. (%d)\n", err);
    }
    else
    {
        ESP_LOGI(ESP_TAG, "Erased Objects Partition\n");
    }
}

#endif