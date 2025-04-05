/*
    little smalltalk, version 3.1
    written by tim budd, July 1988

    new object creation routines
    built on top of memory allocation, these routines
    handle the creation of various kinds of objects

    This file contains functions for creating various types of Smalltalk objects
    including arrays, blocks, byte arrays, characters, classes, contexts, 
    dictionaries, floats, links, methods, errors, strings, and symbols.
    
    Most functions follow a similar pattern:
    1. Allocate memory for the object
    2. Set the object's class
    3. Initialize any required fields
    4. Return the new object
*/

#include <stdio.h>
#include "env.h"
#include "memory.h"
#include "names.h"

/* Cached class references to avoid repeated lookups */
static object arrayClass = nilobj;  /* the class Array */
static object intClass = nilobj;    /* the class Integer */
static object stringClass = nilobj; /* the class String */
static object symbolClass = nilobj; /* the class Symbol */

/**
 * Copy exactly n bytes from source to destination
 *
 * @param p Destination buffer
 * @param q Source buffer
 * @param n Number of bytes to copy
 */
void ncopy(register char *p, register char *q, register int n)
{
    for (; n > 0; n--)
        *p++ = *q++;
}

/**
 * Get the class of an object
 * Handles special case for integers, which don't have a class field
 * 
 * @param obj The object whose class we want to determine
 * @return The class object for the given object
 */
object getClass(register object obj)
{
    if (isInteger(obj))
    {
        if (intClass == nilobj)
            intClass = globalSymbol("Integer");
        return (intClass);
    }
    return (classField(obj));
}

/**
 * Create a new Array object with the specified size
 *
 * @param size Number of elements in the array
 * @return Newly created Array object
 */
object newArray(int size)
{
    object newObj;

    newObj = allocObject(size);
    if (arrayClass == nilobj)
        arrayClass = globalSymbol("Array");
    setClass(newObj, arrayClass);
    return newObj;
}

/**
 * Create a new Block object
 * Block objects represent code blocks in Smalltalk
 *
 * @return Newly created Block object
 */
object newBlock()
{
    object newObj;

    newObj = allocObject(blockSize);
    setClass(newObj, globalSymbol("Block"));
    return newObj;
}

/**
 * Create a new ByteArray object with the specified size
 * ByteArrays store raw bytes rather than object references
 *
 * @param size Number of bytes to allocate
 * @return Newly created ByteArray object
 */
object newByteArray(int size)
{
    object newobj;

    newobj = allocByte(size);
    setClass(newobj, globalSymbol("ByteArray"));
    return newobj;
}

/**
 * Create a new Character object with the given ASCII/Unicode value
 *
 * @param value The numeric value of the character
 * @return Newly created Character object
 */
object newChar(int value)
{
    object newobj;

    newobj = allocObject(1);
    basicAtPut(newobj, 1, newInteger(value));
    setClass(newobj, globalSymbol("Char"));
    return (newobj);
}

/**
 * Internal helper function to create a basic shallow copy of an object
 * Copies the object's class and all its instance variables directly
 * 
 * @param obj The object to copy
 * @return A shallow copy of the object
 */
static object basicShallowCopy(object obj)
{
    object newObj;
    int size = (int)sizeField(obj);
    newObj = allocObject(size);
    setClass(newObj, getClass(obj));
    incr(obj);
    for (int i = 1; i <= size; i++)
    {
        basicAtPut(newObj, i, basicAt(obj, i));
    }
    return newObj;
}

/**
 * Create a shallow copy of an object
 * The copy has the same class as the original and copies of all instance variables
 * Integer instance variables are not copied but shared (since integers are immutable)
 * 
 * @param obj The object to copy
 * @return A shallow copy of the object
 */
object shallowCopy(object obj)
{
    object newObj;
    int size = (int)sizeField(obj);
    newObj = allocObject(size);
    setClass(newObj, getClass(obj));
    incr(obj);
    for (int i = 1; i <= size; i++)
    {
        // basicAtPut(newObj, i, basicAt(obj, i) );
        object instVar = basicAt(obj, i);
        object varCopy = isInteger(instVar) ? instVar : basicShallowCopy(instVar);
        basicAtPut(newObj, i, varCopy);
    }
    /*
    printf("Original ");
    printObject(obj);
    printf("New ");
    printObject(newObj);
    */
    return newObj;
}

/*
void printObject(object obj)
{
    printf("Object Info for: %d\n", obj);
    printf("\tclass: %d\n", getClass(obj));
    int size = (int) sizeField(obj);
    printf("\tsize: %d\n", size);
    for (int i = 1; i <= size; i++) {
        printf("\tinst var: %d\tvalue:%d class:%d\n", i, basicAt(obj, i), getClass(basicAt(obj, i)));
	    // basicAtPut(newObj, i, basicShallowCopy(basicAt(obj, i)) );
	}
}
*/

/**
 * Create a new Class object with the given name
 * Also adds the class to the global symbols table
 *
 * @param name Name of the class to create
 * @return Newly created Class object
 */
