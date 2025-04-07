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

## System Architecture

Smalltalkje follows a modular architecture with several interacting subsystems that collectively provide a complete Smalltalk environment. The system divides into distinct layers, each with specific responsibilities.

### High-Level Architecture

```
+---------------------------------+
|         Smalltalk Image         |
| (Objects, Classes, Methods, etc)|
+---------------------------------+
|       Virtual Machine (VM)      |
| +-----------------------------+ |
| |     Bytecode Interpreter    | |
| +-----------------------------+ |
| |      Memory Management      | |
| +-----------------------------+ |
| |      Primitive Operations   | |
| +-----------------------------+ |
+---------------------------------+
|         Platform Support        |
| (ESP32, Mac, I/O, WiFi, etc)    |
+---------------------------------+
```

### Key Architectural Components

#### 1. Memory Management System

The memory system in Smalltalkje is designed to operate efficiently on memory-constrained devices:

- **Object Model**: Objects are referenced via indexes into an object table that holds metadata
- **Reference Counting**: Simple, efficient memory reclamation using reference counting
- **Split Memory Model**: Unique design that allows storing immutable objects in Flash memory while keeping mutable objects in RAM
- **Efficient Allocation**: Multi-strategy object allocation with size-specific free lists to minimize fragmentation
- **Byte Objects**: Special handling for strings, byte arrays, and other binary data

#### 2. Image System

The image system provides object persistence and efficient loading:

- **Smart Loading**: Can load objects selectively into RAM or leave them in Flash
- **Image Formats**: Supports both traditional (all RAM) and split (RAM/Flash) image formats
- **Object Classification**: Automatically identifies which objects can safely remain in Flash
- **Memory Optimization**: Significantly reduces RAM usage by only copying mutable objects from Flash

#### 3. Virtual Machine Core

The VM is the heart of the system, interpreting Smalltalk bytecodes and managing execution:

- **Bytecode Interpreter**: Executes compiled Smalltalk methods with highly optimized instruction set
- **Process Management**: Time-sliced cooperative multitasking for Smalltalk processes
- **Method Cache**: Performance optimization for method lookup
- **Context Management**: Handles method activation records and returns
- **Primitives**: Native C implementations of performance-critical operations

#### 4. Compiler and Parser

The compilation system converts Smalltalk code to bytecodes:

- **Lexical Analysis**: Tokenization with state machine-based scanning
- **Recursive Descent Parser**: Parses method syntax and generates bytecodes
- **Method Compilation**: Optimizes common control structures and message patterns
- **Block Closure Support**: First-class functions with lexical scope

#### 5. Platform Integration

Smalltalkje integrates closely with the underlying platform:

- **ESP32 Features**: Direct access to WiFi, HTTP, GPIO, display drivers
- **Interrupt Handling**: Bridge between ESP32 events and Smalltalk processes
- **Non-Volatile Storage**: Persistence using ESP32's NVS system
- **I/O Subsystem**: TTY, file system, and communication interfaces

## Virtual Machine Architecture

The Smalltalkje virtual machine implements a complete Smalltalk environment with specific optimizations for embedded systems. The VM architecture balances the trade-offs between memory usage, execution speed, and flexibility while maintaining the dynamic nature of Smalltalk.

### Object Representation

All entities in Smalltalkje are objects with a uniform representation, providing a consistent foundation for the entire system:

- **Object Table**: Central registry of all objects with metadata
  - **What it does**:
    - Acts as the central registry for all objects in the system
    - Provides a level of indirection that simplifies memory management
    - Allows objects to be moved in memory without changing references
    - Maintains critical metadata for each object
    - Enables the split memory model by tracking object location
  - **Components**:
    - **Index**: Object reference divided by 2, serving as the unique identifier
    - **Class reference**: Points to the class object defining the object's behavior
    - **Size**: Number of instance variables or bytes, with negative values indicating byte objects
    - **Reference count**: Tracks how many references exist to this object
    - **Memory pointer**: Points to instance variables or byte data in RAM or Flash

