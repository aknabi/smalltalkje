/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021

	Based on:
	
	Little Smalltalk, version 3
	Written by Tim Budd, Oregon State University, July 1988

	Primitive Implementation Module
	
	This module implements the primitive operations that form the bridge between
	the Smalltalk environment and the underlying C implementation. Primitives
	allow Smalltalk code to perform operations that cannot be expressed in
	Smalltalk itself, such as:
	
	1. Basic arithmetic and comparison operations
	2. Object allocation and manipulation
	3. System operations (time, random numbers, etc.)
	4. String and character operations
	5. Floating-point mathematics
	
	Unlike Smalltalk-80, Little Smalltalk primitives cannot fail with a doesNotUnderstand:
	message (although they can return nil to indicate failure). In this respect,
	primitives in Little Smalltalk are more like traditional system calls.
	
	The primitives are organized into groups of 10 according to argument count and type:
	- 0-9:    Zero-argument primitives (system operations)
	- 10-19:  One-argument object primitives
	- 20-29:  Two-argument object primitives
	- 30-39:  Three-argument object primitives
	- 50-59:  Integer unary primitives
	- 60-79:  Integer binary primitives
	- 80-89:  String unary primitives
	- 100-109: Float unary primitives
	- 110-119: Float binary primitives
	- 120-139: File I/O primitives (implemented in ioPrimitive.c)
	- 150+:    System-specific primitives (implemented in sysPrimitive.c)
	
	IMPORTANT NOTE: The integer overflow detection technique used in intBinary()
	depends on integers being within certain size limits. Proper overflow checking
	is performed using the longCanBeInt() macro.
*/

#include "build.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "process.h"
#include "env.h"
#include "names.h"
#include "primitive.h"

#ifdef TARGET_ESP32

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#endif

extern object processStack;
extern int linkPointer;

extern double frexp(double, int *);
extern double ldexp(double, int);
extern long time(long*);
extern object ioPrimitive(INT X OBJP);
extern object sysPrimitive(INT X OBJP);
extern void byteAtPut(OBJ X INT X INT);
extern void setInstanceVariables(OBJ);
extern boolean parse(OBJ X char *X boolean);
extern void flushCache(OBJ X OBJ);

/**
 * Zero-argument primitive operations (primitives 0-9)
 * 
 * These primitives don't take any Smalltalk arguments and generally
 * provide system-level operations and information.
 * 
 * @param number The primitive number within the zero-argument group
 * @return The result object of the primitive operation
 */
static object zeroaryPrims(number) int number;
{
	short i;
	object returnedObject;
    int objectCount(void);

	returnedObject = nilobj;
	switch (number)
	{
	case 1: /* Primitive 1: Return the number of objects in Smalltalkje */
		fprintf(stderr, "did primitive 1\n");
		returnedObject = newInteger(objectCount());
		break;

	case 2: /* Primitive 2: Return the number of available objects in Smalltalkje */
		fprintf(stderr, "object count %d context count %d string count: %d\n", 
			objectCount(), 
			classInstCount(globalSymbol("Context")), 
			classInstCount(globalSymbol("String")) );
		returnedObject = newInteger(ObjectTableMax - objectCount());
		break;

	case 3: /* Primitive 3: Return a random number */
		/* Note: This implementation is adjusted for the representation
		 * of integers in Smalltalkje. We strip off lower bits with >> 8,
		 * ensure the value is positive, and then further shift to fit
		 * within the tagged integer range. */
		i = rand() >> 8; /* strip off lower bits */
		if (i < 0)
			i = -i;
		returnedObject = newInteger(i >> 1);
		break;

	case 4: /* Primitive 4: Return current time in seconds */
		/* TODO: This needs to move to datetime primitives */
		i = (short)time((long *)0);
		returnedObject = newInteger(i);
		break;

	case 5: /* Primitive 5: Return true if the device has a display */
		/* This was originally for flipping the watch flag, but is now used
		 * to determine if the current device has a display capability */
#ifdef DEVICE_DISPLAY_TYPE
		returnedObject = trueobj;
#else
		returnedObject = falseobj;
#endif
		break;

	case 6: /* Primitive 6: Return a queued block for VM to execute (or nil if none) */
		/* This is part of the event-driven execution system, allowing
		 * the VM to get blocks that have been queued by asynchronous events */
		returnedObject = getNextVMBlockToRun();
		break;

	case 7: /* Primitive 7: Reset a block that the VM needs to run (now deprecated) */
	case 8: /* Primitive 8: Available for future use */
		/* These primitives used to manage the vmBlockToRun variable,
		 * but this functionality has been replaced by the queue system */
		returnedObject = trueobj;
		break;

	case 9: /* Primitive 9: Exit the system immediately */
		exit(0);
		break;

	default: /* Unknown primitive */
		sysError("unknown primitive", "zeroargPrims");
		break;
	}
	return (returnedObject);
}

