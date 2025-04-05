/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021

	Based on Little Smalltalk version 3
	Written by Tim Budd, Oregon State University, July 1988

	Bytecode Interpreter Module
	
	This module is the core of the Smalltalk execution engine. It:
	1. Takes a Smalltalk process object and executes its bytecodes
	2. Implements the low-level execution of the Smalltalk virtual machine
	3. Handles message sending, method lookup, and primitive execution
	4. Manages execution contexts and stack frames
	5. Provides support for ESP32-specific interrupts and time slicing
	
	Key components:
	- Method cache: A performance optimization that caches method lookups
	- Message sending: Implementation of the Smalltalk message passing system
	- Primitive execution: Bridge to directly executed C functions for performance
	- Process management: Time slicing and context switching between processes
	- Block execution: Support for Smalltalk blocks (closures)
	
	The interpreter is implemented as a large state machine that executes
	bytecodes one at a time until a time slice has ended or an interrupt occurs.
*/

#include <stdio.h>
#include "env.h"
#include "memory.h"
#include "names.h"
#include "interp.h"

void sysDecr(object z);
void flushCache(object messageToSend, object class);
object hashEachElement(object dict, register int hash, int (*fun)(void));
noreturn sysWarn(char *s1, char *s2);

object trueobj, falseobj;
boolean watching = 0;
extern object primitive(INT X OBJP);

/*
	the following variables are local to this module
*/

static object method, messageToSend;
boolean _interruptInterpreter = false;

static int messTest(obj)
	object obj;
{
	return obj == messageToSend;
}

/**
 * Method cache for fast method lookup
 * 
 * This cache significantly improves performance by avoiding method lookup
 * in the class hierarchy for repeated message sends to the same class.
 * The cache uses a simple hash function based on message and class to
 * determine the cache slot.
 * 
 * The cache size of 211 is a prime number, which helps distribute entries
 * more evenly across the cache, reducing collisions.
 */
#define cacheSize 211
static struct
{
	object cacheMessage; /* The message selector being sent */
	object lookupClass;	 /* The class of the receiver object */
	object cacheClass;	 /* The class where the method was found (may be a superclass) */
	object cacheMethod;	 /* The actual method object found */
} methodCache[cacheSize];

/**
 * Sets a flag to interrupt the interpreter
 * 
 * This function is used to interrupt the bytecode interpreter from outside
 * (e.g., after a delay task is completed or an external event occurs).
 * It's particularly important for the ESP32 implementation to handle
 * asynchronous events like timers, I/O completions, etc.
 * 
 * @return true if the flag was set, false if already set (which means
 *         the caller should wait until the current interrupt is handled)
 */

boolean interruptInterpreter()
{
	if (_interruptInterpreter)
		return false;
	_interruptInterpreter = true;
	return true;
}

/**
 * Flushes an entry from the method cache
 * 
 * This function is called when a method has been recompiled or modified,
 * to ensure the cached version is no longer used. It invalidates the
 * appropriate cache entry by setting its message to nil.
 * 
 * The cache uses a simple hash based on the message and class to
 * determine which entry to flush.
 *
 * @param messageToSend The message selector being sent
 * @param class The class to which the message is being sent
 */
void flushCache(messageToSend, class)
	object messageToSend,
	class;
{
	int hash;

	hash = (((int)messageToSend) + ((int)class)) / cacheSize;
	methodCache[hash].cacheMessage = nilobj;
}

/**
 * Finds a method in a class hierarchy
 * 
 * This function searches for a method matching the given message selector
 * starting from the specified class and proceeding up the inheritance
 * hierarchy. It sets the 'method' global variable when a match is found
 * and updates the methodClassLocation to point to the class where
 * the method was found (which may be a superclass).
 * 
 * This is a key part of the Smalltalk message dispatch system and implements
 * the inheritance-based method lookup.
 *
 * @param methodClassLocation Pointer to the starting class, updated to the matching class
 * @return true if a method was found, false otherwise
 */
static boolean findMethod(methodClassLocation)
	object *methodClassLocation;
{
	object methodTable, methodClass;

	method = nilobj;
	methodClass = *methodClassLocation;

