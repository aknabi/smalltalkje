/**
 * m5rtc.c
 *
 * (C) 2021 - Abdul Nabi <abdul@codekrafters.com>
 * This code is licensed under the MIT License.
 */

#include "include/m5rtc.h"
#include "esp_sntp.h"

// if we want to use esp-idf sntp stuff
#include <lwip/apps/sntp.h>

static const char * TAG = "m5rtc";

uint8_t rtc_data[7]; 
uint8_t asc[14];

static RTC_TimeTypeDef rtcTimeNow;

static uint8_t bcd2ToByte(uint8_t value);
static uint8_t byteToBcd2(uint8_t value);
static void bcd2ascii(void);
static void maskRTCData(void);


    

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

static void sntp_obtain_time(void)
{
    // initialize_sntp();

    // wait for time to be set
    int retry = 0;
    const int retry_count = 3;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    time(&now);
    localtime_r(&now, &timeinfo);
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

    char strftime_buf[64];

    // Set timezone to Eastern Standard Time and print local time
    setTimeZone("EST5EDT,M3.2.0/2,M11.1.0");
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in New York is: %s", strftime_buf);

    // Set timezone to EU Central Time
    setTimeZone("UTC-1");
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Amsterdam is: %s", strftime_buf);
}

void get_sntp_time(void) {
    sntp_obtain_time();
    get_esp32_time();
}

void init_sntp_time(void) {
    sntp_setservername(0, "pool.ntp.org");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_init();
}

esp_err_t m5rtc_init(void) {
//   Wire1.begin(21,22);

//   Wire1.beginTransmission(0x51);
//   Wire1.write(0x00);
//   Wire1.write(0x00);  // Status reset
//   Wire1.write(0x00);  // Status2 reset
//   Wire1.endTransmission();

    esp_err_t e;
    i2c_cmd_handle_t cmd;
    uint8_t buf[3] = { (BM8563_I2C_ADDR << 1) | I2C_MASTER_WRITE, 0, 0, 0 };

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    // i2c_master_write_byte(cmd, (BM8563_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);

    i2c_master_write(cmd, buf, 4, I2C_MASTER_NACK);

    i2c_master_stop(cmd);
    e = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "error sending init");
    }

    i2c_cmd_link_delete(cmd);

    rtcTimeNow.hours = 15;
    rtcTimeNow.minutes = 45;
    rtcTimeNow.seconds = 30;

    // setRTCTime(&rtcTimeNow);

    rtcTimeNow.hours = 0;
    rtcTimeNow.minutes = 0;
    rtcTimeNow.seconds = 0;

    getRTCTime(&rtcTimeNow);

    printf("M5 RTC Time Now: %d:%d:%d\n", rtcTimeNow.hours, rtcTimeNow.minutes, rtcTimeNow.seconds);

    return e;
}

esp_err_t getRTCTime(RTC_TimeTypeDef* rtcTimeStruct){
    
    uint8_t buf[3] = {0};

//     Wire1.beginTransmission(0x51);
//     Wire1.write(0x02);
//     Wire1.endTransmission();
//     Wire1.requestFrom(0x51,3); 

//     while(Wire1.available()){
    
//       buf[0] = Wire1.read();
//       buf[1] = Wire1.read();
//       buf[2] = Wire1.read();
     
//    }

    esp_err_t e;
    i2c_cmd_handle_t cmd;

    cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BM8563_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x02, true);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BM8563_I2C_ADDR << 1) | I2C_MASTER_READ, true);

    i2c_master_read(cmd, buf, 3, I2C_MASTER_LAST_NACK);

    i2c_master_stop(cmd);
    e = i2c_master_cmd_begin(I2C_NUM_0, cmd, 250/portTICK_PERIOD_MS);

    if (e != ESP_OK) {
        ESP_LOGE(TAG, "error sending getRTCTime");
    } else {
         ESP_LOGE(TAG, "success sending getRTCTime");       
    }
    i2c_cmd_link_delete(cmd);


    rtcTimeStruct->seconds  = bcd2ToByte(buf[0]&0x7f);
    rtcTimeStruct->minutes  = bcd2ToByte(buf[1]&0x7f);
    rtcTimeStruct->hours    = bcd2ToByte(buf[2]&0x3f);

    return e;
}

