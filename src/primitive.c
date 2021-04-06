/*

	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021

	Based on:
	
	Little Smalltalk, version 3
	Written by Tim Budd, Oregon State University, July 1988

	Primitive processor

	primitives are how actions are ultimately executed in the Smalltalk
	system.
	unlike ST-80, Little Smalltalk primitives cannot fail (although
	they can return nil, and methods can take this as an indication
	of failure).  In this respect primitives in Little Smalltalk are
	much more like traditional system calls.

	Primitives are combined into groups of 10 according to
	argument count and type, and in some cases type checking is performed.

	IMPORTANT NOTE:
		The technique used to tell if an arithmetic operation
		has overflowed in intBinary() depends upon integers
		being 16 bits.  If this is not true, other techniques
		may be required.

	system specific I/O primitives are found in a different file.
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

static object zeroaryPrims(number) int number;
{
	short i;
	object returnedObject;
    int objectCount(void);

	returnedObject = nilobj;
	switch (number)
	{

	// Return the number of objects in Smalltalkje
	case 1:
		fprintf(stderr, "did primitive 1\n");
		returnedObject = newInteger(objectCount());
		break;

	// Return the number of availalbe objects in Smalltalkje
	case 2:
		fprintf(stderr, "object count %d context count %d string count: %d\n", objectCount(), classInstCount(globalSymbol("Context")), classInstCount(globalSymbol("String")) );
		returnedObject = newInteger(ObjectTableMax - objectCount());
		break;

	case 3: /* return a random number */
		/* this is hacked because of the representation */
		/* of integers as shorts */
		i = rand() >> 8; /* strip off lower bits */
		if (i < 0)
			i = -i;
		returnedObject = newInteger(i >> 1);
		break;

	// TODO: This needs to move to datetime prims... makes prim 4 available
	case 4: /* return time in seconds */
		i = (short)time((long *)0);
		returnedObject = newInteger(i);
		break;

	// Prim 5 True true if the device has a display
	case 5: /* flip watch - done in interp */
#ifdef DEVICE_DISPLAY_TYPE
		returnedObject = trueobj;
#else
		returnedObject = falseobj;
#endif
		break;

	case 6: /* return a block that the VM needs to run (or nil if non) */
		returnedObject = getNextVMBlockToRun();
		break;

	// TODO: Prim 7 no longer used. Prim 8 available

	case 7: /* reset a block that the VM needs to run */
	case 8:
		// VM incr when storing it
		// if (refCountField(vmBlockToRun) > 0) decr(vmBlockToRun);

		// if (vmBlockToRun != nilobj) {
		// 	decr(vmBlockToRun);
		// 	vmBlockToRun = nilobj;
		// }

		returnedObject = trueobj;
		break;

	case 9:
		exit(0);

	default: /* unknown primitive */
		sysError("unknown primitive", "zeroargPrims");
		break;
	}
	return (returnedObject);
}

object blockToExecute;
extern void doIt(char *evalText, object arg);
extern void runBlock(object block, object arg);

char *primString = { 0x0, 0x0 };

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

/*
 * Primitive 30 - 39 are platform independent 3 argument primitives
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

/*
 * Primitive 50 - 59 are platform independent Integer unary (1 arg) primitives
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

/*
 * Primitive 60 - 79 are platform independent Integer binary (2 arg) primitives
 */

static object intBinary(number, firstarg, secondarg) register int firstarg, secondarg;
int number;
{
    boolean binresult = false;
	long longresult;
	object returnedObject;

	switch (number)
	{
	case 0: /* addition */
		longresult = firstarg;
		longresult += secondarg;
		if (longCanBeInt(longresult))
			firstarg = (int) longresult;
		else
			goto overflow;
		break;
	case 1: /* subtraction */
		longresult = firstarg;
		longresult -= secondarg;
		if (longCanBeInt(longresult))
			firstarg = (int) longresult;
		else
			goto overflow;
		break;

	case 2: /* relationals */
		binresult = firstarg < secondarg;
		break;
	case 3:
		binresult = firstarg > secondarg;
		break;
	case 4:
		binresult = firstarg <= secondarg;
		break;
	case 5:
		binresult = firstarg >= secondarg;
		break;
	case 6:
	case 13:
		binresult = firstarg == secondarg;
		break;
	case 7:
		binresult = firstarg != secondarg;
		break;

	case 8: /* multiplication */
		longresult = firstarg;
		longresult *= secondarg;
		if (longCanBeInt(longresult))
			firstarg = (int) longresult;
		else
			goto overflow;
		break;

	case 9: /* quo: */
		if (secondarg == 0)
			goto overflow;
		firstarg /= secondarg;
		break;

	case 10: /* rem: */
		if (secondarg == 0)
			goto overflow;
		firstarg %= secondarg;
		break;

	case 11: /* bit operations */
		firstarg &= secondarg;
		break;

	case 12:
		firstarg ^= secondarg;
		break;

	case 19: /* shifts */
		if (secondarg < 0)
			firstarg >>= (-secondarg);
		else
			firstarg <<= secondarg;
		break;
	}
	if ((number >= 2) && (number <= 7))
		if (binresult)
			returnedObject = trueobj;
		else
			returnedObject = falseobj;
	else
		returnedObject = newInteger(firstarg);
	return (returnedObject);

	/* on overflow, return nil and let smalltalk code */
	/* figure out what to do */
overflow:
	returnedObject = nilobj;
	return (returnedObject);
}

/*
 * Primitive 80 - 89 are platform independent String unary (1 arg) primitives
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

/*
 * Primitive 100 - 109 are platform independent Float unary (1 arg) primitives
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

/*
 * Primitive 110 - 119 are platform independent Float unary (1 arg) primitives
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

/* primitive -
	the main driver for the primitive handler
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
