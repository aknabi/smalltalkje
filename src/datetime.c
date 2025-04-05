/*
    Smalltalkje, version 1
    Written by Abdul Nabi, code krafters, March 2021

    Date and Time Management Implementation
    
    This module provides functions for managing date and time functionality in the Smalltalkje
    system. It implements the interface defined in datetime.h and handles time synchronization,
    formatting, and manipulation across different time zones.
    
    On ESP32 targets, this module uses the ESP-IDF SNTP (Simple Network Time Protocol) client
    to synchronize with network time servers. For other platforms, it relies on standard C
    library time functions.
    
    The implementation supports operations such as:
    - Getting the current time from the ESP32's RTC
    - Synchronizing time with an SNTP server (on ESP32)
    - Formatting time values into strings
    - Setting time zones
    - Extracting components from time values (seconds, minutes, hours, etc.)
    - Creating new date and time values by modifying existing ones
*/

#include "time.h"
#include "build.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "esp_sntp.h"

/** Current time in epoch seconds */
time_t now;

/**
 * Time structure used for representing calendar time
 * 
 * The tm structure contains the following fields:
 *   tm_sec   - seconds [0,61]
 *   tm_min   - minutes [0,59]
 *   tm_hour  - hour [0,23]
 *   tm_mday  - day of month [1,31]
 *   tm_mon   - month of year [0,11]
 *   tm_year  - years since 1900
 *   tm_wday  - day of week [0,6] (Sunday = 0)
 *   tm_yday  - day of year [0,365]
 *   tm_isdst - daylight savings flag
 * 
 * The tm_isdst field is positive if Daylight Saving Time is in effect,
 * 0 if Daylight Saving Time is not in effect, and negative if the
 * information is not available.
 */
struct tm timeinfo = { 0 };

/** Buffer for storing formatted time strings */
char strftime_buf[64];

/** Tag for ESP logging */
static const char * TAG = "datetime";

/**
 * Wait for SNTP to obtain the current time from the time server
 * 
 * This function attempts to sync with an SNTP server, waiting up to
 * three attempts for the time to be set. Once sync is complete or
 * the retry count is exhausted, the current time is fetched and stored.
 */
static void sntp_obtain_time(void)
{
#ifdef TARGET_ESP32
    // Wait for time to be set by SNTP
    int retry = 0;
    const int retry_count = 3;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
#endif
    // Get the current time and convert to local time
    time(&now);
    localtime_r(&now, &timeinfo);
}

/**
 * Initialize the SNTP client with default settings
 * 
 * Configures the SNTP client to use pool.ntp.org as the time server
 * and sets it to poll mode. On non-ESP32 platforms, this function
 * does nothing.
 */
void init_sntp_time(void) 
{
#ifdef TARGET_ESP32
    sntp_setservername(0, "pool.ntp.org");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_init();
#endif
}

/**
 * Get the current time from SNTP and update the local time information
 * 
 * This function first attempts to get the time from SNTP (if on ESP32),
 * then updates the local time information by reading the system clock.
 */
void get_sntp_time(void) 
{
#ifdef TARGET_ESP32
    sntp_obtain_time();
#endif
    get_esp32_time();
}

/**
 * Format a time value into a string according to the specified format
 * 
 * @param epochSeconds Pointer to the time value in epoch seconds
 * @param format The format string following strftime() conventions
 * @return Pointer to the formatted string or NULL on failure
 */
char *time_string(time_t* epochSeconds, char *format)
{
    struct tm *local_time = localtime(epochSeconds);
    size_t n = strftime(strftime_buf, sizeof(strftime_buf), format, local_time);
    char *retStr = NULL;
    if (n > 0) {
      retStr = strftime_buf;
    }
    return retStr;
}