object blockToExecute;
extern void doIt(char *evalText, object arg);
extern void runBlock(object block, object arg);

char *primString = { 0x0, 0x0 };

/**
 * One-argument primitive operations (primitives 10-19)
 * 
 * These primitives take a single Smalltalk object as an argument
 * and perform various operations on it.
 * 
 * @param number The primitive number within the unary group (0-9)
 * @param firstarg The Smalltalk object argument
 * @return The result object of the primitive operation
 */
static int unaryPrims(number, firstarg) int number;
object firstarg;
{
	int i, j, saveLinkPointer;
	object returnedObject, saveProcessStack;

	returnedObject = firstarg;
	switch (number)
	{
	case 0: /* Prim 10 instance count of class */
		returnedObject = newInteger(classInstCount(firstarg));
		break;

	case 1: /* Prim 11 class of object */
		returnedObject = getClass(firstarg);
		break;

	case 2: /* Prim 12 basic size of object */
		if (isInteger(firstarg))
			i = 0;
		else
		{
			i = sizeField(firstarg);
			/* byte objects have negative size */
			if (i < 0)
				i = (-i);
		}
		returnedObject = newInteger(i);
		break;

	case 3: /* Prim 13 hash value of object */
		if (isInteger(firstarg))
			returnedObject = firstarg;
		else
			returnedObject = newInteger(firstarg);
		break;

	case 4: /* Prim 14 basic print */
		printf("%s", charPtr(firstarg));
		fflush(stdout);
		break;

	case 5: /* Prim 15 Create a string with the Char passed in */
		returnedObject = nilobj;
		char c = (char) intValue( basicAt(firstarg, 1) );
		if (c != 0x0) {
			returnedObject = newStString(" ");
			charPtr(returnedObject)[0] = c;
		}
		break;

	// TODO: Given String>>value is available and this prim unused, could delete
	case 6: /* Prim 16 - Execute string */
		fprintf(stderr, "primitive 16 execute string %s\n", charPtr(firstarg));
		doIt(charPtr(firstarg), nilobj);
		break;

	// TODO: This creates havoc with the interpreter... either sort it out or delete and redo at a later date
	case 7: /* prim 17 - Execute block (Block forkTask)... WAS Execute saved block with first argument */
		runBlock(firstarg, nilobj);
		returnedObject = trueobj;
		// if ( blockToExecute == nilobj ) {
		// 	returnedObject = falseobj;
		// } else {
		// 	runBlock(blockToExecute, firstarg);
		// 	returnedObject = trueobj;
		// }
		break;

	case 8: /* Prim 18 change return point - block return */
		/* first get previous link pointer */
		i = intValue(basicAt(processStack, linkPointer));
		/* then creating context pointer */
		j = intValue(basicAt(firstarg, 1));
		if (basicAt(processStack, j + 1) != firstarg)
		{
			returnedObject = falseobj;
			break;
		}
		/* first change link pointer to that of creator */
		fieldAtPut(processStack, i, basicAt(processStack, j));
		/* then change return point to that of creator */
		fieldAtPut(processStack, i + 2, basicAt(processStack, j + 2));
		returnedObject = trueobj;
		break;

	case 9: /* Prim 19 process execute */
		/* first save the values we are about to clobber */
		saveProcessStack = processStack;
		saveLinkPointer = linkPointer;
#ifdef SIGNAL
		/* trap control-C */
		signal(SIGINT, brkfun);
		if (setjmp(jb))
		{
			returnedObject = falseobj;
		}
		else
#endif
#ifdef CRTLBRK
			/* trap control-C using dos ctrlbrk routine */
			ctrlbrk(brkfun);
		if (setjmp(jb))
		{
			returnedObject = falseobj;
		}
		else
#endif
			if (execute(firstarg, 5000))
			returnedObject = trueobj;
		else
			returnedObject = falseobj;
		/* then restore previous environment */
		processStack = saveProcessStack;
		linkPointer = saveLinkPointer;
#ifdef SIGNAL
		signal(SIGINT, brkignore);
#endif
#ifdef CTRLBRK
		ctrlbrk(brkignore);
#endif
		break;

	default: /* unknown primitive */
		sysError("unknown primitive", "unaryPrims");
		break;
	}
	return (returnedObject);
}