esp_err_t setRTCTime(RTC_TimeTypeDef* rtcTimeStruct){

    if(rtcTimeStruct == NULL) return;

    // Wire1.beginTransmission(0x51);
    // Wire1.write(0x02);
    // Wire1.write(ByteToBcd2(rtcTimeStruct->seconds)); 
    // Wire1.write(ByteToBcd2(rtcTimeStruct->minutes)); 
    // Wire1.write(ByteToBcd2(rtcTimeStruct->hours)); 
    // Wire1.endTransmission();

    esp_err_t e;
    i2c_cmd_handle_t cmd;

    uint8_t hours = byteToBcd2(rtcTimeStruct->hours);
    uint8_t minutes = byteToBcd2(rtcTimeStruct->minutes);
    uint8_t seconds = byteToBcd2(rtcTimeStruct->seconds);

    printf("byteToBcd2 for set is: %d%d:%d%d:%d%d\n", hours >> 4, hours & 0xf, minutes >> 4, minutes & 0xf, seconds >> 4, seconds & 0xf);

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);

    // i2c_master_write(cmd, buf, 5, I2C_MASTER_NACK);

    i2c_master_write_byte(cmd, (BM8563_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x02, true);
    i2c_master_write_byte(cmd, seconds, true);
    i2c_master_write_byte(cmd, minutes, true);
    i2c_master_write_byte(cmd, hours, true);

    i2c_master_stop(cmd);
    e = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "error sending setRTCTime");
    }
    i2c_cmd_link_delete(cmd);

    return e;
}

esp_err_t getBM8563Time(void) {
//   Wire1.beginTransmission(0x51);
//   Wire1.write(0x02);
//   Wire1.endTransmission();
//   Wire1.requestFrom(0x51,7); 



//   while(Wire1.available()){
    
//       trdata[0] = Wire1.read();
//       trdata[1] = Wire1.read();
//       trdata[2] = Wire1.read();
//       trdata[3] = Wire1.read();
//       trdata[4] = Wire1.read();
//       trdata[5] = Wire1.read();
//       trdata[6] = Wire1.read();
     
//   }

//   DataMask();
//   Bcd2asc();
//   Str2Time();

    esp_err_t e;
    i2c_cmd_handle_t cmd;

    cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BM8563_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x07, true);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BM8563_I2C_ADDR << 1) | I2C_MASTER_READ, true);

    i2c_master_read(cmd, rtc_data, 7, I2C_MASTER_LAST_NACK);

    i2c_master_stop(cmd);
    e = i2c_master_cmd_begin(I2C_NUM_0, cmd, 250/portTICK_PERIOD_MS);

    if (e != ESP_OK) {
        ESP_LOGE(TAG, "error sending getBM8563Time");
    } else {
        ESP_LOGE(TAG, "success sending getBM8563Time");
        maskRTCData();
        bcd2ascii();
    }
    i2c_cmd_link_delete(cmd);

    return e;
}

/*
 * private BCD <-> Byte fuunctions
 */

static uint8_t bcd2ToByte(uint8_t value)
{
  uint8_t tmp = 0;
  tmp = ((uint8_t)(value & (uint8_t)0xF0) >> (uint8_t)0x4) * 10;
  return (tmp + (value & (uint8_t)0x0F));
}

static uint8_t byteToBcd2(uint8_t val)
{
  uint8_t bcdhigh = 0;
  uint8_t value = val;

  while (value >= 10)
  {
    bcdhigh++;
    value -= 10;
  }
  
  return  ((uint8_t)(bcdhigh << 4) | value);
}

static void maskRTCData()
{
  
  rtc_data[0] = rtc_data[0]&0x7f; 
  rtc_data[1] = rtc_data[1]&0x7f;
  rtc_data[2] = rtc_data[2]&0x3f;
  
  rtc_data[3] = rtc_data[3]&0x3f;
  rtc_data[4] = rtc_data[4]&0x07;
  rtc_data[5] = rtc_data[5]&0x1f;
  
  rtc_data[6] = rtc_data[6]&0xff;
}

static void bcd2ascii(void)
{
  uint8_t i,j;
  for (j=0,i=0; i<7; i++){
    asc[j++] =(rtc_data[i]&0xf0)>>4|0x30 ;
    asc[j++] =(rtc_data[i]&0x0f)|0x30;
  }
}
