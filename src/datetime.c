#include "time.h"
#include "build.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "esp_sntp.h"

/* epoch seconds */
time_t now;

/*
  int    tm_sec   seconds [0,61]
  int    tm_min   minutes [0,59]
  int    tm_hour  hour [0,23]
  int    tm_mday  day of month [1,31]
  int    tm_mon   month of year [0,11]
  int    tm_year  years since 1900
  int    tm_wday  day of week [0,6] (Sunday = 0)
  int    tm_yday  day of year [0,365]
  int    tm_isdst daylight savings flag

  The value of tm_isdst is positive if Daylight Saving Time is in effect, 
  0 if Daylight Saving Time is not in effect, and negative if the information is not available.
*/
struct tm timeinfo = { 0 };

char strftime_buf[64];
static const char * TAG = "datetime";

static void sntp_obtain_time(void)
{
#ifdef TARGET_ESP32
    // initialize_sntp();

    // wait for time to be set
    int retry = 0;
    const int retry_count = 3;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
#endif
    time(&now);
    localtime_r(&now, &timeinfo);
}

void init_sntp_time(void) 
{
#ifdef TARGET_ESP32
    sntp_setservername(0, "pool.ntp.org");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_init();
#endif
}

void get_sntp_time(void) 
{
#ifdef TARGET_ESP32
    sntp_obtain_time();
#endif
    get_esp32_time();
}

char *time_string(time_t* epochSeconds, char *format)
{
    struct tm *local_time = localtime(&epochSeconds);
    size_t n = strftime(strftime_buf, sizeof(strftime_buf), format, local_time);
    char *retStr = NULL;
    if (n > 0) {
      retStr = strftime_buf;
    }
    return retStr;
}

int get_time_component(time_t *epochSeconds, int component)
{
    struct tm *local_time = localtime(&epochSeconds);
    if (component == 1) {
        return timeinfo.tm_sec;
    } else if (component == 2) {
        return  timeinfo.tm_min;
    } else if (component == 3) {
        return timeinfo.tm_hour;
    } else if (component == 4) {
        return timeinfo.tm_mday;
    } else if (component == 5) {
        return timeinfo.tm_mon + 1;
    } else if (component == 6) {
        return timeinfo.tm_year;
    }
    return 0;
}


time_t getEpochSeconds(void)
{
    get_esp32_time();
    return now;
}

void setTimeZone(char *tzString)
{
    setenv("TZ", tzString, 1);
    tzset();
    localtime_r(&now, &timeinfo);
}

void get_esp32_time(void) {
    time(&now);
    localtime_r(&now, &timeinfo);

    // Set timezone to Eastern Standard Time and print local time
    // setTimeZone("EST5EDT,M3.2.0/2,M11.1.0");
    // strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    // ESP_LOGI(TAG, "The current date/time in New York is: %s", strftime_buf);

    // Set timezone to CET
    // setTimeZone("UTC-1");
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    // ESP_LOGI(TAG, "The current date/time in Amsterdam is: %s", strftime_buf);
}

time_t setNewDate(time_t *epochSeconds, int day, int month, int year)
{
    struct tm *newTime = localtime(&epochSeconds);
    newTime->tm_mday = day;
    newTime->tm_mon = month - 1;
    newTime->tm_year = year - 1900;
    newTime->tm_isdst = -1;
    return mktime(newTime);
}

time_t setNewTime(time_t *epochSeconds, int hour, int minutes, int seconds)
{
    struct tm *newTime = localtime(&epochSeconds);
    // newTime->tm_hour = hour;
    // newTime->tm_min = minutes;
    // newTime->tm_sec = seconds;
    // newTime->tm_isdst = -1;
    return mktime(newTime);
}

char *current_time_string(char *format)
{
    get_esp32_time();
    size_t n = strftime(strftime_buf, sizeof(strftime_buf), format, &timeinfo);
    char *retStr = NULL;
    if (n > 0) {
      retStr = strftime_buf;
    }
    return retStr;
}
