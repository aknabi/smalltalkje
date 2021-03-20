/*
    Smalltalkje, version 1
    Written by Abdul Nabi, code krafters, March 2021

    datetime.h
    
*/

extern void get_esp32_time(void);
extern char *current_time_string(char *format);
extern void setTimeZone(char *tzString);
extern time_t getEpochSeconds(void);
extern int get_time_component(time_t *epochSeconds, int component);
extern char *current_time_string(char *format);
extern char *time_string(time_t* epochSeconds, char *format);
extern time_t setNewDate(time_t *epochSeconds, int day, int month, int year);
extern time_t setNewTime(time_t *epochSeconds, int hour, int minutes, int seconds);
extern void get_sntp_time(void);

