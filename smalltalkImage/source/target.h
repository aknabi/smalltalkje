#include "build.h"

#ifdef TARGET_ESP32

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TT_LOG_ERROR( tag, format, ... ) ESP_LOGE(tag, format, ##__VA_ARGS__)
#define TT_LOG_WARN( tag, format, ... ) ESP_LOGW(tag, format, ##__VA_ARGS__)
#define TT_LOG_INFO( tag, format, ... ) ESP_LOGI(tag, format, ##__VA_ARGS__)
#define TT_LOG_DEBUG( tag, format, ... ) ESP_LOGD(tag, format, ##__VA_ARGS__)
#define TT_LOG_VERB( tag, format, ... ) ESP_LOGV(tag, format, ##__VA_ARGS__)
#define GET_FREE_HEAP_SIZE() esp_get_free_heap_size()

#else
#define TT_LOG_MSG(tag, level, format, ... ) fprintf(stderr, format, ##__VA_ARGS__);
#define TT_LOG_ERROR( tag, format, ... ) TT_LOG_MSG(tag, "ERROR", format, ##__VA_ARGS__)
#define TT_LOG_WARN( tag, format, ... ) TT_LOG_MSG(tag, "WARN", format, ##__VA_ARGS__)
#define TT_LOG_INFO( tag, format, ... ) fprintf(stderr, format, ##__VA_ARGS__);
#define TT_LOG_DEBUG( tag, format, ... ) TT_LOG_MSG(tag, "DEBUG", format, ##__VA_ARGS__)
#define TT_LOG_VERB( tag, format, ... ) TT_LOG_MSG(tag, "VERB", format, ##__VA_ARGS__)
#define GET_FREE_HEAP_SIZE() mallinfo().fordblks

#endif