- **Special Object Types**:
  - **Regular Objects**:
    - Store references to other objects as instance variables
    - Can mutate state by changing instance variable values
    - Reside in RAM to allow modification
    - Form the backbone of most Smalltalk programs
  - **Byte Objects**:
    - Store raw bytes instead of object references
    - Represented with negative size values in the object table
    - Used for strings, byte arrays, and other binary data
    - Memory-efficient for text and binary storage
    - Access optimized for byte-level operations
  - **Small Integers**:
    - Encoded directly in object references using tagged pointers
    - No object table entry needed, saving memory
    - Value stored in the upper 31 bits of the reference
    - Lowest bit set to 1 to distinguish from regular object references
    - Supports immediate arithmetic without object allocation
  - **ROM Objects**:
    - Special objects stored in Flash memory
    - Immutable by design (any mutation attempt causes an error)
    - Marked with maximum reference count (0x7F) to prevent collection
    - Include strings, symbols, byte arrays, and code blocks
    - Critical for reducing RAM usage on memory-constrained devices

### Bytecode Instruction Set

The VM executes compact bytecode instructions, designed specifically for efficient Smalltalk execution on embedded systems:

- **Instruction Format**:
  - **What it does**:
    - Provides a compact encoding of operations
    - Reduces memory footprint of compiled methods
    - Enables efficient instruction decoding
    - Balances code density with execution speed
  - **Components**:
    - **High nibble** (4 bits): Opcode defining the operation
    - **Low nibble** (4 bits): Operand or parameter for the operation
    - Single-byte encoding for most common operations

- **Extended Format**:
  - **What it does**:
    - Handles operations with operands larger than 15
    - Maintains instruction set consistency
    - Provides flexibility for complex operations
  - **Components**:
    - Extended opcode in the first byte
    - Additional byte(s) for larger operand values
    - Special handling by the interpreter

- **Core Instructions**:
  - **Variable Access/Store**:
    - Push/retrieve instance variables (object state)
    - Push/store temporary variables (method-local)
    - Push/store arguments (parameter passing)
    - Push literals (constants embedded in methods)
    - Direct access to self (the receiver object)
    - Special handling for super sends
  - **Message Sending**:
    - Unary messages (no arguments): `object message`
    - Binary messages (one argument): `object + argument`
    - Keyword messages (multiple arguments): `object at: index put: value`
    - Special send bytecodes for common operations
    - Dynamic binding through method lookup
  - **Control Flow**:
    - Conditional branches (for if/else/while)
    - Unconditional jumps (for optimization)
    - Method returns (normal, block, and non-local)
    - Block creation and activation
  - **Primitive Operations**:
    - Direct calls to C-implemented operations
    - Fast paths for arithmetic, comparison, and I/O
    - Escape hatch for performance-critical code
    - Interface to platform-specific functionality

### Message Passing System

Message passing is the primary mechanism for computation, implementing Smalltalk's pure object-oriented paradigm:

- **Method Lookup**:
  - **What it does**:
    - Dynamically resolves which method to execute for a message
    - Implements Smalltalk's inheritance model
    - Provides the foundation for polymorphism
    - Handles method not found errors
  - **Process**:
    - Starts search in the receiver's class
    - Traverses the inheritance chain upward
    - Returns the first matching method found
    - Invokes doesNotUnderstand: if no method is found
    - Results cached for performance

- **Method Cache**:
  - **What it does**:
    - Dramatically improves performance of repeated message sends
    - Avoids costly method lookup in the class hierarchy
    - Balances memory usage with lookup speed
  - **Components**:
    - 211-entry hash table (prime number size)
    - Hash based on message selector and receiver class
    - Each entry stores selector, receiver class, method class, and method
    - Invalidated when methods are modified or classes reorganized

- **Optimized Messages**:
  - **What it does**:
    - Provides fast paths for common operations
    - Reduces overhead for frequently used messages
    - Maintains semantics while improving performance
  - **Implementations**:
    - Special bytecodes for common unary messages (isNil, notNil)
    - Dedicated handling for arithmetic operations
    - Inlined implementation of simple collection access
    - Short-circuit evaluation for boolean operations

