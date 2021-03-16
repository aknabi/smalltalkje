#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include <stdlib.h>
#include "process.h"

#include "env.h"
#include "names.h"

#include "target.h"

static const char *TAG = "stje";

void *objectData;

extern void initFileSystem();

#ifdef WRITE_OBJECT_PARTITION
extern void writeObjectDataPartition();
#endif

// Initialize various subsystems (which can be chip/platform dependent)
noreturn startup()
{
    initFileSystem();

    initVMBlockToRunQueue();

#ifdef TARGET_ESP32
    nvs_init();
#ifdef WRITE_OBJECT_PARTITION
    int value;
    esp_err_t err = nvs_read_int32("_skipODP", &value);
    if (err != ESP_OK || value == 0) {
        writeObjectDataPartition();
    }
    setupObjectData();
#endif
#endif
    TT_LOG_INFO(TAG, "Pre-smalltalk start free heap size: %d", GET_FREE_HEAP_SIZE());
    launchSmalltalk();
    // #endif //WRITE_OBJECT_PARTITION
}

FILEP openFile(STR filename, STR mode)
{
    FILEP fp = fopen(filename, mode);
    if (fp == NULL)
    {
        sysError("cannot open object file %s", filename);
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

// void smalltalkTask(void *process)
// {
//     while (execute((object)process, 15000))
//     {
//         // printf( "Free heap with ST running: %d\n", GET_FREE_HEAP_SIZE() );
//     }
//     /* delete a task when finish */
//     vTaskDelete(NULL);
// }

void launchSmalltalk()
{

    FILE *fp;
    object firstProcess;
    char *p, buffer[120];

    TT_LOG_INFO(TAG, "Starting Smalltalkje, Version 1\n");

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

    if (firstProcess == nilobj)
    {
        sysError("no initial process", "in image");
        exit(1);
    }

    printf("Smalltalkje, Version 1\n");
    printf("Written by Abdul Nabi\n");
    printf("Based on Little Smalltalk, Version 3.1\n");
    printf("Written by Tim Budd, Oregon State University\n");
    printf("Updated for modern systems by Charles Childers\n");
    printf("Free heap with ST running: %d\n", GET_FREE_HEAP_SIZE());

    // For now comment out running in a FreeRTOS task as it was cool, but for what benefit?
// #ifdef TARGET_ESP32
//     // FreeRTOS specific
//     xTaskCreate(
//         smalltalkTask, /* Task function. */
//         "smalltalk",   /* name of task. */
//         8096,          /* Stack size of task */
//         firstProcess,  /* parameter of the task (the Smalltalk process to run) */
//         1,             /* priority of the task */
//         NULL);         /* Task handle to keep track of created task */
// #else
    while (execute(firstProcess, 15000))
    {
        // printf("Free heap with ST running: %d\n", GET_FREE_HEAP_SIZE());
    }
// #endif
}
