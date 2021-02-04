#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include <stdlib.h>
#include "env.h"
#include "memory.h"
#include "names.h"

#include "target.h"

#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "m5stickc.h"

static const char *TAG = "TinyTalk";

void* objectData;

extern void initFileSystem();

#ifdef WRITE_OBJECT_PARTITION 
extern void writeObjectDataPartition();
#endif

noreturn startup()
{
    initFileSystem();
 
 #ifdef WRITE_OBJECT_PARTITION
    writeObjectDataPartition();
 #else
     TT_LOG_INFO(TAG, "Pre-smalltalk start free heap size: %d", GET_FREE_HEAP_SIZE() );
    launchSmalltalk();
 #endif //WRITE_OBJECT_PARTITION
}

FILEP openFile(STR filename, STR mode)
{
    FILEP fp = fopen(filename, mode);
    if (fp == NULL) {
		sysError("cannot open object table", filename);
		exit(1);
	}
   	return fp;
}

void readObjects()
{
   	FILEP fpOT = openFile("/spiffs/objectTable", "r");
   	FILEP fpOD = openFile("/spiffs/objectData", "r");
    readObjectFiles(fpOT, fpOD);
}

// Load "Normal" systemImageFile
#define SYSTEM_IMAGE 1
// Load object table from seperate object table and data files objectTable and objectData
#define OBJECT_FILES 2
// Load object table from file and map object data in flash partition
#define MAP_FLASH_OBJECT_DATA 3

#define IMAGE_TYPE MAP_FLASH_OBJECT_DATA

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

void m5StickTask(void)
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
    ESP_LOGE(TAG, "Button test start");
    TFT_print("Press or hold button", CENTER, (M5DISPLAY_HEIGHT-24)/2);

    m5display_timeout(15000);
}

#endif // TEST_M5STICK

void smalltalkTask(void *process)
{
    while (execute((object) process, 15000)) {
        printf( "Free heap with ST running: %d\n", GET_FREE_HEAP_SIZE() );
    }
    /* delete a task when finish */
   vTaskDelete( NULL );
}

void launchSmalltalk()
{

    FILE *fp;
    object firstProcess;
    char *p, buffer[120];

    TT_LOG_INFO(TAG, "Starting M5StickC Test\n");
    m5StickTask();
    
    TT_LOG_INFO(TAG, "Starting TinyTalk Smalltalk, Version 3.1\n");

    initMemoryManager();

// Load "Normal" systemImageFile
#if IMAGE_TYPE == SYSTEM_IMAGE
    imageRead(openFile("/spiffs/systemImage", "r"));
#endif // SYSTEM_IMAGE


// Load object table from seperate object table and data files objectTable and objectData
#if IMAGE_TYPE == OBJECT_FILES
    readObjects();
#endif // OBJECT_FILES

// Load object table from file and map object data in flash partition
#if IMAGE_TYPE == MAP_FLASH_OBJECT_DATA
    fp = openFile("/spiffs/objectTable", "r");
    readTableWithObjects(fp, objectData);
#endif // MAP_FLASH_OBJECT_DATA

    initCommonSymbols();
    firstProcess = globalSymbol("systemProcess");
    
    if (firstProcess == nilobj) {
	    sysError("no initial process", "in image");
	    exit(1);
    }

    printf("Little Smalltalk, Version 3.1\n");
    printf("Written by Tim Budd, Oregon State University\n");
    printf("Updated for modern systems by Charles Childers\n");
    printf("Updated for embedded systems by Abdul Nabi\n");
    printf( "Free heap with ST running: %d\n", GET_FREE_HEAP_SIZE() );

#ifdef TARGET_ESP32
    // FreeRTOS specific
    xTaskCreate(
        smalltalkTask, /* Task function. */
        "smalltalk", /* name of task. */
        8096, /* Stack size of task */
        firstProcess, /* parameter of the task (the Smalltalk process to run) */
        1, /* priority of the task */
        NULL); /* Task handle to keep track of created task */
#else
    while (execute(firstProcess, 15000)) {
        printf( "Free heap with ST running: %d\n", GET_FREE_HEAP_SIZE() );
    }
#endif
}