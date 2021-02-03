#
# Main Makefile. This is basically the same as a component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_SRCDIRS := . util util/font backported/esp_event backported/newlib
COMPONENT_ADD_INCLUDEDIRS := . include util

#
# Component Makefile
#

COMPONENT_ADD_INCLUDEDIRS := port/include aws-iot-device-sdk-embedded-C/include

COMPONENT_SRCDIRS := aws-iot-device-sdk-embedded-C/src port