- **Primitive Messages**:
  - **What it does**:
    - Bridges Smalltalk with native C implementation
    - Provides essential functionality impossible in pure Smalltalk
    - Optimizes performance-critical operations
    - Interfaces with hardware and platform features
  - **Characteristics**:
    - Numbered primitives for basic operations
    - Class-specific primitives for specialized behavior
    - I/O and device control primitives
    - Fallback to Smalltalk code if primitive fails
    - Error reporting mechanism to Smalltalk

### Memory Management

The memory manager employs several strategies for efficiency, crucial for operating in resource-constrained environments:

- **Reference Counting**:
  - **What it does**:
    - Tracks the number of references to each object
    - Immediately reclaims memory when no references remain
    - Avoids the need for garbage collection pauses
    - Provides deterministic resource cleanup
  - **Operations**:
    - Incremented when object reference is assigned
    - Decremented when reference is overwritten
    - Objects freed when count reaches zero
    - Recursively processes instance variables
    - Special handling for circularities

- **Free Lists**:
  - **What it does**:
    - Organizes reclaimed objects by size for efficient reuse
    - Minimizes memory fragmentation
    - Reduces allocation overhead
    - Optimizes memory utilization
  - **Implementation**:
    - Separate list for each object size (up to 2048 lists)
    - Free objects linked through their first instance variable
    - Quick lookup by exact size
    - Fallback strategies for size mismatches
    - Periodically compacted for efficiency

- **Block Allocation**:
  - **What it does**:
    - Amortizes allocation overhead across multiple objects
    - Reduces memory fragmentation
    - Improves allocation speed
    - Manages memory more efficiently
  - **Strategy**:
    - Allocates memory in blocks of 2048 objects
    - Distributes new objects from these blocks
    - Maintains allocation statistics
    - Reuses memory from reclaimed objects
    - Handles out-of-memory conditions gracefully

- **ROM/RAM Split**:
  - **What it does**:
    - Dramatically reduces RAM usage on embedded devices
    - Leverages Flash memory for immutable objects
    - Preserves RAM for mutable state
    - Enables larger programs on constrained devices
  - **Implementation**:
    - Immutable objects (strings, symbols, code) stored in ROM
    - Only mutable objects copied to RAM during image load
    - Object table records whether object is in ROM or RAM
    - Write-protection for ROM objects
    - Automatic classification during image creation

### Context and Process Model

The VM implements a lightweight concurrency model that enables multitasking while maintaining simplicity:

- **Process Objects**:
  - **What it does**:
    - Represents independent threads of execution
    - Enables concurrent programming model
    - Maintains execution state between time slices
    - Supports priority-based scheduling
  - **Components**:
    - Priority level (determines scheduling order)
    - Link to active context (current execution point)
    - State information (running, waiting, suspended)
    - Next/previous process links (for scheduler queue)
    - Semaphore waiting links (for synchronization)

- **Context Objects**:
  - **What it does**:
    - Stores method execution state
    - Implements the call stack
    - Provides lexical scoping for variables
    - Enables method returns and continuations
  - **Components**:
    - Method reference (which method is executing)
    - Sender context (for call chain and returns)
    - Instruction pointer (current execution point)
    - Stack pointer (top of evaluation stack)
    - Arguments array (method parameters)
    - Temporary variables array (method-local state)
    - Evaluation stack (for expression calculation)

- **Stack-based Execution**:
  - **What it does**:
    - Provides a workspace for calculating expressions
    - Passes values between operations
    - Simplifies bytecode implementation
    - Models Smalltalk expression evaluation
  - **Operations**:
    - Push values onto stack (literals, variables)
    - Pop values for storage or as operation arguments
    - Duplicate or swap stack elements
    - Clear stack entries
    - Build arrays from stack elements
    - Return stack top as method result