object newClass(char *name)
{
    object newObj, nameObj;

    newObj = allocObject(classSize);
    setClass(newObj, globalSymbol("Class"));

    /* now make name */
    nameObj = newSymbol(name);
    basicAtPut(newObj, nameInClass, nameObj);

    /* now put in global symbols table */
    nameTableInsert(symbols, strHash(name), nameObj, newObj);

    return newObj;
}

/**
 * Create a new Array by copying elements from an existing array
 *
 * @param obj Source array to copy from
 * @param start Starting index in the source array (1-based)
 * @param size Number of elements to copy
 * @return New array containing the copied elements
 */
object copyFrom(object obj, int start, int size)
{
    object newObj;
    int i;

    newObj = newArray(size);
    for (i = 1; i <= size; i++)
    {
        basicAtPut(newObj, i, basicAt(obj, start));
        start++;
    }
    return newObj;
}

/**
 * Create a new Context object
 * Contexts represent execution frames in the Smalltalk virtual machine
 *
 * @param link Pointer to previous context in the execution chain
 * @param method The method being executed
 * @param args The arguments to the method
 * @param temp Temporary variables for the context
 * @return Newly created Context object
 */
object newContext(int link, object method, object args, object temp)
{
    object newObj;

    newObj = allocObject(contextSize);
    setClass(newObj, globalSymbol("Context"));
    basicAtPut(newObj, linkPtrInContext, newInteger(link));
    basicAtPut(newObj, methodInContext, method);
    basicAtPut(newObj, argumentsInContext, args);
    basicAtPut(newObj, temporariesInContext, temp);
    return newObj;
}

/**
 * Create a new Dictionary object with the specified size
 * Dictionaries are implemented as arrays of associations (key-value pairs)
 *
 * @param size Initial capacity of the dictionary
 * @return Newly created Dictionary object
 */
object newDictionary(int size)
{
    object newObj;

    newObj = allocObject(1);
    setClass(newObj, globalSymbol("Dictionary"));
    basicAtPut(newObj, 1, newArray(size));
    return newObj;
}

/**
 * Create a new Float object with the given double value
 * Stores the raw bytes of the double in a ByteArray-like object
 *
 * @param d The double value to store
 * @return Newly created Float object
 */
object newFloat(double d)
{
    object newObj;

    newObj = allocByte((int)sizeof(double));
    ncopy(charPtr(newObj), (char *)&d, (int)sizeof(double));
    setClass(newObj, globalSymbol("Float"));
    return newObj;
}

/**
 * Extract the double value from a Float object
 *
 * @param o The Float object
 * @return The double value stored in the object
 */
double floatValue(object o)
{
    double d;

    ncopy((char *)&d, charPtr(o), (int)sizeof(double));
    return d;
}

/**
 * Create a new Link object (key-value association)
 * Used primarily in Dictionary implementations
 *
 * @param key The key object
 * @param value The value object
 * @return Newly created Link object
 */
object newLink(object key, object value)
{
    object newObj;

    newObj = allocObject(3);
    setClass(newObj, globalSymbol("Link"));
    basicAtPut(newObj, 1, key);
    basicAtPut(newObj, 2, value);
    return newObj;
}

/**
 * Create a new Method object
 * Methods represent executable code in the Smalltalk system
 *
 * @return Newly created Method object
 */
object newMethod()
{
    object newObj;

    newObj = allocObject(methodSize);
    setClass(newObj, globalSymbol("Method"));
    return newObj;
}

/**
 * Create a new Error object with the given value
 * Used to represent runtime errors in the Smalltalk system
 *
 * @param value The error value/message
 * @return Newly created Error object
 */
object newError(object value)
{
    object newObj;

    newObj = allocObject(1);
    setClass(newObj, globalSymbol("Error"));
    basicAtPut(newObj, 1, value);
    return newObj;
}

/**
 * Create a new String object with the given value
 * Copies the provided C string into the object's data
 *
 * @param value The C string to copy
 * @return Newly created String object
 */
object newStString(char *value)
{
    object newObj;

    newObj = allocStr(value);
    if (stringClass == nilobj)
        stringClass = globalSymbol("String");
    setClass(newObj, stringClass);
    return (newObj);
}

/**
 * Create a new Symbol object with the given string value
 * Symbols are unique string-like objects used as identifiers
 * If a symbol with the given string already exists, it's returned instead
 *
 * @param str The C string value for the symbol
 * @return Newly created Symbol object or existing Symbol if found
 */
object newSymbol(char *str)
{
    object newObj;

    /* first see if it is already there */
    newObj = globalKey(str);
    if (newObj)
        return newObj;

    /* not found, must make */
    newObj = allocStr(str);
    if (symbolClass == nilobj)
        symbolClass = globalSymbol("Symbol");
    setClass(newObj, symbolClass);
    nameTableInsert(symbols, strHash(str), newObj, nilobj);
    return newObj;
}