/**
 * Two-argument primitive operations (primitives 20-29)
 * 
 * These primitives take two Smalltalk objects as arguments
 * and perform various operations on them.
 * 
 * @param number The primitive number within the binary group (0-9)
 * @param firstarg The first Smalltalk object argument
 * @param secondarg The second Smalltalk object argument
 * @return The result object of the primitive operation
 */
static int binaryPrims(number, firstarg, secondarg) int number;
object firstarg, secondarg;
{
	char buffer[2000];
	int i;
	object returnedObject;

	returnedObject = firstarg;
	switch (number)
	{

	case 0: /* prim 20 Free to use */
		returnedObject = falseobj;
		break;

	case 1: /* Prim 21 object identity test */
		if (firstarg == secondarg)
			returnedObject = trueobj;
		else
			returnedObject = falseobj;
		break;

	case 2: /* Prim 22 set class of object */
		decr(classField(firstarg));
		setClass(firstarg, secondarg);
		returnedObject = firstarg;
		break;

	// TODO: Prim 23 available for use... done need debugging prim
	case 3: /* Prim 23 debugging stuff */
		fprintf(stderr, "primitive 23 %d %d\n", firstarg, secondarg);
		break;

	case 4: /* Prim 24 string cat */
		ignore strcpy(buffer, charPtr(firstarg));
		ignore strcat(buffer, charPtr(secondarg));
		returnedObject = newStString(buffer);
		break;

	case 5: /* Prim 25 basicAt: */
		checkInteger(secondarg)
		returnedObject = basicAt(firstarg, intValue(secondarg));
		break;

	case 6: /* Prim 26 byteAt: */
		checkInteger(secondarg)
		i = byteAt(firstarg, intValue(secondarg));
		if (i < 0)
			i += 256;
		returnedObject = newInteger(i);
		break;

	case 7: /* Prim 27 symbol set */
		nameTableInsert(symbols, strHash(charPtr(firstarg)),
						firstarg, secondarg);
		break;

	case 8: /* Prim 28 block start */
		/* first get previous link */
		i = intValue(basicAt(processStack, linkPointer));
		/* change context and byte pointer */
		fieldAtPut(processStack, i + 1, firstarg);
		fieldAtPut(processStack, i + 4, secondarg);
		break;

	case 9: /* Prim 29 duplicate a block, adding a new context to it */
		returnedObject = newBlock();
		basicAtPut(returnedObject, 1, secondarg);
		basicAtPut(returnedObject, 2, basicAt(firstarg, 2));
		basicAtPut(returnedObject, 3, basicAt(firstarg, 3));
		basicAtPut(returnedObject, 4, basicAt(firstarg, 4));
		break;

	default: /* unknown primitive */
		sysError("unknown primitive", "binaryPrims");
		break;
	}
	return (returnedObject);
}

