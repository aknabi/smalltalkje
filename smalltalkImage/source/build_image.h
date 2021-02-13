/*
 * Comment out the following to build image
 * (Goto smalltalkImage directory and run "make")
 * If uncommented will build for ESP32 
*/
// #define TARGET_ESP32

// For calls to support threading, file init, etc use POSIX calls
#define TARGET_POSIX

#define TARGET_BUILD_IMAGE

/*
 * Uncomment out the following to build ESP32 image
 * that will simply write the object data file to a
 * data partition and stop
*/
// #define WRITE_OBJECT_PARTITION
