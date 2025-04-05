# Smalltalkje

## An Embedded Smalltalk Implementation for ESP32 Devices

Smalltalkje is a lightweight Smalltalk implementation designed specifically for embedded systems, particularly the ESP32 microcontroller platform. It provides a complete Smalltalk environment that can run on resource-constrained devices, bringing object-oriented programming and interactive development to the IoT and embedded world.

**(Currently supporting ESP32 DevKit, SSD1306 OLED, M5StickC, Lilygo T-Wristband, and Mac for development)**

## Overview

Smalltalkje is based on Tim Budd's Little Smalltalk version 3 (Oregon State University, July 1988), but has been significantly modified and enhanced to operate effectively on embedded systems with limited memory and processing power. The implementation features:

- Memory-efficient object representation
- Split memory model with objects in both RAM and Flash
- Support for various ESP32-based development boards
- Optimized bytecode interpreter
- Seamless integration with ESP32 peripherals
- WiFi connectivity
- Display support for several screen types

## Architecture

The system is structured around several key components:

### Memory Manager
Manages Smalltalk objects in memory using a reference counting system. The implementation supports two memory modes:
- **Traditional Mode**: All objects loaded into RAM (like standard Smalltalk)
- **Split Memory Mode**: Frequently accessed objects in RAM, with immutable objects (Strings, Symbols, ByteArrays) stored in Flash memory

### Image System
The Smalltalk environment persists as an image that consists of:
- `objectTable`: Object metadata (class, size, etc.) loaded into RAM
- `objectData`: The actual object content, which can be stored in RAM or Flash
- `systemImage`: A traditional Smalltalk image file

### Bytecode Interpreter
The heart of the Smalltalk VM that executes the bytecode instructions. It includes:
- Method caching for performance
- Time-sliced execution
- Support for primitives (native C functions)
- Context management for method activation

### ESP32 Integration
Components that bridge the Smalltalk environment with ESP32 capabilities:
- WiFi connectivity
- Display drivers
- GPIO control
- Non-volatile storage (NVS)

## Hardware Support

Smalltalkje currently supports:
- ESP32 DevKit (with optional SSD1306 OLED displays)
- M5StickC
- Lilygo T-Wristband
- Mac OS (primarily for development and image creation)

Future support is planned for:
- M5Atom/Lite
- M5Stack
- Other ESP32 variants

## Build & Installation

### Prerequisites
- Visual Studio Code with PlatformIO plugin
- ESP-IDF (for ESP32 targets)
- Xcode (for Mac targets)

### Building for ESP32
1. Clone the repository
2. Open in VS Code with PlatformIO
3. Select the appropriate board target in platformio.ini
4. Build and upload to your device

The build process compiles the Smalltalk source files to create:
- `objectTable`
- `objectData`
- `systemImage`

These files are then uploaded to the SPIFFS partition on the ESP32.

### Building for Mac (Development)
The Mac build is primarily used for testing and image creation:
1. Open the Xcode project
2. Select the appropriate build target
3. Build and run

## Key Features and Components

### Memory Optimization
- ByteArrays, Symbols, and Strings can be stored in Flash memory to conserve RAM
- Object table designed for efficient access
- Reference counting for memory management

### ESP32 Integration
- WiFi connectivity API for network operations
- HTTP client capabilities
- Filesystem access using SPIFFS
- Non-volatile storage for persisting settings

### Display Support
- SSD1306 OLED display driver
- M5StickC built-in display support
- Graphics primitives for drawing

### Development Environment
- Interactive Smalltalk shell over UART
- Ability to file-in Smalltalk code
- Support for image saving and loading

## Implementation Notes

The system is written in C (not C++) for the ESP-IDF framework (not Arduino) to maximize performance and minimize resource usage. This approach is critical for running Smalltalk, which traditionally requires significant memory, on resource-constrained devices.

The ESP32's multiple cores and sufficient RAM make it possible to run a complete Smalltalk environment while still having resources available for interfacing with the physical world.

## Credits

- Based on Little Smalltalk v3 by Tim Budd
- M5StickC library for ESP-IDF from Pablo Bacho (https://github.com/pablobacho/m5stickc-idf-example)
- SSD1306 I2C OLED driver based on work by Limor Fried/Ladyada for Adafruit Industries
- Additional resources from the ESP-IDF and M5Stack repositories

## License

[Original license information should be preserved here]

## Documentation and Development Status

The Smalltalkje codebase is extensively documented to facilitate understanding and contributions:

- **Source Code Documentation**: Comprehensive comments throughout the code explain:
  - Memory management system and reference counting (memory.c)
  - Bytecode interpreter operation and instruction set (interp.c)
  - Object representation and manipulation (news.c)
  - Image loading/saving mechanisms and memory models (image.c)
  - Primitive operations that bridge Smalltalk and C (primitive.c)
  - ESP32-specific features and optimizations (sysprim.c)
  - Lexical analysis state machine and token processing (lex.c)
  - Split RAM/ROM memory model for resource optimization

- **Key Implementation Components**:
  - **Memory Manager**: Reference counting system with specialized handling for byte objects
  - **Interpreter**: Efficient bytecode execution with method cache and proper context handling
  - **Lexical Analyzer**: Robust state machine for tokenizing Smalltalk code with detailed documentation
  - **Parser**: Recursive descent parser with optimizations for common language constructs
  - **Image System**: Flexible image loading mechanisms supporting both RAM and ROM-based objects
  - **Object Creation**: Factory functions for creating and initializing Smalltalk objects
  - **Primitives**: Native functions exposing C capabilities to Smalltalk code

- **Header Files**: Clear interface documentation for all major subsystems
  - Object memory access and manipulation (memory.h)
  - Bytecode instruction set definitions (interp.h)
  - Network functionality (esp32wifi.h)
  - Lexical analysis and token types (lex.h)
  - Parser interfaces and method compilation (parser.h)

This project is under active development. While the core functionality is working, examples and tutorials are still being improved. The codebase is structured to allow for relatively easy addition of new device support and feature enhancements.

For more detailed documentation and getting started guides, please refer to the [Smalltalkje Wiki](https://github.com/aknabi/smalltalkje/wiki).