/**
 * Three-argument primitive operations (primitives 30-39)
 * 
 * These primitives take three Smalltalk objects as arguments
 * and perform various operations on them.
 * 
 * @param number The primitive number within the trinary group (0-9)
 * @param firstarg The first Smalltalk object argument
 * @param secondarg The second Smalltalk object argument
 * @param thirdarg The third Smalltalk object argument
 * @return The result object of the primitive operation
 */
static int trinaryPrims(number, firstarg, secondarg, thirdarg) int number;
object firstarg, secondarg, thirdarg;
{
	char *bp, *tp, buffer[256];
	int i, j;
	object returnedObject;

	returnedObject = firstarg;
	switch (number)
	{
	// TODO: Prim 30 available

	case 1: /* Prim 31 basicAt:Put: */
		checkInteger(secondarg)
		fprintf(stderr, "IN BASICATPUT %d %d %d\n", firstarg,
				intValue(secondarg), thirdarg);
		fieldAtPut(firstarg, intValue(secondarg), thirdarg);
		break;

	case 2: /* Prim 32 basicAt:Put: for bytes */
		if (!isInteger(secondarg))
			sysError("non integer index", "byteAtPut");
		if (!isInteger(thirdarg))
			sysError("assigning non int", "to byte");
		byteAtPut(firstarg, intValue(secondarg), intValue(thirdarg));
		break;

	case 3: /* Prim 33 string copyFrom:to: */
		bp = charPtr(firstarg);
		if ((!isInteger(secondarg)) || (!isInteger(thirdarg)))
			sysError("non integer index", "copyFromTo");
		i = intValue(secondarg);
		j = intValue(thirdarg);
		tp = buffer;
		if (i <= strlen(bp))
			for (; (i <= j) && bp[i - 1]; i++)
				*tp++ = bp[i - 1];
		*tp = '\0';
		returnedObject = newStString(buffer);
		break;

	case 9: /* compile method */
		setInstanceVariables(firstarg);
		if (parse(thirdarg, charPtr(secondarg), false))
		{
			flushCache(basicAt(thirdarg, messageInMethod), firstarg);
			returnedObject = trueobj;
		}
		else
			returnedObject = falseobj;
		break;

	default: /* unknown primitive */
		sysError("unknown primitive", "trinaryPrims");
		break;
	}
	return (returnedObject);
}

/**
 * Integer unary primitive operations (primitives 50-59)
 * 
 * These primitives take a single integer value as an argument
 * and perform various operations on it. The integer is already
 * extracted from its Smalltalk representation.
 * 
 * @param number The primitive number within the integer unary group (0-9)
 * @param firstarg The C integer value extracted from a Smalltalk integer
 * @return The result object of the primitive operation
 */
static int intUnary(int number, int firstarg)
{
	object returnedObject = nilobj;

	switch (number)
	{
	case 1: /* float equiv of integer */
		returnedObject = newFloat((double)firstarg);
		break;

	// TODO: This is usused... either implement and use or remove/reuse
	case 2: /* print - for debugging purposes */
		fprintf(stderr, "debugging print %d\n", firstarg);
		break;

	// TODO: This is usused... either implement and use (set timeslice counter) or remove/reuse
	case 3: /* set time slice - done in interpreter */
		break;

	case 5: /* set random number */
		ignore srand((unsigned)firstarg);
		returnedObject = nilobj;
		break;

	case 8: /* Return an new, uninitialized object of a given size */
		returnedObject = allocObject(firstarg);
		break;

	case 9: /* Return an new, uninitialized byte based object of a given size */
		returnedObject = allocByte(firstarg);
		break;

	default:
		sysError("intUnary primitive", "not implemented yet");
	}
	return (returnedObject);
}

