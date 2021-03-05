/**
 * m5rtc.h
 *
 * (C) 2021 - Abdul Nabi <abdul@codekrafters.com>
 * This code is licensed under the MIT License.
 */

#ifndef _M5RTC_H_
#define _M5RTC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"

#define BM8563_I2C_ADDR 0x51

typedef struct
{
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
} RTC_TimeTypeDef;


typedef struct
{
  uint8_t WeekDay;
  uint8_t Month;
  uint8_t Date;
  uint16_t Year;
} RTC_DateTypeDef;

uint8_t rtc_data[7]; 
uint8_t asc[14];

/**
 * @brief   Initialize RTC configuring BM8563 RTC IC
 *
 * @return  ESP_OK success
 *          ESP_FAIL failed
 */
esp_err_t m5rtc_init(void);

esp_err_t getBM8563Time(void);

esp_err_t setRTCTime(RTC_TimeTypeDef* RTC_TimeStruct);
esp_err_t setRTCDate(RTC_DateTypeDef* RTC_DateStruct);

esp_err_t getRTCTime(RTC_TimeTypeDef* RTC_TimeStruct);
esp_err_t getRTCDate(RTC_DateTypeDef* RTC_DateStruct); 

#ifdef __cplusplus
}
#endif

#endif // _M5RTC_H_