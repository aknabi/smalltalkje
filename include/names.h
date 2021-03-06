/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021
	Based on:

	Little Smalltalk, version 2
	Written by Tim Budd, Oregon State University, July 1987
*/
/*
	names and sizes of internally object used internally in the system
*/

#define classSize 5 	/* Class object - 5 instance variables */
#define nameInClass 1
#define sizeInClass 2
#define methodsInClass 3
#define superClassInClass 4
#define variablesInClass 5

#define methodSize 8
#define textInMethod 1
#define messageInMethod 2
#define bytecodesInMethod 3
#define literalsInMethod 4
#define stackSizeInMethod 5
#define temporarySizeInMethod 6
#define methodClassInMethod 7
#define watchInMethod 8
#define methodStackSize(x) intValue(basicAt(x, stackSizeInMethod))
#define methodTempSize(x) intValue(basicAt(x, temporarySizeInMethod))

#define contextSize 6
#define linkPtrInContext 1
#define methodInContext 2
#define argumentsInContext 3
#define temporariesInContext 4

#define blockSize 6
#define contextInBlock 1
#define argumentCountInBlock 2
#define argumentLocationInBlock 3
#define bytecountPositionInBlock 4

#define processSize 3
#define stackInProcess 1
#define stackTopInProcess 2
#define linkPtrInProcess 3

extern object trueobj;	/* the pseudo variable true */
extern object falseobj; /* the pseudo variable false */

/**
 * @brief Get the class object of the object
 *
 * @param object an instance object to return the class for
 * 
 * @return  An object which is the class object of the receiver.
 * 
 */
extern object getClass(object);

/**
 * @brief Copy elements of an array
 *
 * @param object Array to copy from
 * @param start Start index to copy elements from (Smalltalk 1 based)
 * @param size Number of elements to copy
 * 
 * @return  A new array object with the copied elements.
 * 
 */
extern object copyFrom(object obj, int start, int size);

extern object newArray(INT);
extern object newBlock();
extern object newByteArray(INT);
extern object newClass(STR);
extern object newChar(INT);
extern object newContext(INT X OBJ X OBJ X OBJ);
extern object newDictionary(INT);
extern object newFloat(FLOAT);
extern object newMethod();
extern object newLink(OBJ X OBJ);
extern object newStString(STR);
extern object newSymbol(STR);

extern object shallowCopy(OBJ);

extern double floatValue(OBJ);

extern noreturn initCommonSymbols(); /* common symbols */
noreturn readObjectFiles(FILEP, FILEP);
extern object unSyms[], binSyms[];

extern noreturn nameTableInsert(OBJ X INT X OBJ X OBJ);
extern int strHash(STR);
extern object globalKey(STR);
extern object nameTableLookup(OBJ X STR);
extern object findClass(STR);

#define globalSymbol(s) nameTableLookup(symbols, s)

#define isClassNameEqual(c, s) (c == globalSymbol(s))
#define isObjectOfClassName(o, s) isClassNameEqual(getClass(o), s)
