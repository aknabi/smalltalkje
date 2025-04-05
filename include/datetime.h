/*
    Smalltalkje, version 1
    Written by Abdul Nabi, code krafters, March 2021

    Date and Time Management Header
    
    This header defines the interface for managing date and time functionality
    in the Smalltalkje system. It provides functions for retrieving the current
    time, formatting time values, setting time zones, and manipulating date and
    time components.
    
    This module is particularly important for ESP32 applications that need to:
    1. Display accurate time information
    2. Perform time-based operations
    3. Coordinate with network services using timestamps
    4. Synchronize time via NTP servers
    
    The implementation supports both local ESP32 time and network time via SNTP
    (Simple Network Time Protocol). The functions handle time zone conversions
    and various time formats.
*/

/**
 * Update the internal time value from the ESP32's RTC
 * 
 * This function refreshes the system's internal time value by reading
 * the current time from the ESP32's real-time clock.
 */
extern void get_esp32_time(void);

/**
 * Format the current time as a string using the specified format
 * 
 * This function gets the current time and formats it according to
 * the provided format string. The format string follows the standard
 * strftime() format specifiers, such as:
 * - %c: Standard date and time string
 * - %Y: Year (4 digits)
 * - %m: Month (01-12)
 * - %d: Day of month (01-31)
 * - %H: Hour (00-23)
 * - %M: Minute (00-59)
 * - %S: Second (00-61)
 * 
 * @param format The format string using strftime() specifiers
 * @return A pointer to the formatted string (statically allocated)
 */
extern char *current_time_string(char *format);

/**
 * Set the system time zone
 * 
 * This function configures the time zone used for time conversions and
 * formatting. The time zone string should follow the standard TZ format
 * as described in the tzset() documentation.
 * 
 * Examples:
 * - "UTC0" for UTC/GMT
 * - "EST5EDT" for US Eastern Time
 * - "CET-1CEST" for Central European Time
 * 
 * @param tzString The time zone string in TZ format
 */
extern void setTimeZone(char *tzString);

/**
 * Get the current time as Unix epoch seconds
 * 
 * This function returns the current time as the number of seconds
 * since January 1, 1970 (Unix epoch).
 * 
 * @return The current time in epoch seconds
 */
extern time_t getEpochSeconds(void);

/**
 * Extract a specific component from a time value
 * 
 * This function extracts a particular time component from the given
 * time value. The component is specified by a numeric code:
 * 1: Seconds (0-59)
 * 2: Minutes (0-59)
 * 3: Hours (0-23)
 * 4: Day of month (1-31)
 * 5: Month (1-12)
 * 6: Year (full year, e.g., 2023)
 * 
 * @param epochSeconds Pointer to the time value in epoch seconds
 * @param component Numeric code for the desired time component
 * @return The value of the specified time component
 */
extern int get_time_component(time_t *epochSeconds, int component);

/**
 * Format a specific time value as a string
 * 
 * This function formats the given time value according to the
 * provided format string. The format string follows the standard
 * strftime() format specifiers.
 * 
 * @param epochSeconds Pointer to the time value in epoch seconds
 * @param format The format string using strftime() specifiers
 * @return A pointer to the formatted string (statically allocated)
 */
extern char *time_string(time_t* epochSeconds, char *format);

/**
 * Set a new date for a time value
 * 
 * This function modifies the date components (day, month, year) of
 * the given time value, preserving the time-of-day components.
 * 
 * @param epochSeconds Pointer to the time value to modify
 * @param day New day of month (1-31)
 * @param month New month (1-12)
 * @param year New year (e.g., 2023)
 * @return The updated time value in epoch seconds
 */
extern time_t setNewDate(time_t *epochSeconds, int day, int month, int year);

/**
 * Set a new time of day for a time value
 * 
 * This function modifies the time-of-day components (hour, minute, second)
 * of the given time value, preserving the date components.
 * 
 * @param epochSeconds Pointer to the time value to modify
 * @param hour New hour (0-23)
 * @param minutes New minutes (0-59)
 * @param seconds New seconds (0-59)
 * @return The updated time value in epoch seconds
 */
extern time_t setNewTime(time_t *epochSeconds, int hour, int minutes, int seconds);

/**
 * Synchronize time with an SNTP server
 * 
 * This function attempts to obtain the current time from an SNTP
 * (Simple Network Time Protocol) server and update the system's
 * internal time accordingly. This requires an active network connection.
 */
extern void get_sntp_time(void);
