/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021

	Build Configuration Header
	
	This header defines build settings and configuration options for specific
	target platforms and devices. It controls which features and hardware support
	are enabled during compilation, allowing the same codebase to target multiple
	platforms and devices.
	
	The configuration options defined here include:
	1. Target platform (Mac, ESP32)
	2. Specific device types (M5StickC, ESP32 with SSD1306, etc.)
	3. Display types and drivers
	4. Peripheral support (keyboard, terminal)
	
	While many of these settings could be migrated to build system flags 
	(Make, CMake, XCode settings), they are centralized here for easier
	management and to provide a clear overview of supported configurations.
*/

#ifndef __BUILD_H__
#define __BUILD_H__

/**
 * Component Dependencies
 * 
 * Note that this header is also imported by some project components,
 * such as m5button.h. Any changes here may affect those components.
 */
// TODO: build.h is also imported by some project components... for example search for m5button.h

/**
 * Desktop Platform Settings
 * 
 * These settings are used when building for desktop platforms,
 * particularly macOS. They enable features like image building
 * and POSIX-compatible APIs.
 * 
 * Uncomment TARGET_POSIX to use POSIX calls for threading, file I/O, etc.
 */
//#define TARGET_POSIX

/**
 * macOS Build Configuration
 * 
 * When targeting macOS, we enable image building mode and set
 * the platform name accordingly. macOS builds are primarily used
 * for development and for building Smalltalk images that will be
 * deployed to embedded devices.
 */
#ifdef TARGET_MAC

#define TARGET_BUILD_IMAGE      /* Enable image building mode */
#define PLATFORM_NAME_STRING "MACOS"

#else /* ESP32 and other embedded targets */

/**
 * ESP32 Build Configuration
 * 
 * These settings control the build for ESP32 targets. To build an image instead,
 * comment out TARGET_ESP32 and run "make" in the smalltalkImage directory.
 */
#define TARGET_ESP32    /* Enable ESP32 target */

/**
 * GPIO Configuration
 * 
 * Configures which GPIO pin to use for the blink example.
 * This can be set via menuconfig (idf.py menuconfig) or directly here.
 */
#define BLINK_GPIO CONFIG_BLINK_GPIO

/**
 * Filesystem Operation Mode
 * 
 * When defined, this setting creates an ESP32 build that simply writes
 * the object data file to a data partition and stops, rather than running
 * the full Smalltalk environment. This is useful for deploying a new
 * image to the device.
 */
#define WRITE_OBJECT_PARTITION

/**
 * Supported Device Types
 * 
 * These constants define the different ESP32-based devices that Smalltalkje supports.
 * Each device has different hardware capabilities (display, buttons, sensors) that
 * the system can utilize.
 */
#define DEVICE_ESP32_SSD1306 1  /* Standard ESP32 dev board with SSD1306 I2C OLED display */
#define DEVICE_M5STICKC 2       /* M5StickC compact ESP32 with built-in display and buttons */
// #define DEVICE_M5STICKC_PLUS  /* Uncomment for M5StickC Plus variant */
#define DEVICE_M5SATOM_LITE 3   /* M5Atom Lite ESP32 with built-in button and LED matrix */
#define DEVICE_T_WRISTBAND 4    /* Lilygo T-Wristband with ST7735 160x80 display */

/**
 * Current Target Device
 * 
 * This setting determines which device will be targeted for the current build.
 * Change this value to build for a different device.
 */
#define TARGET_DEVICE DEVICE_M5STICKC

#endif /* TARGET_MAC */

/**
 * Device-Specific Configuration
 * 
 * These settings configure platform-specific details based on the selected
 * target device. Each device has a unique platform name string and may have
 * additional configuration settings like display type.
 */
#if TARGET_DEVICE == DEVICE_M5STICKC
#define PLATFORM_NAME_STRING "M5StickC"     /* Human-readable platform name */
#define DEVICE_DISPLAY_TYPE "ST7789V"       /* Display controller type */
#elif TARGET_DEVICE == DEVICE_M5SATOM_LITE
#define PLATFORM_NAME_STRING "M5AtomLite"   /* Human-readable platform name */
#elif TARGET_DEVICE == DEVICE_ESP32_SSD1306
#define PLATFORM_NAME_STRING "ESP32-1306"   /* Human-readable platform name */
#define DEVICE_DISPLAY_TYPE "SSD1306"       /* Display controller type */
#elif TARGET_DEVICE == DEVICE_T_WRISTBAND
#define PLATFORM_NAME_STRING "T-WRBD"       /* Human-readable platform name */
#define DEVICE_DISPLAY_TYPE "SSD1306"       /* Display controller type */
#else
#define PLATFORM_NAME_STRING "XXXX"         /* Unknown platform */
#endif

/**
 * Peripheral and Feature Support
 * 
 * These settings enable support for various peripherals and features
 * that can be used by Smalltalkje on supported devices.
 */

/* Enable support for M5Stack CardKB mini keyboard */
#define CARD_KB_SUPPORTED

/* Enable terminal emulation using keyboard input and display output */
#define DEVICE_TERMINAL_SUPPORTED

#endif /* __BUILD_H__ */
