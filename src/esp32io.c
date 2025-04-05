/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021

    ESP32 Input/Output Implementation
    
    This module provides ESP32-specific I/O functionality for the Smalltalkje system.
    It handles various ESP32 platform features including:
    
    - M5StickC button event handling
    - File system initialization and management
    - SPIFFS partition mounting and configuration
    - Object data partition management
    
    The implementation conditionally compiles based on target platform definitions,
    allowing the same codebase to be built for different ESP32-based devices
    including standard ESP32 boards, M5StickC, M5 Atom Lite, and T-Wristband.
    
    For M5StickC and similar devices, the module also handles button event
    registration and display initialization.
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

/** Tag for ESP logging */
static const char *ESP_TAG = "ESP32";

#if TARGET_DEVICE == DEVICE_M5STICKC || TARGET_DEVICE == DEVICE_M5SATOM_LITE || TARGET_DEVICE == DEVICE_T_WRISTBAND

extern object buttonProcesses[4];
extern void runBlockAfter(object block, object arg, int ticks);
extern void uart_input_init();

/**
 * Event handler for M5StickC button events
 * 
 * This function processes button events from the M5StickC device and
 * executes the appropriate Smalltalk block associated with the event.
 * Button events are looked up in the "EventHandlerBlocks" dictionary
 * by event name.
 * 
 * Supported events:
 * - BigButtonClicked: Button A click
 * - BigButtonHeld: Button A hold
 * - LittleButtonClicked: Button B click
 * - LittleButtonHeld: Button B hold
 * 
 * @param handler_arg Event handler arguments (unused)
 * @param base Event base indicating which button generated the event
 * @param id Event ID indicating the type of button event
 * @param event_data Additional event data (unused)
 */
void m5ButtonHandler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    object buttonBlock = nilobj;
    object eventDict = globalSymbol("EventHandlerBlocks");
    char *eventStr;
    
    // No event dictionary, nothing to do
    if (eventDict == nilobj)
        return;

    // Determine which event occurred based on the button and event type
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
    
    // Look up the block to execute for this event
    buttonBlock = nameTableLookup(eventDict, eventStr);
    if (buttonBlock != nilobj)
    {
        // Queue the block for execution by the VM
        queueVMBlockToRun(buttonBlock);
    }
}

/** Flag to prevent multiple M5StickC initializations */
bool isM5InitCalled = false;

/**
 * Initialize the M5StickC device
 * 
 * This function initializes the M5StickC hardware including power management,
 * display, and button event handling. The function ensures it's only called
 * once to avoid issues with multiple initializations.
 * 
 * It configures:
 * - The M5StickC event loop and power management
 * - Display settings (rotation, font, colors)
 * - Button event handlers for both buttons
 */
void m5StickInit()
{
    // Prevent multiple initializations
    if (isM5InitCalled) return;
    isM5InitCalled = true;

    // Initialize M5StickC with default configuration
    m5stickc_config_t m5config = M5STICKC_CONFIG_DEFAULT();
    m5config.power.lcd_backlight_level = 3; // Set starting backlight level
    m5_init(&m5config);

    // Configure display settings
    font_rotate = 0;
    text_wrap = 0;
    font_transparent = 0;
    font_forceFixed = 0;
    gray_scale = 0;
    
    // Set up the LCD display
    TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
    TFT_setRotation(LANDSCAPE_FLIP);
    TFT_setFont(DEFAULT_FONT, NULL);
    TFT_resetclipwin();
    _bg = TFT_BLACK;
    _fg = TFT_WHITE;
    TFT_fillScreen(_bg);

    // Register button event handlers
    esp_event_handler_register_with(m5_event_loop, M5BUTTON_A_EVENT_BASE, M5BUTTON_BUTTON_CLICK_EVENT, m5ButtonHandler, NULL);
    esp_event_handler_register_with(m5_event_loop, M5BUTTON_A_EVENT_BASE, M5BUTTON_BUTTON_HOLD_EVENT, m5ButtonHandler, NULL);
    esp_event_handler_register_with(m5_event_loop, M5BUTTON_B_EVENT_BASE, M5BUTTON_BUTTON_CLICK_EVENT, m5ButtonHandler, NULL);
    esp_event_handler_register_with(m5_event_loop, M5BUTTON_B_EVENT_BASE, M5BUTTON_BUTTON_HOLD_EVENT, m5ButtonHandler, NULL);
}

#endif // DEVICE_M5STICKC || DEVICE_M5SATOM_LITE || DEVICE_T_WRISTBAND

/**
 * Initialize the file system for the ESP32
 * 
 * This function sets up the SPIFFS file system on the ESP32. It initializes
 * the main storage partition for Smalltalkje to use.
 */
void initFileSystem()
{
    initSPIFFSPartition("storage", "/spiffs");
}

/**
 * Initialize and mount a SPIFFS partition
 * 
 * This function configures and mounts a SPIFFS partition with the specified
 * name and mount point. It also reports information about the partition size.
 * 
 * @param partitionName The label of the partition to mount
 * @param basePath The mount point path in the VFS
 */
void initSPIFFSPartition(char *partitionName, char *basePath)
{
    ESP_LOGI(ESP_TAG, "Initializing SPIFFS");

    // Configure the SPIFFS partition
    esp_vfs_spiffs_conf_t conf = {
        .base_path = basePath,
        .partition_label = partitionName,
        .max_files = 5,
        .format_if_mount_failed = false
    };

    // Initialize and mount the SPIFFS filesystem
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    // Handle initialization errors
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

    // Get and report partition size information
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

/** Handle for mapped object data partition */
spi_flash_mmap_handle_t objectDataHandle;

/** External object data pointer */
extern void *objectData;

/**
 * Map the objects partition into memory
 * 
 * This function finds the "objects" partition and maps it into memory
 * for direct access. The mapped memory is accessible through the
 * external objectData pointer.
 */
void setupObjectData()
{
    const esp_partition_t *part;
    esp_err_t err;

    // Find the objects partition
    part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "objects");
    
    // Map the partition into memory
    err = esp_partition_mmap(part, 0, part->size, SPI_FLASH_MMAP_DATA, 
                            (const void **)&objectData, &objectDataHandle);

    // Handle mapping errors
    if (err != ESP_OK)
    {
        ESP_LOGE(ESP_TAG, "esp_partition_mmap failed. (%d)\n", err);
    }
}

#ifdef WRITE_OBJECT_PARTITION

/**
 * Write object data from a file to the objects partition
 * 
 * This function reads object data from a file in the SPIFFS file system
 * and writes it to the "objects" partition. It prompts the user for
 * confirmation before proceeding.
 * 
 * Only available when WRITE_OBJECT_PARTITION is defined.
 */
void writeObjectDataPartition()
{
    // Open the object data file in SPIFFS
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

    // Prompt for confirmation
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

    // Find and erase the objects partition
    const esp_partition_t *part;
    part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "objects");
    eraseObjectDataPartition(part);

    // Prepare for writing
    size_t offset = 0;
    char *fileBuf;
    int readBytes = -1;
    esp_err_t err;

    fileBuf = heap_caps_malloc(4096, MALLOC_CAP_DEFAULT);

    // Read and write the data in chunks
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

    // Clean up and notify completion
    fclose(fpObjData);
    ESP_LOGI(ESP_TAG, "Done writing objects partition. Hit <Return> to start smalltalk");
    fflush(stdout);
    fgetc(stdin);
}

#endif // WRITE_OBJECT_PARTITION

/**
 * Erase the objects partition
 * 
 * This function erases the entire object data partition.
 * 
 * @param part Pointer to the partition to erase
 */
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
