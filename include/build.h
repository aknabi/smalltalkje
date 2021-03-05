/*

	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021

	build settings

	This include file defines settings for a specific build target

    This includes the platform, OS and target

    Over time these settings can/should/will migrate over to build flags
    (e.g. in make, CMake, XCode settings, etc)

*/

// Use this block for building on the Mac
// For calls to support threading, file init, etc use POSIX calls
//#define TARGET_POSIX

#ifdef TARGET_MAC

#define TARGET_BUILD_IMAGE

#else

/*
 * Comment out the following to build image
 * (Goto smalltalkImage directory and run "make")
 * If uncommented will build for ESP32 
 */

#define TARGET_ESP32


/* Can use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO

/*
 * Uncomment out the following to build ESP32 image
 * that will simply write the object data file to a
 * data partition and stop
 */
#define WRITE_OBJECT_PARTITION

// Define device as ESP32 dev board with a SSD1306 I2C OLED
#define DEVICE_ESP32_SSD1306 1
// Define device as an M5StickC
#define DEVICE_M5STICKC 2

#define TARGET_DEVICE DEVICE_M5STICKC

// M5StickC defines

#endif
