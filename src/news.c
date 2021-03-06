/*
	little smalltalk, version 3.1
	written by tim budd, July 1988

	new object creation routines
	built on top of memory allocation, these routines
	handle the creation of various kinds of objects
*/

#include <stdio.h>
#include "env.h"
#include "memory.h"
#include "names.h"

static object arrayClass = nilobj;  /* the class Array */
static object intClass = nilobj;    /* the class Integer */
static object stringClass = nilobj; /* the class String */
static object symbolClass = nilobj; /* the class Symbol */

/* ncopy - copy exactly n bytes from place to place */
void ncopy(register char *p, register char *q, register int n)
{
    for (; n > 0; n--)
        *p++ = *q++;
}

object getClass(register object obj) /* getClass - get the class of an object */
{
    if (isInteger(obj))
    {
        if (intClass == nilobj)
            intClass = globalSymbol("Integer");
        return (intClass);
    }
    return (classField(obj));
}

object newArray(int size)
{
    object newObj;

    newObj = allocObject(size);
    if (arrayClass == nilobj)
        arrayClass = globalSymbol("Array");
    setClass(newObj, arrayClass);
    return newObj;
}

object newBlock()
{
    object newObj;

    newObj = allocObject(blockSize);
    setClass(newObj, globalSymbol("Block"));
    return newObj;
}

object newByteArray(int size)
{
    object newobj;

    newobj = allocByte(size);
    setClass(newobj, globalSymbol("ByteArray"));
    return newobj;
}

object newChar(int value)
{
    object newobj;

    newobj = allocObject(1);
    basicAtPut(newobj, 1, newInteger(value));
    setClass(newobj, globalSymbol("Char"));
    return (newobj);
}

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

object newDictionary(int size)
{
    object newObj;

    newObj = allocObject(1);
    setClass(newObj, globalSymbol("Dictionary"));
    basicAtPut(newObj, 1, newArray(size));
    return newObj;
}

object newFloat(double d)
{
    object newObj;

    newObj = allocByte((int)sizeof(double));
    ncopy(charPtr(newObj), (char *)&d, (int)sizeof(double));
    setClass(newObj, globalSymbol("Float"));
    return newObj;
}

double floatValue(object o)
{
    double d;

    ncopy((char *)&d, charPtr(o), (int)sizeof(double));
    return d;
}

object newLink(object key, object value)
{
    object newObj;

    newObj = allocObject(3);
    setClass(newObj, globalSymbol("Link"));
    basicAtPut(newObj, 1, key);
    basicAtPut(newObj, 2, value);
    return newObj;
}

object newMethod()
{
    object newObj;

    newObj = allocObject(methodSize);
    setClass(newObj, globalSymbol("Method"));
    return newObj;
}

object newError(object value)
{
    object newObj;

    newObj = allocObject(1);
    setClass(newObj, globalSymbol("Error"));
    basicAtPut(newObj, 1, value);
    return newObj;
}

// This will copy the string passed to use as the object's data (not the)
object newStString(char *value)
{
    object newObj;

    newObj = allocStr(value);
    if (stringClass == nilobj)
        stringClass = globalSymbol("String");
    setClass(newObj, stringClass);
    return (newObj);
}

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