/**
 * Integer binary primitive operations (primitives 60-79)
 * 
 * These primitives take two integer values as arguments
 * and perform various operations on them. The integers are already
 * extracted from their Smalltalk representations.
 * 
 * This includes arithmetic operations (+, -, *, /), comparisons,
 * bit operations, and shifts.
 * 
 * @param number The primitive number within the integer binary group (0-19)
 * @param firstarg The first C integer value
 * @param secondarg The second C integer value
 * @return The result object of the primitive operation
 */
/**
 * Integer binary primitive operations (primitives 60-79)
 * 
 * This function implements all the binary operations for integer values:
 * - Arithmetic operations (+, -, *, /, %)
 * - Comparison operations (<, >, <=, >=, ==, !=)
 * - Bitwise operations (AND, XOR)
 * - Bit shifting (left and right)
 * 
 * All operations include proper overflow checking. When an overflow occurs,
 * the function returns nil, allowing the Smalltalk code to handle the
 * overflow condition (typically by using a large number implementation).
 * 
 * To prevent overflow in intermediate calculations, operations are performed
 * using a long type, then checked to ensure the result fits in a Smalltalk
 * integer before returning.
 *
 * @param number The primitive operation number (0-19, relative to group start)
 * @param firstarg The first C integer value
 * @param secondarg The second C integer value
 * @return The result as a Smalltalk object (integer, true/false, or nil on overflow)
 */
static object intBinary(number, firstarg, secondarg) register int firstarg, secondarg;
int number;
{
    boolean binresult = false;
	long longresult;
	object returnedObject;

	switch (number)
	{
	case 0: /* addition (prim 60) */
		longresult = firstarg;
		longresult += secondarg;
		if (longCanBeInt(longresult))
			firstarg = (int) longresult;
		else
			goto overflow;
		break;
		
	case 1: /* subtraction (prim 61) */
		longresult = firstarg;
		longresult -= secondarg;
		if (longCanBeInt(longresult))
			firstarg = (int) longresult;
		else
			goto overflow;
		break;

	/* Comparison operations (prims 62-67) */
	case 2: /* less than (prim 62) */
		binresult = firstarg < secondarg;
		break;
	case 3: /* greater than (prim 63) */
		binresult = firstarg > secondarg;
		break;
	case 4: /* less than or equal (prim 64) */
		binresult = firstarg <= secondarg;
		break;
	case 5: /* greater than or equal (prim 65) */
		binresult = firstarg >= secondarg;
		break;
	case 6: /* equal (prim 66) */
	case 13: /* identical (prim 73) */
		binresult = firstarg == secondarg;
		break;
	case 7: /* not equal (prim 67) */
		binresult = firstarg != secondarg;
		break;

	case 8: /* multiplication (prim 68) */
		longresult = firstarg;
		longresult *= secondarg;
		if (longCanBeInt(longresult))
			firstarg = (int) longresult;
		else
			goto overflow;
		break;

	case 9: /* integer division (prim 69) */
		if (secondarg == 0)
			goto overflow;
		firstarg /= secondarg;
		break;

	case 10: /* remainder (prim 70) */
		if (secondarg == 0)
			goto overflow;
		firstarg %= secondarg;
		break;

	case 11: /* bitwise AND (prim 71) */
		firstarg &= secondarg;
		break;

	case 12: /* bitwise XOR (prim 72) */
		firstarg ^= secondarg;
		break;

	case 19: /* bit shift (prim 79) */
		/* Negative shift value means shift right, positive means shift left */
		if (secondarg < 0)
			firstarg >>= (-secondarg);  /* Right shift (divide by power of 2) */
		else
			firstarg <<= secondarg;     /* Left shift (multiply by power of 2) */
		break;
	}
	
	/* For comparison operations, return true or false */
	if ((number >= 2) && (number <= 7))
		returnedObject = binresult ? trueobj : falseobj;
	else
		/* For arithmetic and bitwise operations, return the integer result */
		returnedObject = newInteger(firstarg);
		
	return (returnedObject);

	/* On overflow or division by zero, return nil and let Smalltalk code handle it */
overflow:
	returnedObject = nilobj;
	return (returnedObject);
}

