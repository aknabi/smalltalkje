/*
	Smalltalkje, version 1 based on:

	Little Smalltalk, version two
	Written by Tim Budd, Oregon State University, July 1987

	Environment and Cross-Platform Compatibility Header
	
	This header defines fundamental types, macros, and constants that ensure
	consistent behavior across different C compilers and operating systems.
	It serves as the foundation for the Smalltalkje system by isolating
	platform-specific and compiler-specific details.
	
	The definitions in this file address issues such as:
	- Basic type definitions that may vary between compilers
	- Convenience macros for common operations
	- Boolean constants and types
	- Numeric range checks for internal representations
	- Function parameter type abbreviations for better readability
	
	Most of Smalltalkje's source files include this header to ensure
	consistent behavior regardless of the underlying platform.
	
	(This file has remained largely unchanged from the original Little Smalltalk)
*/

#include <stdio.h>

/**
 * Basic Type Definitions
 * 
 * These define fundamental types used throughout the Smalltalkje system.
 */
typedef unsigned char byte;        /* 8-bit unsigned value, used for bytecodes and byte objects */

/**
 * Byte Conversion Macros
 * 
 * These provide consistent handling of byte values across platforms.
 */
#define byteToInt(b) (b)           /* Convert a byte value to an integer */

/**
 * Numeric Range Checks
 * 
 * These macros determine if numeric values fit within Smalltalk's
 * internal representation limits. Smalltalk uses tagged integers
 * with a limited range to avoid allocating separate objects.
 */
/* Range check for Smalltalk integer representation (limited to 15 bits) */
#define longCanBeInt(l) ((l >= -16383) && (l <= 16383))

/**
 * Utility Macros
 * 
 * These provide common operations used throughout the codebase.
 */
// #pragma GCC diagnostic ignored "-Werror=nonnull"  /* Compiler warning suppression */

/* String equality comparison shorthand */
#define streq(a, b) (strcmp(a, b) == 0)

/**
 * Boolean Constants and Type
 * 
 * Standard boolean values and type definition for consistency.
 */
#define true 1                     /* Boolean true value */
#define false 0                    /* Boolean false value */
typedef int boolean;               /* Boolean type (values true/false) */

/**
 * Code Style and Lint Helpers
 * 
 * These macros help with code style and suppressing compiler warnings.
 */
/* Explicitly ignore a function return value */
#define ignore (void)              /* Indicates intentional ignoring of return value */
#define noreturn void              /* Indicates function returns nothing */

/**
 * Function Prototype Type Abbreviations
 * 
 * These abbreviations make function prototypes more readable by
 * providing shorter names for common parameter types. They're used
 * extensively in header files throughout the system.
 * 
 * For example, a function like:
 *   extern object someFunction(object x, int y, char *z);
 * 
 * Can be written as:
 *   extern object someFunction(OBJ X INT X STR);
 */
#define X ,                        /* Parameter separator */
#define OBJ object                 /* Smalltalk object parameter */
#define OBJP object *              /* Pointer to object parameter */
#define INT int                    /* Integer parameter */
#define BOOL boolean               /* Boolean parameter */
#define STR char *                 /* String parameter */
#define FLOAT double               /* Floating point parameter */
#define NOARGS void                /* No parameters */
#define FILEP FILE *               /* File pointer parameter */
#define FUNC ()                    /* Function with no special declaration */