/**
 * Extract a specific component from a time value
 * 
 * Extracts a particular component from the time structure based on the
 * component code:
 * - 1: Seconds (0-59)
 * - 2: Minutes (0-59)
 * - 3: Hours (0-23)
 * - 4: Day of month (1-31)
 * - 5: Month (1-12)
 * - 6: Year (years since 1900)
 * 
 * @param epochSeconds Pointer to the time value in epoch seconds
 * @param component Numeric code for the desired component
 * @return The value of the specified component, or 0 if invalid
 */
int get_time_component(time_t *epochSeconds, int component)
{
    struct tm *local_time = localtime(epochSeconds);
    
    switch (component) {
        case 1: return timeinfo.tm_sec;
        case 2: return timeinfo.tm_min;
        case 3: return timeinfo.tm_hour;
        case 4: return timeinfo.tm_mday;
        case 5: return timeinfo.tm_mon + 1; // Convert from 0-11 to 1-12
        case 6: return timeinfo.tm_year;
        default: return 0;
    }
}


/**
 * Get the current time in epoch seconds
 * 
 * Updates the current time value and returns it in epoch seconds
 * (seconds since January 1, 1970, 00:00:00 UTC).
 * 
 * @return The current time in epoch seconds
 */
time_t getEpochSeconds(void)
{
    get_esp32_time();
    return now;
}

/**
 * Set the system time zone
 * 
 * Updates the time zone setting by setting the TZ environment variable
 * and calling tzset() to update the timezone data. Also updates the
 * local time information based on the new time zone.
 * 
 * @param tzString The time zone string in TZ format (e.g., "UTC0", "EST5EDT")
 */
void setTimeZone(char *tzString)
{
    setenv("TZ", tzString, 1);
    tzset();
    localtime_r(&now, &timeinfo);
}

/**
 * Update the internal time value from the system's clock
 * 
 * Gets the current time from the system clock and converts it to local time.
 * Formats the time as a string for potential display or logging.
 */
void get_esp32_time(void) {
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Format the time as a standard date and time string
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
}

/**
 * Set a new date for a time value
 * 
 * Creates a new time value by modifying the date components of an existing
 * time value while preserving the time-of-day components.
 * 
 * @param epochSeconds Pointer to the time value to modify
 * @param day New day of month (1-31)
 * @param month New month (1-12)
 * @param year New year (full year, e.g., 2023)
 * @return The updated time value in epoch seconds
 */
time_t setNewDate(time_t *epochSeconds, int day, int month, int year)
{
    struct tm *newTime = localtime(epochSeconds);
    newTime->tm_mday = day;
    newTime->tm_mon = month - 1;  // Convert from 1-12 to 0-11
    newTime->tm_year = year - 1900;  // Years since 1900
    newTime->tm_isdst = -1;  // Let the system determine DST
    return mktime(newTime);
}

/**
 * Set a new time of day for a time value
 * 
 * Creates a new time value by modifying the time-of-day components of an
 * existing time value while preserving the date components.
 * 
 * Note: This function currently returns the original time unchanged due to
 * commented-out code. This should be fixed in a future update.
 * 
 * @param epochSeconds Pointer to the time value to modify
 * @param hour New hour (0-23)
 * @param minutes New minutes (0-59)
 * @param seconds New seconds (0-59)
 * @return The updated time value in epoch seconds
 */
time_t setNewTime(time_t *epochSeconds, int hour, int minutes, int seconds)
{
    struct tm *newTime = localtime(epochSeconds);
    newTime->tm_hour = hour;
    newTime->tm_min = minutes;
    newTime->tm_sec = seconds;
    newTime->tm_isdst = -1;  // Let the system determine DST
    return mktime(newTime);
}

/**
 * Format the current time as a string using the specified format
 * 
 * Updates the current time and formats it according to the provided
 * format string, which follows standard strftime() conventions.
 * 
 * @param format The format string (e.g., "%Y-%m-%d %H:%M:%S")
 * @return Pointer to the formatted string or NULL on failure
 */
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
