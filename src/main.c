/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021
	Based on Little Smalltalk, version 3
	Written by Tim Budd, Oregon State University, July 1988

	Main Program Entry Point
	
	This module contains the main entry points and initialization routines
	for the Smalltalkje system. It handles:
	
	1. System Initialization:
	   - Memory manager initialization
	   - File system setup
	   - Loading the Smalltalk image (from various sources)
	   - Setting up the execution environment
	
	2. Platform-specific Startup:
	   - ESP32 initialization (app_main entry point)
	   - FreeRTOS task configuration
	   - Heap management and monitoring
	
	3. Image Loading Options:
	   - Standard monolithic image file
	   - Split object table and data files
	   - Memory-mapped Flash object data for ESP32
	
	This file is the starting point for the Smalltalkje execution and
	coordinates the initialization of all other subsystems.
*/

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include <stdlib.h>
#include "process.h"

#include "env.h"
#include "names.h"

#include "target.h"

/* Tag for logging messages */
static const char *TAG = "stje";

/* Pointer to object data in Flash memory (for MAP_FLASH_OBJECT_DATA mode) */
void *objectData;

/* External function declarations */
extern void initFileSystem();

#ifdef WRITE_OBJECT_PARTITION
extern void writeObjectDataPartition();
#endif

#ifdef TARGET_ESP32

/**
 * Main entry point for ESP32 platform
 * 
 * This function is called by the ESP-IDF framework when the system boots.
 * It initializes the UART for input/output, starts the Smalltalk system,
 * and then keeps the FreeRTOS scheduler running.
 * 
 * The function:
 * 1. Initializes the UART for console I/O
 * 2. Logs initial free heap size for monitoring
 * 3. Calls startup() to initialize and launch Smalltalk
 * 4. Explicitly starts the FreeRTOS scheduler
 * 5. Enters an infinite loop to keep the system running
 */
void app_main(void)
{
    uart_input_init();

    ESP_LOGI(TAG, "Fresh free heap size: %d", esp_get_free_heap_size());

    startup();

    // System seems to run smoother with this, not necessary, but need to dig in to details
    vTaskStartScheduler();

    while (true)
    {
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

#endif

/**
 * Initialize the system and start Smalltalk
 * 
 * This function performs the core initialization of Smalltalkje:
 * 1. Initializes the file system
 * 2. Sets up the VM block execution queue
 * 3. On ESP32, initializes non-volatile storage (NVS)
 * 4. On ESP32 with WRITE_OBJECT_PARTITION, writes object data to flash
 * 5. Launches the Smalltalk environment
 * 
 * This is called from app_main() on ESP32 or directly from main() on other platforms.
 */
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

/**
 * Opens a file with error checking
 * 
 * This utility function opens a file and performs error checking.
 * If the file cannot be opened, it reports a system error and exits.
 * 
 * @param filename The path to the file to open
 * @param mode The file open mode (e.g., "r" for read, "w" for write)
 * @return A file pointer to the opened file
 */
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

/**
 * Reads object table and data from separate files
 * 
 * This function opens the objectTable and objectData files from
 * the SPIFFS filesystem and loads them using readObjectFiles().
 * This implements the OBJECT_FILES image loading strategy.
 */
void readObjects()
{
    FILEP fpOT = openFile("/spiffs/objectTable", "r");
    FILEP fpOD = openFile("/spiffs/objectData", "r");
    readObjectFiles(fpOT, fpOD);
}

/**
 * Image Loading Strategies
 * 
 * These constants define different ways of loading the Smalltalk image:
 * 
 * SYSTEM_IMAGE (1): Load a traditional monolithic image file
 * OBJECT_FILES (2): Load object table and data from separate files
 * MAP_FLASH_OBJECT_DATA (3): Load object table from file and map object data from flash
 * 
 * The default strategy is MAP_FLASH_OBJECT_DATA, which is most memory-efficient
 * on ESP32 as it keeps immutable objects in flash memory.
 */
#define SYSTEM_IMAGE 1          /* Load "normal" systemImageFile */
#define OBJECT_FILES 2          /* Load object table and data from separate files */
#define MAP_FLASH_OBJECT_DATA 3 /* Load table from file, map data from flash */

#define IMAGE_TYPE MAP_FLASH_OBJECT_DATA

/**
 * FreeRTOS task for running Smalltalk (currently commented out)
 * 
 * This function would run the Smalltalk interpreter in a separate FreeRTOS task.
 * Currently disabled in favor of running directly in the main task.
 * 
 * @param process The Smalltalk process object to execute
 */
// void smalltalkTask(void *process)
// {
//     while (execute((object)process, 15000))
//     {
//         // printf( "Free heap with ST running: %d\n", GET_FREE_HEAP_SIZE() );
//     }
//     /* delete a task when finish */
//     vTaskDelete(NULL);
// }

/**
 * Launch and run the Smalltalk environment
 * 
 * This function is the heart of Smalltalkje startup:
 * 1. Initializes the memory manager
 * 2. Loads the Smalltalk image using the configured strategy
 * 3. Initializes common symbols
 * 4. Looks up the initial system process
 * 5. Displays startup banner
 * 6. Executes the system process in the interpreter
 * 
 * The system continues running until the main process terminates.
 */
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
