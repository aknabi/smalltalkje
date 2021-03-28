/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021

	primitive.h - Macro and other support for primitives
*/

#define getIntArg(i) intValue(arguments[i])

#define checkIntArg(i) checkInteger(arguments[i])

#define checkInteger(i)                     \
    if (!isInteger(i))                      \
    {                                       \
        sysError("non integer index", "x"); \
    }


#define checkArgClass(i, classStr)                                \
    if (classField(arguments[i]) != findClass(classStr))          \
    {                                                             \
        sysError("Argument is not the expected class", classStr); \
    }

#define checkAndGetIntArg(i)    \
    checkIntArg(i)              \
    getIntArg(i)