- **Time-sliced Execution**:
  - **What it does**:
    - Enables cooperative multitasking
    - Provides fair execution time to all processes
    - Maintains system responsiveness
    - Simulates concurrent execution
  - **Implementation**:
    - Each process gets a fixed bytecode execution quota
    - Bytecode counter decremented with each instruction
    - Process yields when counter reaches zero
    - Scheduler selects next process based on priority
    - I/O operations and waits can yield immediately
    - External interrupts can preempt the current process

## Code Organization

The Smalltalkje codebase is organized into logical modules, each handling a specific aspect of the system. This modular design provides clear separation of concerns and makes the system easier to understand and maintain.

### Core Subsystems

#### Memory Management (`memory.c`)

The memory management subsystem is the foundation of Smalltalkje, responsible for object allocation, tracking, and reclamation through reference counting:

- `initMemoryManager()`: Initializes the memory system by setting up the object table, clearing free list pointers, zeroing reference counts, and building initial free lists. This must be called before any other memory operations can be performed.

- `allocObject()`: Implements a sophisticated multi-strategy allocation algorithm for efficient object creation:
  1. First attempts to find an exact-sized object in the free list (fastest path)
  2. Tries to repurpose a size-0 object by expanding it to the needed size
  3. Finds a larger object and uses it (potentially wasting some space)
  4. Locates a smaller object, frees its memory, and resizes it
  This approach maximizes memory reuse while minimizing fragmentation.

- `allocByte()`: Creates specialized objects for storing raw bytes (like strings and byte arrays) rather than object references. These have negative size fields to indicate their special nature.

- `sysDecr()`: The core of the reference counting system that reclaims objects when their reference count reaches zero. It:
  1. Validates the reference count isn't negative (error detection)
  2. Decrements the reference count of the object's class
  3. Adds the object to the appropriate free list
  4. Recursively decrements the reference count of all instance variables
  5. Clears all instance variables to prevent dangling references

- `visit()`: Rebuilds reference counts during image loading by implementing a depth-first traversal of the object graph. This ensures only reachable objects are retained in memory.

- `mBlockAlloc()`: Manages memory allocation in large blocks (2048 objects at a time) rather than individual malloc calls. This amortizes allocation overhead across many objects and reduces memory fragmentation.

- Memory optimization through free lists: Reclaimed objects are organized by size in free lists, allowing fast reuse without new memory allocation. The system uses up to 2048 distinct size buckets for precise matching.

- Special ESP32 support for split memory model: Objects can reside in both RAM and Flash (ROM) memory, with immutable objects kept in Flash to save precious RAM.

#### Interpreter (`interp.c`)

The interpreter is the heart of the Smalltalk VM, executing bytecodes and implementing the dynamic message dispatch system:

- `execute()`: The main bytecode execution loop, which operates as a large state machine processing one bytecode at a time. It:
  1. Extracts execution state from the process object (stack, context, etc.)
  2. Runs a loop decoding and executing bytecodes until the time slice ends
  3. Handles message sends, primitive calls, returns, and control flow
  4. Implements time-sliced cooperative multitasking via bytecode counting
  5. Saves execution state back to the process when yielding

- `findMethod()`: Implements Smalltalk's inheritance-based method lookup by searching for a matching method starting from a class and proceeding up the inheritance hierarchy. This is the core of dynamic dispatch.

- `flushCache()`: Invalidates method cache entries when methods have been recompiled or modified, ensuring the cached version is no longer used.

- Method cache optimization: The VM uses a 211-entry cache to significantly improve performance by avoiding repeated method lookups. Each cache entry stores:
  - The message selector being sent
  - The class of the receiver
  - The class where the method was found
  - The actual method object
  
- Bytecode execution optimizations:
  - Special handling for common unary messages (like isNil, notNil)
  - Fast paths for binary operations on primitive types
  - Dedicated implementation of frequently used primitives
  - Optimized stack and context management

- Interrupt handling: The interpreter can be interrupted by external events (timers, I/O completions) via the `interruptInterpreter()` function, which is crucial for ESP32 asynchronous events.

- Process model: The interpreter implements a lightweight concurrency model with Smalltalk Process objects representing independent threads of execution, and Context objects storing method execution state.

