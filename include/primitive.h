/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021

	Primitive Support Header
	
	This header provides macros and utilities for implementing primitive methods in the
	Smalltalk system. Primitives are operations implemented directly in C code rather
	than Smalltalk, typically for performance reasons or to access system-level functionality.
	
	The macros defined here provide consistent argument checking and value extraction
	for primitive implementations, ensuring type safety and proper error handling.
	They are used extensively in the primitive.c and sysprim.c files.
*/

/**
 * Integer Argument Extraction
 * 
 * Extracts the integer value from a Smalltalk integer object.
 * This macro is used to convert from Smalltalk's tagged integer representation
 * to a native C integer value that can be used in primitive operations.
 * 
 * @param i Index of the argument in the arguments array
 * @return The C integer value of the argument
 */
#define getIntArg(i) intValue(arguments[i])

/**
 * Integer Argument Validation
 * 
 * Checks if the specified argument is a Smalltalk integer.
 * This is a shorthand for checking an argument from the arguments array.
 * 
 * @param i Index of the argument in the arguments array
 */
#define checkIntArg(i) checkInteger(arguments[i])

/**
 * Integer Object Validation
 * 
 * Verifies that an object is a valid Smalltalk integer.
 * If the object is not an integer, raises a system error.
 * This is used to ensure type safety before operations that
 * require integer values.
 * 
 * @param i The Smalltalk object to check
 */
#define checkInteger(i)                     \
    if (!isInteger(i))                      \
    {                                       \
        sysError("non integer index", "x"); \
    }

/**
 * Class Type Validation
 * 
 * Verifies that an argument is an instance of the expected class.
 * If the argument is not of the specified class, raises a system error.
 * This is used to ensure type safety before operations that require
 * specific object types.
 * 
 * @param i Index of the argument in the arguments array
 * @param classStr String name of the expected class
 */
#define checkArgClass(i, classStr)                                \
    if (classField(arguments[i]) != findClass(classStr))          \
    {                                                             \
        sysError("Argument is not the expected class", classStr); \
    }

/**
 * Combined Integer Validation and Extraction
 * 
 * This macro combines checking if an argument is an integer and
 * extracting its value. It's a convenience for the common pattern
 * of validating and then using an integer argument.
 * 
 * @param i Index of the argument in the arguments array
 * @return The C integer value of the argument
 */
#define checkAndGetIntArg(i)    \
    checkIntArg(i)              \
    getIntArg(i)