	for (; methodClass != nilobj; methodClass =
									  basicAt(methodClass, superClassInClass))
	{
		methodTable = basicAt(methodClass, methodsInClass);
		method = hashEachElement(methodTable, messageToSend, messTest);
		if (method != nilobj)
			break;
	}

	if (method == nilobj)
	{ /* it wasn't found */
		methodClass = *methodClassLocation;
		return false;
	}

	*methodClassLocation = methodClass;
	return true;
}

/* Interpreter operation macros
 * These macros encapsulate common operations performed by the interpreter.
 * They handle reference counting, stack manipulation, and object access.
 * Using macros allows for more concise code in the interpreter loop while
 * properly maintaining the memory management invariants.
 */

/* Bytecode fetch - Gets the next bytecode and advances the pointer */
#define nextByte() *(bp + byteOffset++)

/* Stack operations - Handle pushing, accessing, and popping from the process stack */
#define ipush(x) incr(*++pst = (x))                  /* Push object onto stack with reference count increment */
#define stackTop() *pst                              /* Access object at top of stack */
#define stackTopPut(x) \
	decr((*pst));      \
	incr((*pst = x))                                 /* Replace top of stack with new object, maintaining ref counts */
#define stackTopFree() \
	decr((*pst));      \
	*pst-- = nilobj                                  /* Remove top of stack, decrementing ref count */
/* note that ipop leaves x with excess reference count */
#define ipop(x)     \
	x = stackTop(); \
	*pst-- = nilobj                                  /* Pop object into x, leaving x with excess reference count */
#define processStackTop() (int)((pst - psb) + 1)     /* Get current stack depth (1-based) */

/* Object field access - Access instance variables of various objects */
#define receiverAt(n) *(rcv + n)                     /* Access nth field of receiver object */
#define receiverAtPut(n, x) \
	decr(receiverAt(n));    \
	incr(receiverAt(n) = (x))                        /* Update nth field of receiver, maintaining ref counts */
#define argumentsAt(n) *(arg + n)                    /* Access nth argument */
#define temporaryAt(n) *(temps + n)                  /* Access nth temporary variable */
#define temporaryAtPut(n, x) \
	decr(temporaryAt(n));    \
	incr(temporaryAt(n) = (x))                       /* Update nth temporary, maintaining ref counts */
#define literalsAt(n) *(lits + n)                    /* Access nth literal from method's literal frame */
#define contextAt(n) *(cntx + n)                     /* Access nth field of current context */
#define contextAtPut(n, x) incr(contextAt(n - 1) = (x)) /* Update context field, maintaining ref counts */
#define processStackAt(n) *(psb + (n - 1))           /* Access nth stack slot (0-based array but 1-based access) */

/* the following are manipulated by primitives */
object processStack;
int linkPointer;

/**
 * Expands the process stack when it's close to capacity
 * 
 * This function is called when the process stack needs more space.
 * It creates a new, larger stack array and copies the contents of
 * the current stack into it. This is similar to how dynamic arrays
 * resize in other languages.
 *
 * The minimum growth is 100 slots regardless of the requested amount,
 * which helps reduce the frequency of resizing operations.
 *
 * @param top Current top position of the stack
 * @param toadd Minimum number of slots to add (at least 100 will be added)
 * @return The new, expanded stack object
 */
static object growProcessStack(top, toadd) int top, toadd;
{
	int size, i;
	object newStack;

	if (toadd < 100)
		toadd = 100;
	size = sizeField(processStack) + toadd;
	newStack = newArray(size);
	for (i = 1; i <= top; i++)
	{
		basicAtPut(newStack, i, basicAt(processStack, i));
	}
	return newStack;
}

