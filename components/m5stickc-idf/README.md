# M5StickC-IDF

ESP-IDF component to work with M5StickC. Works without including the Arduino framework.

---

## Current features:

* Initializes power management IC AXP192
  * Provides easy function calls to read/write its registers
* Initializes display ST7735
* Initializes push buttons
  * "Click" & "hold" events available
* Set display backlight level
  * Option to turn it completely off
* Display API thanks to Loboris ESP32_TFT_library
  * Added support for custom fonts without modifying its code

## Usage

Check out the [example project](https://github.com/pablobacho/m5stickc-idf-example).

Clone repository in your `components` folder in your project and include `m5stickc.h` in your source code.

Call `m5_init()` to initialize your M5StickC. This initializes the AXP192 power manager chip, the display and the push buttons.

This component creates a custom event loop called `m5_event_loop` for device events. User program can subscribe to it and listen to events such as "button click" and "button hold". More actions coming. Check out .h files for specific module events.

Use `m5display_set_backlight_level(uint8_t backlight_level)` to set the backlight level on the display. Backlight range between 0 and 7. To turn on or off the backlight, use `m5display_on()` and `m5display_off()`.

## Display driver based on Loboris ESP32_TFT_library

Includes a modified version of [ESP32_TFT_library](https://github.com/loboris/ESP32_TFT_library) by Boris Lovosevic. Check his GitHub for help on how to use the display.

One of the modifications enables support for custom embedded fonts. After converting the font file to an embedded array using the tools provided by Loboris, use:

    TFT_setFont(CUSTOM_EMBEDDED_FONT, (char *) opensans_semibold_12); // opensans_semibold_12 is my custom embedded font.

## Contributing

If you find a bug, improve documentation, add a feature, or anything else, I encourage you to open an issue and/or make a pull request.

When contributing, please follow [Espressif IoT Development Framework Style Guide](https://docs.espressif.com/projects/esp-idf/en/latest/contribute/style-guide.html) and their [Documenting Code Guide](https://docs.espressif.com/projects/esp-idf/en/latest/contribute/documenting-code.html)

Licensed under the MIT License.