/**
 * String unary primitive operations (primitives 80-89)
 * 
 * These primitives take a single string (char*) as an argument
 * and perform various operations on it. The string pointer is
 * already extracted from the Smalltalk ByteArray/String object.
 * 
 * @param number The primitive number within the string unary group (0-9)
 * @param firstargument The C string pointer extracted from a Smalltalk string
 * @return The result object of the primitive operation
 */
static int strUnary(number, firstargument) int number;
char *firstargument;
{
	object returnedObject = nilobj;

	switch (number)
	{
	case 1: /* length of string */
		returnedObject = newInteger((int) strlen(firstargument));
		break;

	case 2: /* hash value of symbol */
		returnedObject = newInteger(strHash(firstargument));
		break;

	case 3: /* string as symbol */
		returnedObject = newSymbol(firstargument);
		break;

	case 7: /* value of symbol */
		returnedObject = globalSymbol(firstargument);
		break;

	case 8:
#ifndef NOSYSTEM
		returnedObject = newInteger(system(firstargument));
#endif
		break;

	case 9:
		sysError("fatal error", firstargument);
		break;

	default:
		sysError("unknown primitive", "strUnary");
		break;
	}

	return (returnedObject);
}

/**
 * Float unary primitive operations (primitives 100-109)
 * 
 * These primitives take a single floating-point value as an argument
 * and perform various operations on it. The float value is already
 * extracted from its Smalltalk representation.
 * 
 * @param number The primitive number within the float unary group (0-9)
 * @param firstarg The C double value extracted from a Smalltalk float
 * @return The result object of the primitive operation
 */
static int floatUnary(number, firstarg) int number;
double firstarg;
{
	char buffer[20];
	double temp;
	int i, j;
	object returnedObject = nilobj;

	switch (number)
	{
	case 1: /* floating value asString */
		ignore sprintf(buffer, "%g", firstarg);
		returnedObject = newStString(buffer);
		break;

	case 2: /* log */
		returnedObject = newFloat(log(firstarg));
		break;

	case 3: /* exp */
		returnedObject = newFloat(exp(firstarg));
		break;

	case 6: /* integer part */
			/* return two integers n and m such that */
			/* number can be written as n * 2** m */
#define ndif 12
		temp = frexp(firstarg, &i);
		if ((i >= 0) && (i <= ndif))
		{
			temp = ldexp(temp, i);
			i = 0;
		}
		else
		{
			i -= ndif;
			temp = ldexp(temp, ndif);
		}
		j = (int)temp;
		returnedObject = newArray(2);
		basicAtPut(returnedObject, 1, newInteger(j));
		basicAtPut(returnedObject, 2, newInteger(i));
#ifdef trynew
		/* if number is too big it can't be integer anyway */
		if (firstarg > 2e9)
			returnedObject = nilobj;
		else
		{
			ignore modf(firstarg, &temp);
			ltemp = (long)temp;
			if (longCanBeInt(ltemp))
				returnedObject = newInteger((int)temp);
			else
				returnedObject = newFloat(temp);
		}
#endif
		break;

	default:
		sysError("unknown primitive", "floatUnary");
		break;
	}

	return (returnedObject);
}

/**
 * Float binary primitive operations (primitives 110-119)
 * 
 * These primitives take two floating-point values as arguments
 * and perform various operations on them. The float values are already
 * extracted from their Smalltalk representations.
 * 
 * This includes arithmetic operations (+, -, *, /), and comparisons.
 * 
 * @param number The primitive number within the float binary group (0-9)
 * @param first The first C double value
 * @param second The second C double value
 * @return The result object of the primitive operation
 */