/**
 * Main bytecode interpreter execution loop
 * 
 * This function is the heart of the Smalltalk VM. It executes bytecodes
 * from a Smalltalk process object, handling message sends, primitive calls,
 * context switches, and returns. The interpreter runs until either:
 * 1. The time slice expires (maxsteps reaches 0)
 * 2. An interrupt is triggered (via interruptInterpreter function)
 * 3. The process terminates naturally
 * 
 * The function uses several goto labels to manage control flow in different
 * parts of the interpreter, making it more efficient than using function calls:
 * - readLinkageBlock: Sets up variables from the process linkage area
 * - readMethodInfo: Prepares to execute a new method
 * - doSendMessage: Handles message sending to objects
 * - doFindMessage: Performs method lookup in a class hierarchy
 * - doReturn: Handles method returns
 * 
 * @param aProcess - The Smalltalk process object to execute
 * @param maxsteps - Maximum number of bytecodes to execute before yielding
 * @return true if the process should continue execution, false if it's complete
 */
boolean execute(object aProcess, int maxsteps)
{
	object returnedObject;
	int returnPoint, timeSliceCounter;
	object *pst, *psb, *rcv, *arg, *temps, *lits, *cntx;
	object contextObject, *primargs;
	int byteOffset;
	object methodClass, argarray;
	int i, j;
	register int low;
	int high;
	register object incrobj; /* Speed up increments and decrements */
	byte *bp;

	/* Unpack the instance variables from the process */
	processStack = basicAt(aProcess, stackInProcess);
	psb = sysMemPtr(processStack);
	j = intValue(basicAt(aProcess, stackTopInProcess));
	pst = psb + (j - 1);
	linkPointer = intValue(basicAt(aProcess, linkPtrInProcess));

	/* set the process time-slice counter before entering loop */
	timeSliceCounter = maxsteps;

	/* retrieve current values from the linkage area */
/* readLinkageBlock - Setup execution from process linkage area
 * This goto label extracts necessary state from the current linkage area in the process stack,
 * including context object, return point, bytecode offset, and method to be executed.
 * It sets up the pointers to these objects and prepares for method execution.
 * The linkage area contains:
 * 1. linkPointer+1: Context object (nil means using process stack)
 * 2. linkPointer+2: Return point (stack position to return to)
 * 3. linkPointer+3: Method object (if context is nil)
 * 4. linkPointer+4: Current bytecode offset in the method
 */
readLinkageBlock:
	contextObject = processStackAt(linkPointer + 1);
	returnPoint = intValue(processStackAt(linkPointer + 2));
	byteOffset = intValue(processStackAt(linkPointer + 4));
	if (contextObject == nilobj)
	{
		contextObject = processStack;
		cntx = psb;
		arg = cntx + (returnPoint - 1);
		method = processStackAt(linkPointer + 3);
		temps = cntx + linkPointer + 4;
	}
	else
	{ /* read from context object */
		cntx = sysMemPtr(contextObject);
		method = basicAt(contextObject, methodInContext);
		arg = sysMemPtr(basicAt(contextObject, argumentsInContext));
		temps = sysMemPtr(basicAt(contextObject, temporariesInContext));
	}

	if (!isInteger(argumentsAt(0)))
		rcv = sysMemPtr(argumentsAt(0));

/* readMethodInfo - Prepare to execute a method
 * This goto label prepares the VM to execute a method by:
 * 1. Setting up the literals array pointer from the method object
 * 2. Setting up the bytecode pointer from the method object
 * 3. Positioning the bytecode offset to start execution
 * After this setup, the interpreter enters its main bytecode execution loop
 */
readMethodInfo:
	lits = sysMemPtr(basicAt(method, literalsInMethod));
	bp = bytePtr(basicAt(method, bytecodesInMethod)) - 1;

	while (--timeSliceCounter > 0 && !_interruptInterpreter)
	{
		low = (high = nextByte()) & 0x0F;
		high >>= 4;
		if (high == 0)
		{
			high = low;
			low = nextByte();
		}
		/* Begin bytecode interpretation loop
         * Each bytecode is one or two bytes:
         * - Single byte: high nibble is opcode, low nibble is operand
         * - Two bytes: first byte's high and low nibbles are swapped, second byte is operand
         */
		switch (high)
		{

		case PushInstance:
			/* Push instance variable from receiver onto stack */
			ipush(receiverAt(low));
			break;

		case PushArgument:
			/* Push method argument onto stack */
			ipush(argumentsAt(low));
			break;

		case PushTemporary:
			/* Push temporary variable onto stack */
			ipush(temporaryAt(low));
			break;

		case PushLiteral:
			/* Push literal value from method's literal frame onto stack */
			ipush(literalsAt(low));
			break;

		case PushConstant:
			switch (low)
			{
			case 0:
			case 1:
			case 2:
				ipush(newInteger(low));
				break;

			case minusOne:
				ipush(newInteger(-1));
				break;

			case contextConst:
				/* check to see if we have made a block context yet */
				if (contextObject == processStack)
				{
					/* not yet, do it now - first get real return point */
					returnPoint =
						intValue(processStackAt(linkPointer + 2));
					contextObject =
						newContext(linkPointer, method,
								   copyFrom(processStack, returnPoint,
											linkPointer - returnPoint),
								   copyFrom(processStack, linkPointer + 5,
											methodTempSize(method)));
					basicAtPut(processStack, linkPointer + 1,
							   contextObject);
					ipush(contextObject);
					/* save byte pointer then restore things properly */
					fieldAtPut(processStack, linkPointer + 4,
							   newInteger(byteOffset));
					goto readLinkageBlock;
				}
				ipush(contextObject);
				break;

			case nilConst:
				ipush(nilobj);
				break;

			case trueConst:
				ipush(trueobj);
				break;

			case falseConst:
				ipush(falseobj);
				break;

			default:
				sysError("unimplemented constant", "pushConstant");
			}
			break;

		case AssignInstance:
			receiverAtPut(low, stackTop());
			break;

		case AssignTemporary:
			temporaryAtPut(low, stackTop());
			break;

		case MarkArguments:
			/* Prepare for message send by marking where arguments start on stack
			 * 'low' contains the argument count, and we calculate where the
			 * receiver (first argument) is located by subtracting argument count
			 * from the current stack top */
			returnPoint = (processStackTop() - low) + 1;
			timeSliceCounter++; /* make sure we do send */
			break;

		case SendMessage:
			/* Send a message with a literal selector and the previously marked arguments
			 * This implements normal method dispatch by:
			 * 1. Loading the message selector from literals
			 * 2. Using the goto to doSendMessage which handles method lookup and activation
			 */
			messageToSend = literalsAt(low);

		/* doSendMessage - Entry point for message sending implementation
		 * This goto label begins the process of sending a message:
		 * 1. Sets up the arguments array pointer
		 * 2. Gets the class of the receiver object
		 * 3. Proceeds to method lookup in that class
		 */
		doSendMessage:
			arg = psb + (returnPoint - 1);
			if (isInteger(argumentsAt(0)))
				/* should fix this later */
				methodClass = getClass(argumentsAt(0));
			else
			{
				rcv = sysMemPtr(argumentsAt(0));
				methodClass = classField(argumentsAt(0));
			}

		/* doFindMessage - Entry point for method lookup
		 * This goto label handles method lookup in the class hierarchy:
		 * 1. First checks the method cache for a hit
		 * 2. If not found, calls findMethod to search the class hierarchy
		 * 3. If still not found, sends the #doesNotUnderstand: message
		 * 4. Adds successful lookups to the method cache
		 */
		doFindMessage:
			/* look up method in cache */
			i = (((int)messageToSend) + ((int)methodClass)) % cacheSize;
			if ((methodCache[i].cacheMessage == messageToSend) &&
				(methodCache[i].lookupClass == methodClass))
			{
				method = methodCache[i].cacheMethod;
				methodClass = methodCache[i].cacheClass;
			}
			else
			{
				methodCache[i].lookupClass = methodClass;
				if (!findMethod(&methodClass))
				{
					/* not found, we invoke a smalltalk method */
					/* to recover */
					j = processStackTop() - returnPoint;
					argarray = newArray(j + 1);
					for (; j >= 0; j--)
					{
						ipop(returnedObject);
						basicAtPut(argarray, j + 1, returnedObject);
						decr(returnedObject);
					}
					ipush(basicAt(argarray, 1)); /* push receiver back */
					ipush(messageToSend);
					messageToSend =
						newSymbol("message:notRecognizedWithArguments:");
					ipush(argarray);
					/* try again - if fail really give up */
					if (!findMethod(&methodClass))
					{
						sysWarn("can't find method", "error recovery method");
						/* just quit */
						return false;
					}
				}
				methodCache[i].cacheMessage = messageToSend;
				methodCache[i].cacheMethod = method;
				methodCache[i].cacheClass = methodClass;
			}

			if (watching && (basicAt(method, watchInMethod) != nilobj))
			{
				/* being watched, we send to method itself */
				j = processStackTop() - returnPoint;
				argarray = newArray(j + 1);
				for (; j >= 0; j--)
				{
					ipop(returnedObject);
					basicAtPut(argarray, j + 1, returnedObject);
					decr(returnedObject);
				}
				ipush(method); /* push method */
				ipush(argarray);
				messageToSend = newSymbol("watchWith:");
				/* try again - if fail really give up */
				methodClass = classField(method);
				if (!findMethod(&methodClass))
				{
					sysWarn("can't find", "watch method");
					/* just quit */
					return false;
				}
			}

			/* save the current byte pointer */
			fieldAtPut(processStack, linkPointer + 4,
					   newInteger(byteOffset));

			/* make sure we have enough room in current process */
			/* stack, if not make stack larger */
			i = 6 + methodTempSize(method) + methodStackSize(method);
			j = processStackTop();
			if ((j + i) > sizeField(processStack))
			{
				processStack = growProcessStack(j, i);
				psb = sysMemPtr(processStack);
				pst = (psb + j);
				fieldAtPut(aProcess, stackInProcess, processStack);
			}

			byteOffset = 1;
			/* now make linkage area */
			/* position 0 : old linkage pointer */
			ipush(newInteger(linkPointer));
			linkPointer = processStackTop();
			/* position 1 : context object (nil means stack) */
			ipush(nilobj);
			contextObject = processStack;
			cntx = psb;
			/* position 2 : return point */
			ipush(newInteger(returnPoint));
			arg = cntx + (returnPoint - 1);
			/* position 3 : method */
			ipush(method);
			/* position 4 : bytecode counter */
			ipush(newInteger(byteOffset));
			/* then make space for temporaries */
			temps = pst + 1;
			pst += methodTempSize(method);
			/* break if we are too big and probably looping */
			if (sizeField(processStack) > 1800)
				timeSliceCounter = 0;
			goto readMethodInfo;

		case SendUnary:
			/* do isNil and notNil as special cases, since */
			/* they are so common */
			if ((!watching) && (low <= 1))
			{
				if (stackTop() == nilobj)
				{
					stackTopPut((low ? falseobj : trueobj));
					break;
				}
			}
			returnPoint = processStackTop();
			messageToSend = unSyms[low];
			goto doSendMessage;
			break;

		case SendBinary:
			/* optimized as long as arguments are int */
			/* and conversions are not necessary */
			/* and overflow does not occur */
			if ((!watching) && (low <= 12))
			{
				primargs = pst - 1;
				returnedObject = primitive(low + 60, primargs);
				if (returnedObject != nilobj)
				{
					/* pop arguments off stack , push on result */
					stackTopFree();
					stackTopPut(returnedObject);
					break;
				}
			}
			/* else we do it the old fashion way */
			returnPoint = processStackTop() - 1;
			messageToSend = binSyms[low];
			goto doSendMessage;

		case DoPrimitive:
			/* low gives number of arguments */
			/* next byte is primitive number */
			primargs = (pst - low) + 1;
			/* next byte gives primitive number */
			i = nextByte();
			/* a few primitives are so common, and so easy, that
	       they deserve special treatment */
			switch (i)
			{
			case 5: /* set watch */
				watching = !watching;
				returnedObject = watching ? trueobj : falseobj;
				break;

			case 11: /* class of object */
				returnedObject = getClass(*primargs);
				break;
			case 21: /* object equality test */
				if (*primargs == *(primargs + 1))
					returnedObject = trueobj;
				else
					returnedObject = falseobj;
				break;
			case 25: /* basicAt: */
				j = intValue(*(primargs + 1));
				returnedObject = basicAt(*primargs, j);
				break;
			case 31: /* basicAt:Put: */
				j = intValue(*(primargs + 1));
				fieldAtPut(*primargs, j, *(primargs + 2));
				returnedObject = nilobj;
				break;
			case 53: /* set time slice */
				timeSliceCounter = intValue(*primargs);
				returnedObject = nilobj;
				break;
			case 58: /* allocObject */
				j = intValue(*primargs);
				returnedObject = allocObject(j);
				break;
			case 87: /* value of symbol */
				returnedObject = globalSymbol(charPtr(*primargs));
				break;
			default:
				returnedObject = primitive(i, primargs);
				break;
			}
			/* increment returned object in case pop would destroy it */
			incr(returnedObject);
			/* pop off arguments */
			while (low-- > 0)
			{
				stackTopFree();
			}
			/* returned object has already been incremented */
			ipush(returnedObject);
			decr(returnedObject);
			break;

		/* doReturn - Entry point for method return handling
		 * This goto label handles returning from a method:
		 * 1. Restores the previous context's linkage information
		 * 2. Removes everything from the stack down to the return point
		 * 3. Pushes the returned object onto the stack
		 * 4. Either returns to the calling context or exits the interpreter
		 */
		doReturn:
			returnPoint = intValue(basicAt(processStack, linkPointer + 2));
			linkPointer = intValue(basicAt(processStack, linkPointer));
			while (processStackTop() >= returnPoint)
			{
				stackTopFree();
			}
			/* returned object has already been incremented */
			ipush(returnedObject);
			decr(returnedObject);
			/* now go restart old routine */
			if (linkPointer != nilobj)
				goto readLinkageBlock;
			else
				return false /* all done */;

		case DoSpecial:
			switch (low)
			{
			case SelfReturn:
				incr(returnedObject = argumentsAt(0));
				goto doReturn;

			case StackReturn:
				ipop(returnedObject);
				goto doReturn;

			case Duplicate:
				/* avoid possible subtle bug */
				returnedObject = stackTop();
				ipush(returnedObject);
				break;

			case PopTop:
				ipop(returnedObject);
				decr(returnedObject);
				break;

			case Branch:
				/* avoid a subtle bug here */
				i = nextByte();
				byteOffset = i;
				break;

			case BranchIfTrue:
				ipop(returnedObject);
				i = nextByte();
				if (returnedObject == trueobj)
				{
					/* leave nil on stack */
					pst++;
					byteOffset = i;
				}
				decr(returnedObject);
				break;

			case BranchIfFalse:
				ipop(returnedObject);
				i = nextByte();
				if (returnedObject == falseobj)
				{
					/* leave nil on stack */
					pst++;
					byteOffset = i;
				}
				decr(returnedObject);
				break;

			case AndBranch:
				ipop(returnedObject);
				i = nextByte();
				if (returnedObject == falseobj)
				{
					ipush(returnedObject);
					byteOffset = i;
				}
				decr(returnedObject);
				break;

			case OrBranch:
				ipop(returnedObject);
				i = nextByte();
				if (returnedObject == trueobj)
				{
					ipush(returnedObject);
					byteOffset = i;
				}
				decr(returnedObject);
				break;

			case SendToSuper:
				i = nextByte();
				messageToSend = literalsAt(i);
				rcv = sysMemPtr(argumentsAt(0));
				methodClass = basicAt(method, methodClassInMethod);
				/* if there is a superclass, use it
		   otherwise for class Object (the only 
		   class that doesn't have a superclass) use
		   the class again */
				returnedObject = basicAt(methodClass, superClassInClass);
				if (returnedObject != nilobj)
					methodClass = returnedObject;
				goto doFindMessage;

			default:
				sysError("invalid doSpecial", "");
				break;
			}
			break;

		default:
			sysError("invalid bytecode", "");
			break;
		}

	} // interpreter while loop end

	_interruptInterpreter = false;

	/* before returning we put back the values in the current process */
	/* object */

	fieldAtPut(processStack, linkPointer + 4, newInteger(byteOffset));
	fieldAtPut(aProcess, stackTopInProcess, newInteger(processStackTop()));
	fieldAtPut(aProcess, linkPtrInProcess, newInteger(linkPointer));

	return true;
}