#### Image Management (`image.c`)

The image management subsystem handles object persistence and the unique split memory model of Smalltalkje:

- `imageRead()`: Loads a complete traditional Smalltalk image where all objects are placed in RAM. It:
  1. Reads the symbols table reference (root object)
  2. Processes each object's metadata (index, class, size)
  3. Allocates memory for and loads each object's data
  4. Restores reference counts and rebuilds free lists

- `readTableWithObjects()`: Implements the memory-optimized split approach where:
  1. All object table entries are loaded into RAM
  2. Immutable objects (ByteArray, String, Symbol, Block) point directly to Flash memory
  3. Mutable objects are copied into RAM
  This critical optimization significantly reduces RAM usage on ESP32 devices by keeping large portions of the image in Flash.

- `writeObjectTable()`: Saves only the metadata for objects (index, class, size, flags) to a file. It identifies and marks ROM-eligible objects (immutable types) via flags, enabling the split memory optimization when the image is later loaded.

- `writeObjectData()`: Writes the actual content of all objects to a separate file. In the split memory approach, this data can be embedded in Flash memory and accessed directly for immutable objects.

- `imageWrite()`: Creates a traditional combined image with both metadata and object data in a single file, preserved for compatibility with systems that don't need memory optimizations.

- Class-based ROM eligibility: The system identifies which objects can safely remain in Flash based on their class:
  - ByteArray (class 18)
  - String (class 34)
  - Symbol (class 8)
  - Block (class 182)
  These objects have their reference count set to maximum (0x7F) to prevent garbage collection, and their memory pointers point directly to Flash.

#### Parser (`parser.c`)

The parser translates Smalltalk source code into bytecodes, implementing a complete compiler for method definitions:

- `parse()`: The main entry point that coordinates the entire parsing process from method selector to method body. It populates a Method object with:
  - Message selector (method name)
  - Bytecodes (executable instructions)
  - Literals (constants used in the method)
  - Stack and temporary variable size information
  - Optionally the source text for debugging

- Recursive descent parsing: The parser implements Smalltalk grammar through a set of mutually recursive functions, each handling specific language constructs:
  - `messagePattern()`: Parses method selectors (unary, binary, keyword)
  - `temporaries()`: Handles temporary variable declarations
  - `body()`: Processes the sequence of statements in a method
  - `statement()`: Handles individual statements including returns
  - `expression()`: Parses complex expressions including assignments
  - `term()`: Processes basic expression elements (variables, literals, blocks)
  - `block()`: Handles block closure syntax and semantics

- Control flow optimizations: The parser intelligently transforms common control structures into efficient bytecode sequences:
  - `ifTrue:`/`ifFalse:` become conditional branch instructions
  - `whileTrue:` becomes an optimized loop structure
  - `and:`/`or:` implement short-circuit evaluation
  - These optimizations avoid the overhead of message sends for these common patterns

- Variable scoping: The parser correctly resolves variable references across different scopes:
  - `self`/`super` (receiver references)
  - Temporary variables (method-local)
  - Method arguments
  - Instance variables (object state)
  - Global variables (looked up at runtime)

- Bytecode generation: The parser produces compact bytecode instructions through:
  - `genInstruction()`: Creates instructions with high nibble (opcode) and low nibble (operand)
  - `genLiteral()`: Manages the literal table for constants used in methods
  - `genCode()`: Appends raw bytecodes to the instruction stream

- Block closure handling: The parser implements Smalltalk's powerful block closures (anonymous functions with lexical scope) by:
  1. Creating a Block object with metadata about arguments
  2. Generating code to invoke the block creation primitive at runtime
  3. Compiling the block body inline but skipping it during normal execution
  4. Handling special block return semantics

#### Lexical Analysis (`lex.c`)

The lexical analyzer tokenizes Smalltalk source code, converting raw text into a stream of tokens for the parser to process:

