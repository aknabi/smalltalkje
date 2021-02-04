/*
 * Comment out the following to build image
 * (Goto smalltalkImage directory and run "make")
 * If uncommented will build for ESP32 
*/
#define TARGET_ESP32

/*
 * Uncomment out the following to build ESP32 image
 * that will simply write the object data file to a
 * data partition and stop
*/
// #define WRITE_OBJECT_PARTITION


// Define device as ESP32 dev board with a SSD1306 I2C OLED
#define DEVICE_ESP32_SSD1306 1
// Define device as an M5StickC
#define DEVICE_M5STICKC 2

#define TARGET_DEVICE DEVICE_M5STICKC

// M5StickC defines
// #define TEST_M5STICK