static object floatBinary(number, first, second) int number;
double first, second;
{
    boolean binResult = false;
	object returnedObject;

	switch (number)
	{
	case 0:
		first += second;
		break;

	case 1:
		first -= second;
		break;
	case 2:
		binResult = (first < second);
		break;
	case 3:
		binResult = (first > second);
		break;
	case 4:
		binResult = (first <= second);
		break;
	case 5:
		binResult = (first >= second);
		break;
	case 6:
		binResult = (first == second);
		break;
	case 7:
		binResult = (first != second);
		break;
	case 8:
		first *= second;
		break;
	case 9:
		first /= second;
		break;
	default:
		sysError("unknown primitive", "floatBinary");
		break;
	}

	if ((number >= 2) && (number <= 7))
		if (binResult)
			returnedObject = trueobj;
		else
			returnedObject = falseobj;
	else
		returnedObject = newFloat(first);
	return (returnedObject);
}

/**
 * Main primitive dispatch function
 * 
 * This is the entry point for all primitive operations in the system.
 * It takes a primitive number and an array of arguments, determines
 * which primitive handler to call, and returns the result.
 * 
 * The primitive number determines which group of primitives to invoke:
 * - 0-9:    Zero-argument primitives
 * - 10-19:  One-argument object primitives
 * - 20-29:  Two-argument object primitives
 * - 30-39:  Three-argument object primitives
 * - 50-59:  Integer unary primitives
 * - 60-79:  Integer binary primitives
 * - 80-89:  String unary primitives
 * - 100-109: Float unary primitives
 * - 110-119: Float binary primitives
 * - 120-139: File I/O primitives (implemented in ioPrimitive.c)
 * - 150+:    System-specific primitives (implemented in sysPrimitive.c)
 * 
 * @param primitiveNumber The number of the primitive to execute
 * @param arguments Array of Smalltalk object arguments
 * @return The result object of the primitive operation
 */
object primitive(register int primitiveNumber, object *arguments)
{
	register int primitiveGroup = primitiveNumber / 10;
	object returnedObject = nilobj;

	if (primitiveNumber >= 150)
	{
		/* system dependent primitives, handled in separate module */
		returnedObject = sysPrimitive(primitiveNumber, arguments);
	}
	else
	{
		switch (primitiveGroup)
		{
		case 0:
			returnedObject = zeroaryPrims(primitiveNumber);
			break;
		case 1:
			returnedObject =
				unaryPrims(primitiveNumber - 10, arguments[0]);
			break;
		case 2:
			returnedObject =
				binaryPrims(primitiveNumber - 20, arguments[0],
							arguments[1]);
			break;
		case 3:
			returnedObject =
				trinaryPrims(primitiveNumber - 30, arguments[0],
							 arguments[1], arguments[2]);
			break;

		case 5: /* integer unary operations */
			if (!isInteger(arguments[0]))
				returnedObject = nilobj;
			else
				returnedObject = intUnary(primitiveNumber - 50,
										  intValue(arguments[0]));
			break;

		case 6:
		case 7: /* integer binary operations */
			if ((!isInteger(arguments[0])) || !isInteger(arguments[1]))
				returnedObject = nilobj;
			else
				returnedObject = intBinary(primitiveNumber - 60,
										   intValue(arguments[0]),
										   intValue(arguments[1]));
			break;

		case 8: /* string unary */
			returnedObject =
				strUnary(primitiveNumber - 80, charPtr(arguments[0]));
			break;

		case 10: /* float unary */
			returnedObject =
				floatUnary(primitiveNumber - 100,
						   floatValue(arguments[0]));
			break;

		case 11: /* float binary */
			returnedObject = floatBinary(primitiveNumber - 110,
										 floatValue(arguments[0]),
										 floatValue(arguments[1]));
			break;

		case 12:
		case 13: /* file operations */

			returnedObject = ioPrimitive(primitiveNumber - 120, arguments);
			break;

		default:
			sysError("unknown primitive number", "doPrimitive");
			break;
		}
	}

	return (returnedObject);
}