- State machine architecture: The lexer implements a finite state machine with different states for:
  - Regular token recognition (identifiers, keywords, operators)
  - String literal processing (with escape sequences)
  - Numeric literal parsing (integers and floating point)
  - Comment handling (single and multi-line)
  - Special character processing (brackets, parentheses, etc.)

- Token types: The lexer identifies and categorizes different elements of Smalltalk syntax:
  - `nameconst`: Identifiers like variable names
  - `namecolon`: Keywords in method selectors (ending with colon)
  - `binary`: Binary operators and other special characters
  - `intconst`: Integer literals
  - `floatconst`: Floating-point literals
  - `charconst`: Character literals
  - `symconst`: Symbol literals (starting with #)
  - `strconst`: String literals (in single quotes)
  - `arraybegin`: Start of literal arrays (#( )
  - Miscellaneous tokens for punctuation and syntax elements

- Advanced features:
  - Look-ahead capability for multi-character operators and tokens
  - Proper handling of nested comments
  - Support for various numeric formats (decimal, hex, scientific notation)
  - Proper string escape sequence processing
  - Error detection for malformed tokens

- The lexer maintains state between calls to provide a continuous token stream to the parser, with functions like:
  - `nextToken()`: Advances to and returns the next token
  - `lexinit()`: Initializes the lexer with source text
  - `peek()`: Examines the next character without consuming it
  - Error reporting with precise location information

### Platform Support

#### ESP32 Integration

The ESP32 integration layer bridges the Smalltalk environment with the powerful capabilities of the ESP32 microcontroller:

- `esp32wifi.c`: Implements complete WiFi networking functionality for Smalltalk:
  - Network scanning, connection, and management
  - IP address and network status handling
  - Event-driven connectivity with callbacks to Smalltalk methods
  - Support for both station and access point modes
  - Integration with FreeRTOS task management and event handling
  
- `esp32http.c`: Provides HTTP client capabilities that allow Smalltalk programs to:
  - Make GET, POST, PUT, and DELETE requests to web servers
  - Handle headers, request bodies, and response parsing
  - Process chunked transfer encoding and redirects
  - Implement REST API clients and web service integrations
  - Support both blocking and non-blocking request patterns

- `esp32nvs.c`: Implements a non-volatile storage interface that enables:
  - Persistent storage of Smalltalk objects between reboots
  - Key-value style storage in the ESP32's NVS flash partition
  - Different namespaces for organizing related data
  - Storage of strings, integers, and binary data from Smalltalk
  - Recovery of application state after power loss

- `esp32io.c`: Provides general-purpose I/O control, exposing capabilities like:
  - GPIO pin manipulation (digital read/write, PWM output)
  - Analog-to-digital conversion for sensor readings
  - Hardware timers and interrupts with Smalltalk callbacks
  - Low-level peripheral control (I2C, SPI, UART configuration)
  - Power management and deep sleep functionality

#### Display Support

Smalltalkje includes sophisticated display support for several screen types, enabling graphical user interfaces:

- SSD1306 OLED driver integration:
  - Full control of monochrome 128x64 and 128x32 OLED displays
  - Pixel-level drawing operations and text rendering
  - Buffer-based graphics with efficient partial updates
  - I2C communication with configurable address and pins
  - Low-level display commands for advanced control

- M5StickC display support:
  - Complete integration with M5StickC's color LCD
  - Hardware-accelerated drawing via ESP32's SPI capabilities
  - Font rendering with multiple built-in typefaces
  - Sprite and bitmap handling for fluid animations
  - Touch input integration for interactive applications
  - Power-efficient display management

- Graphics primitives for drawing:
  - Lines, rectangles, circles, and polygons
  - Bitmap rendering and scaling
  - Text with multiple font options
  - Compositing operations (XOR, OR, AND modes)
  - Screen buffering for flicker-free updates
  - Coordinate transformations and clipping

#### I/O Subsystem

The I/O subsystem provides a unified interface for input/output operations across different platforms:

- `tty.c`: Terminal input/output functionality:
  - Character and line-based terminal interaction
  - ANSI/VT100 terminal control sequences
  - Keyboard input handling with buffering
  - Special key detection (arrows, function keys)
  - Virtual terminal support for headless operation

- `uart.c`: Serial communication facilities:
  - Configurable baud rate, data bits, parity, and stop bits
  - Hardware flow control options (RTS/CTS)
  - Interrupt-driven I/O with buffer management
  - Multiple UART port support
  - Binary and text transfer modes
  - Integration with Smalltalk streams architecture

- `unixio.c`: File system operations that work across platforms:
  - File opening, reading, writing, and closing
  - Directory creation, listing, and navigation
  - File attribute handling (timestamps, permissions)
  - Cross-platform path normalization
  - Stream-based file access for Smalltalk
  - Error handling with detailed reporting

### Development Tools

Smalltalkje includes several powerful tools to aid in development and interaction:

- `filein.c`: Loads and executes Smalltalk code from files:
  - Parses and compiles method definitions
  - Handles class and instance method declarations
  - Processes class definitions with inheritance
  - Supports incremental loading of code
  - Error reporting with line and position information
  - Integration with the image system for persistence

- `st.c`: The interactive Smalltalk shell (REPL):
  - Read-Eval-Print Loop for direct code execution
  - Command history with editing capabilities
  - Workspace-like environment for testing code
  - Object inspection and exploration
  - Syntax highlighting and indentation
  - Integration with the debugger for error handling
  - Support for loading and executing script files

## Memory Optimization Details

Smalltalkje employs several innovative techniques to minimize RAM usage on embedded devices.

### Split Memory Model

The most significant memory optimization is the split memory approach:

1. Object Table: Always loaded into RAM for fast access to metadata
2. Object Data: Selectively distributed between RAM and Flash:
   - Mutable objects (regular objects) copied to RAM
   - Immutable objects (strings, symbols, bytearrays, blocks) kept in Flash

### ROM-Eligible Objects

Specific classes are identified as immutable and can safely remain in Flash:

- ByteArray (class 18)
- String (class 34)
- Symbol (class 8)
- Block (class 182)

These objects have their reference count set to the maximum (0x7F) to prevent garbage collection, and their memory pointers point directly to Flash memory regions.

### Memory Allocation Strategies

Object allocation follows a multi-strategy approach for efficiency:

1. Free list exact match: Reuse object of exactly the right size
2. Zero-size expansion: Repurpose a size-0 object with new memory
3. Larger object reuse: Repurpose larger object (potentially wasting some space)
4. Smaller object reuse: Reallocate memory for smaller object

### Block Allocation

Rather than individual malloc calls for each object, Smalltalkje allocates memory in blocks of 2048 object slots, significantly reducing allocation overhead and fragmentation.

## Implementation Notes

The system is written in C (not C++) for the ESP-IDF framework (not Arduino) to maximize performance and minimize resource usage. This approach is critical for running Smalltalk, which traditionally requires significant memory, on resource-constrained devices.

The ESP32's multiple cores and sufficient RAM make it possible to run a complete Smalltalk environment while still having resources available for interfacing with the physical world.

## Credits

- Based on Little Smalltalk v3 by Tim Budd
- M5StickC library for ESP-IDF from Pablo Bacho (https://github.com/pablobacho/m5stickc-idf-example)
- SSD1306 I2C OLED driver based on work by Limor Fried/Ladyada for Adafruit Industries
- Additional resources from the ESP-IDF and M5Stack repositories

## License

This work is licensed under the [Creative Commons Attribution 4.0 International License (CC BY 4.0)](https://creativecommons.org/licenses/by/4.0/).

© Abdul Nabi, 2025

You are free to:
- **Share** — copy and redistribute the material in any medium or format  
- **Adapt** — remix, transform, and build upon the material for any purpose, even commercially  

Under the following terms:
- **Attribution** — You must give appropriate credit, provide a link to the license, and indicate if changes were made.  
  You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.

---

### Included Works

Any third-party code, libraries, images, or other assets included in this software are **copyrighted by their respective owners**
and are subject to their own license agreements. Please refer to individual files or documentation for specific licensing terms.
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
