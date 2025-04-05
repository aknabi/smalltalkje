/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021

	Platform Target Configuration Header
	
	This header defines platform-specific configurations, macros, and includes
	for different target environments. It provides a uniform interface for
	platform-dependent functionality like logging, memory management, and
	system services.
	
	The file uses conditional compilation to select the appropriate implementation
	based on the target platform, currently supporting:
	1. ESP32 - Using FreeRTOS and ESP-IDF libraries
	2. Other platforms (desktop environments) - Using standard C libraries
	
	This abstraction layer allows the Smalltalkje codebase to be portable while
	still leveraging platform-specific optimizations and features.
*/
#include "build.h"

/**
 * ESP32 Platform Configuration
 * 
 * When targeting the ESP32 platform, we include ESP-IDF and FreeRTOS headers
 * and define macros that use the ESP32's native logging and memory management
 * facilities. The ESP32 is an embedded platform with specific requirements
 * and capabilities that differ from desktop environments.
 */
#ifdef TARGET_ESP32

#include "esp_err.h"    /* ESP32 error handling */
#include "esp_log.h"    /* ESP32 logging system */

#include "freertos/FreeRTOS.h"  /* FreeRTOS core */
#include "freertos/task.h"      /* FreeRTOS task management */

/**
 * ESP32 Logging Macros
 * 
 * These macros provide a consistent interface for logging at different levels
 * while using the ESP-IDF's native logging system. The ESP-IDF logger supports
 * features like log filtering by tag and level, timestamps, and colored output.
 */
#define TT_LOG_ERROR(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__) /* Error level logging */
#define TT_LOG_WARN(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)  /* Warning level logging */
#define TT_LOG_INFO(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)  /* Info level logging */
#define TT_LOG_DEBUG(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__) /* Debug level logging */
#define TT_LOG_VERB(tag, format, ...) ESP_LOGV(tag, format, ##__VA_ARGS__)  /* Verbose level logging */

/**
 * ESP32 Memory Management
 * 
 * Provides a platform-independent way to check available memory.
 * On ESP32, this uses the ESP-IDF's heap size function.
 */
#define GET_FREE_HEAP_SIZE() esp_get_free_heap_size() /* Get available heap size in bytes */

/**
 * Non-ESP32 Platform Configuration
 * 
 * For desktop and other non-ESP32 platforms, we define simpler implementations
 * of the same macros using standard C libraries. This allows the same code to
 * run on both embedded devices and development machines.
 */
#else
/**
 * Generic Logging Macros
 * 
 * These provide simplified logging implementations for non-ESP32 platforms,
 * typically outputting to stderr. They maintain the same interface as the
 * ESP32 versions for compatibility.
 */
#define TT_LOG_MSG(tag, level, format, ...) fprintf(stderr, format, ##__VA_ARGS__);
#define TT_LOG_ERROR(tag, format, ...) TT_LOG_MSG(tag, "ERROR", format, ##__VA_ARGS__)
#define TT_LOG_WARN(tag, format, ...) TT_LOG_MSG(tag, "WARN", format, ##__VA_ARGS__)
#define TT_LOG_INFO(tag, format, ...) fprintf(stderr, format, ##__VA_ARGS__);
#define TT_LOG_DEBUG(tag, format, ...) TT_LOG_MSG(tag, "DEBUG", format, ##__VA_ARGS__)
#define TT_LOG_VERB(tag, format, ...) TT_LOG_MSG(tag, "VERB", format, ##__VA_ARGS__)

/**
 * Generic Memory Management
 * 
 * On non-ESP32 platforms, uses the standard C library's mallinfo function
 * to report available heap memory.
 */
#define GET_FREE_HEAP_SIZE() mallinfo().fordblks

#endif /* TARGET_ESP32 */
