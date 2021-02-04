# ESP32-smalltalk
Little Smalltalk for ESP32 with SSD1306 OLED and M5StickC support

This is a very early project. Right now builds in VisualStudio Code wiht PlatformIO plugin the and runs (on the Mac at least).

Publishing so that folks can take a look. 

It's written in C (no C++) for the esp-idf (not arduino)... this was done to keep it as lean and mean as possible (as Smalltalk needs the memory).

A ESP32 or image building target can be built (the image builder is in a directory smalltalkImage)... building this compiles the Smalltalk source files to create the objectTable, objectData and systemImage files.

A SPIFFS partition is created with an exported files uploaded after the image is built. 

The objectTable file is the export of the built image object table... on the ESP32 these entries are loaded into RAM on init

The objectData file is the export of the object data (pointed to by the object table)... on the ESP32 these entries are either loaded into RAM on init, or optionally ByteArrays, Symbols and Strings are copied to a FLASH addressable memory partition and the object table points to those freeing up RAM. RAM vs. Flash is chosen via a #define and copying the objectData file to the addressable Flash requires an extra step... more details to follow.

The systemImage file is the "standard" Smalltalk image file we're all used to. Via a #define this can be loaded into RAM and run like a normal Smalltalk system... this was the safe way to run while getting the above files working and keeping for a while... writing an image is supported (but not testing and supported on SPIFFS yet)... we'll see where that goes.

However, don't start playing with the code yet... getting some more basic graphics and command line sorted then
will document with more details and clear guides.

Will also link to the various other bits of code used for reference (the info is in the copyright files in the source for now).

An OLED display class has been added (very early work... all this will change)... is supports a I2C SSD1306 OLED display (the cheap ones you find on AliExpress or Adafruit)... you can configure the I2C pins the OLED is connected to on the ESP32 dev board.

There's also intial support for the M5StickC (and will add M5Atom/Lite, M5Stack support next)... these is actually the "real" targets for this project (the ESP32 dev board allowed be to use JTAG debugging, which the M5 doesn't have... but now getting to a point where it's not needed). I'll try to keep the ESP32 dev support going... will be doable for the core, but color displays and other features may just be stubbed on the ESP32 dev and left as a porting effort for anyone using other peripherals.

Again... this is just a commit after getting the M5Stick drawing basic graphics and things will be fast moving from here for a bit.
