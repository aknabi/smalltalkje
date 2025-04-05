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
 * Each cache entry stores:
 * - cacheMessage: The message selector (method name) being sent
 * - lookupClass: The class of the receiver object
 * - cacheClass: The class where the method was found (may be a superclass)
 * - cacheMethod: The actual method object that was found
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
 * 
 * Reference counting is critical to the memory management:
 * - incr(): Increments the reference count of an object
 * - decr(): Decrements the reference count and potentially garbage collects
 */

/* Bytecode fetch - Gets the next bytecode and advances the instruction pointer */
#define nextByte() *(bp + byteOffset++)

/* Stack operations - Handle pushing, accessing, and popping from the process stack */
#define ipush(x) incr(*++pst = (x))                  /* Push object onto stack with reference count increment */
#define stackTop() *pst                              /* Access object at top of stack without changing ref count */
#define stackTopPut(x) \
	decr((*pst));      \
	incr((*pst = x))                                 /* Replace top of stack with new object, maintaining ref counts */
#define stackTopFree() \
	decr((*pst));      \
	*pst-- = nilobj                                  /* Remove top of stack, decrementing ref count and moving pointer down */
/* note that ipop leaves x with excess reference count - caller must handle this */
#define ipop(x)     \
	x = stackTop(); \
	*pst-- = nilobj                                  /* Pop object into x, leaving x with excess reference count */
#define processStackTop() (int)((pst - psb) + 1)     /* Get current stack depth (1-based) */

/* Object field access - These macros provide controlled access to object fields
 * with proper reference counting. The memory model requires incrementing
 * reference counts when storing references and decrementing when removing them. */
#define receiverAt(n) *(rcv + n)                     /* Access nth field of receiver object */
#define receiverAtPut(n, x) \
	decr(receiverAt(n));    \
	incr(receiverAt(n) = (x))                        /* Update nth field of receiver, maintaining ref counts */
#define argumentsAt(n) *(arg + n)                    /* Access nth argument from method arguments */
#define temporaryAt(n) *(temps + n)                  /* Access nth temporary variable in method context */
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
 * context switches, and returns. The interpreter operates as a large state
 * machine that processes one bytecode at a time.
 * 
 * Execution Model:
 * 1. The function initializes its state from the process object, extracting:
 *    - The process stack (where execution state is stored)
 *    - The current stack top pointer
 *    - The current linkage pointer (pointing to current method's activation record)
 * 
 * 2. It runs in a loop, executing bytecodes until one of these conditions:
 *    - The time slice expires (maxsteps reaches 0) - enables cooperative multitasking
 *    - An interrupt is triggered (via interruptInterpreter function) - for async events
 *    - The process terminates naturally (returns from the top-level method)
 * 
 * 3. Each bytecode is decoded and executed, potentially:
 *    - Manipulating values on the stack
 *    - Modifying object state
 *    - Looking up and activating methods via message sends
 *    - Handling control flow (conditionals, loops, returns)
 *    - Executing primitive operations (direct C function calls)
 * 
 * Unique Features:
 * - Method Cache: The VM uses a cache to speed up method lookup
 * - Object Model: All values are objects, including integers and booleans
 * - Reference Counting: Memory management via reference counting
 * - Time Slicing: Cooperative multitasking via bytecode counting
 * - Block Closures: Support for first-class functions with lexical scope
 * 
 * Flow Control:
 * This function uses several goto labels to manage control flow efficiently:
 * - readLinkageBlock: Sets up variables from the process linkage area
 * - readMethodInfo: Prepares to execute a new method's bytecodes
 * - doSendMessage: Handles message sending to objects
 * - doFindMessage: Performs method lookup in a class hierarchy
 * - doReturn: Handles method returns to calling contexts
 * 
 * @param aProcess - The Smalltalk process object to execute
 * @param maxsteps - Maximum number of bytecodes to execute before yielding (time slice)
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
 * 
 * The linkage area is a region in the process stack that contains execution context:
 * 1. linkPointer+0: Previous linkage pointer (points to caller's linkage area)
 * 2. linkPointer+1: Context object (nil means using process stack directly)
 * 3. linkPointer+2: Return point (stack position to return to upon method completion)
 * 4. linkPointer+3: Method object (current method being executed if context is nil)
 * 5. linkPointer+4: Current bytecode offset in the method (instruction pointer)
 * 
 * This structure forms the call stack of the Smalltalk VM, enabling method returns
 * and maintaining execution state across method calls.
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
 * 
 * The method object contains:
 * - literalsInMethod: Array of literal values used by the method (e.g., constants, symbols)
 * - bytecodesInMethod: The actual bytecode instructions to execute
 * 
 * After this setup, the interpreter enters its main bytecode execution loop
 * which processes one bytecode at a time until the time slice ends or an interrupt occurs.
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
         * - Single byte format: high nibble is opcode, low nibble is operand
         * - Two byte format: first byte's high nibble is 0, low nibble is opcode,
         *                   second byte is the full operand (for operations needing larger operands)
         * 
         * The switch statement below handles each bytecode operation type (high/opcode):
         * - PushInstance, PushArgument, etc.: Load values onto the stack
         * - AssignInstance, AssignTemporary: Store values from stack to variables
         * - SendMessage, SendUnary, SendBinary: Dispatch method calls
         * - DoPrimitive: Execute native C functions for performance
         * - DoSpecial: Handle control flow, returns, and other special operations
         */
		switch (high)
		{

		case PushInstance:
			/* Push instance variable from receiver onto stack
			 * The 'low' value is the index of the instance variable in the receiver object.
			 * This opcode implements accessing an object's state, equivalent to "self instVar" in Smalltalk.
			 */
			ipush(receiverAt(low));
			break;

		case PushArgument:
			/* Push method argument onto stack
			 * The 'low' value is the index in the arguments array.
			 * Index 0 is the receiver itself, index 1+ are the actual arguments.
			 */
			ipush(argumentsAt(low));
			break;

		case PushTemporary:
			/* Push temporary variable onto stack
			 * The 'low' value is the index in the temporaries array.
			 * Temporaries are method-local variables used for intermediate calculations.
			 */
			ipush(temporaryAt(low));
			break;

		case PushLiteral:
			/* Push literal value from method's literal frame onto stack
			 * The 'low' value is the index in the literals array.
			 * Literals include constants, symbols, and other values defined in the method.
			 */
			ipush(literalsAt(low));
			break;

		case PushConstant:
			/* Push constant values onto the stack
			 * This opcode handles various constant values that are commonly used:
			 * - Small integers (0, 1, 2, -1)
			 * - Boolean values (true, false)
			 * - nil
			 * - Block context (for closures/blocks)
			 */
			switch (low)
			{
			case 0:
			case 1:
			case 2:
				/* Push small integers 0, 1, or 2 directly onto the stack
				 * These are very common constants, so they have dedicated opcodes
				 */
				ipush(newInteger(low));
				break;

			case minusOne:
				/* Push -1 onto the stack
				 * Another common integer constant with a dedicated opcode
				 */
				ipush(newInteger(-1));
				break;

			case contextConst:
				/* Push the current execution context onto the stack
				 * This is used for block closures in Smalltalk ([ ... ] blocks)
				 * 
				 * If we haven't created a block context yet (contextObject == processStack),
				 * we need to create a proper Context object that captures:
				 * 1. The current method's environment (linkPointer)
				 * 2. The method being executed
				 * 3. The arguments from the stack
				 * 4. The temporary variables
				 * 
				 * This implements lexical closure - the block can access variables
				 * from its enclosing method scope even after that method has returned.
				 */
				if (contextObject == processStack)
				{
					/* not yet, do it now - first get real return point */
					returnPoint =
						intValue(processStackAt(linkPointer + 2));
					/* Create a new context object with:
					 * - The current linkage pointer (for accessing outer scope)
					 * - The current method
					 * - A copy of the arguments from the stack
					 * - A copy of the temporary variables
					 */
					contextObject =
						newContext(linkPointer, method,
								   copyFrom(processStack, returnPoint,
											linkPointer - returnPoint),
								   copyFrom(processStack, linkPointer + 5,
											methodTempSize(method)));
					/* Update the linkage area to point to this new context */
					basicAtPut(processStack, linkPointer + 1,
							   contextObject);
					ipush(contextObject);
					/* Save the current bytecode position then restore execution state */
					fieldAtPut(processStack, linkPointer + 4,
							   newInteger(byteOffset));
					goto readLinkageBlock;
				}
				ipush(contextObject);
				break;

			case nilConst:
				/* Push the nil object onto the stack
				 * nil represents the absence of a value in Smalltalk
				 */
				ipush(nilobj);
				break;

			case trueConst:
				/* Push the true object onto the stack
				 * true is a singleton object in Smalltalk representing boolean truth
				 */
				ipush(trueobj);
				break;

			case falseConst:
				/* Push the false object onto the stack
				 * false is a singleton object in Smalltalk representing boolean falsity
				 */
				ipush(falseobj);
				break;

			default:
				/* Unknown constant type - report an error */
				sysError("unimplemented constant", "pushConstant");
			}
			break;

		case AssignInstance:
			/* Assign/store to an instance variable of the receiver
			 * The 'low' value is the index of the instance variable
			 * This opcode implements "self instVar: value" assignments in Smalltalk
			 * The value to be assigned is on top of the stack
			 */
			receiverAtPut(low, stackTop());
			break;

		case AssignTemporary:
			/* Assign/store to a temporary variable
			 * The 'low' value is the index of the temporary variable
			 * This opcode implements "tempVar := value" assignments for method locals in Smalltalk
			 * The value to be assigned is on top of the stack
			 */
			temporaryAtPut(low, stackTop());
			break;

		case MarkArguments:
			/* Prepare for message send by marking where arguments start on stack
			 * 'low' contains the argument count (not including the receiver)
			 * 
			 * This opcode typically precedes a SendMessage opcode. It calculates the
			 * stack position where the receiver is located by subtracting the argument
			 * count from the current stack top. This value is stored in returnPoint
			 * and will be used by the subsequent SendMessage opcode.
			 * 
			 * For example, for a message send like "obj method: arg1 with: arg2":
			 * - Stack would contain [obj, arg1, arg2] (top)
			 * - low would be 2 (two arguments)
			 * - returnPoint would be set to point to obj's position
			 */
			returnPoint = (processStackTop() - low) + 1;
			timeSliceCounter++; /* make sure we have enough time slice left to do the send */
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
		 * 1. Sets up the arguments array pointer to access receiver and arguments
		 * 2. Gets the class of the receiver object by checking its type:
		 *    - For integers, gets their special class (e.g., SmallInteger)
		 *    - For non-integers, gets the class from their class field
		 * 3. Proceeds to method lookup in that class via doFindMessage
		 * 
		 * This implements the core of Smalltalk's dynamic dispatch system where
		 * messages are sent to objects and the appropriate method is looked up
		 * in the object's class hierarchy at runtime.
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
		 * 1. First checks the method cache for a hit using a hash of message and class
		 * 2. If not found in cache, calls findMethod to search the class hierarchy
		 * 3. If still not found, creates an arguments array and sends the
		 *    #message:notRecognizedWithArguments: message (Smalltalk's version of
		 *    "method not found" exception handling)
		 * 4. Adds successful lookups to the method cache for future performance
		 * 5. Handles method watching (debugging) if enabled for the found method
		 * 
		 * Once the method is found, the interpreter:
		 * 1. Saves the current bytecode pointer in the linkage area
		 * 2. Ensures enough room in the process stack for the new method activation
		 * 3. Creates a new linkage area for the method being called
		 * 4. Sets up the method for execution and jumps to readMethodInfo
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
				/* Optimize common unary messages isNil and notNil
				 * - low=0: isNil  (returns true if receiver is nil, false otherwise)
				 * - low=1: notNil (returns false if receiver is nil, true otherwise)
				 * 
				 * This optimization bypasses the normal message send machinery
				 * for these frequently used methods when they're called on nil.
				 */
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
			/* Optimized binary operations for primitive types
			 * This is a fast path for common binary operations (like +, -, <, >, etc.)
			 * that can be performed directly when:
			 * - Arguments are of the right type (typically integers)
			 * - No type conversions are necessary
			 * - No overflow occurs
			 * 
			 * If any of these conditions fail, we fall back to the normal
			 * message sending mechanism via the "else" path below.
			 */
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
			/* Fast path for common primitives
			 * These primitives are so frequently used and so simple that they're
			 * implemented directly in the interpreter loop rather than calling
			 * the general primitive function. This improves performance for
			 * these critical operations:
			 * 
			 * 5: Toggle watching (debugging) mode
			 * 11: Get class of an object
			 * 21: Test object equality
			 * 25: Basic array/object indexing (basicAt:)
			 * 31: Basic array/object update (basicAt:put:)
			 * 53: Set time slice counter for the interpreter
			 * 58: Allocate a new object
			 * 87: Lookup a global value by symbol name
			 */
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
			/* Handle method return
			 * This code implements returning from a method by:
			 * 1. Retrieving the return point from the linkage area (where to return to)
			 * 2. Getting the previous linkage pointer (restoring the call stack)
			 * 3. Popping values off the stack down to the return point, cleaning up
			 *    any values that were on the stack
			 * 4. Pushing the returned value onto the stack for the caller
			 * 5. Either:
			 *    - Returning to the caller by jumping to readLinkageBlock if there is one
			 *    - Or returning false from execute() if we've reached the bottom of the call stack
			 */
			returnPoint = intValue(basicAt(processStack, linkPointer + 2));
			linkPointer = intValue(basicAt(processStack, linkPointer));
			/* Pop and decrement everything on the stack down to the return point */
			while (processStackTop() >= returnPoint)
			{
				stackTopFree();
			}
			/* Push the returned object onto the stack for the caller
			 * Note: returnedObject has already been incremented by the caller */
			ipush(returnedObject);
			decr(returnedObject);
			/* Now restart the caller or exit if we're at the bottom of the call stack */
			if (linkPointer != nilobj)
				goto readLinkageBlock;
			else
				return false /* all done with this process */;

		case DoSpecial:
			switch (low)
			{
			case SelfReturn:
				/* Return the receiver (self) from the method
				 * This implements "^self" in Smalltalk code.
				 * We increment the reference count of the receiver before
				 * jumping to doReturn where it will be properly handled.
				 */
				incr(returnedObject = argumentsAt(0));
				goto doReturn;

			case StackReturn:
				/* Return the top value from the stack
				 * This implements "^expression" in Smalltalk code.
				 * We pop the value into returnedObject (with increased ref count)
				 * and jump to doReturn where it will be properly handled.
				 */
				ipop(returnedObject);
				goto doReturn;

			case Duplicate:
				/* Duplicate the top stack value
				 * This creates a second reference to the top value,
				 * which is needed for operations that consume values
				 * but where the original is still needed.
				 */
				returnedObject = stackTop();
				ipush(returnedObject);
				break;

			case PopTop:
				/* Discard the top value from the stack
				 * This implements statement sequences in Smalltalk,
				 * where intermediate results need to be discarded.
				 * We pop the value and explicitly decrement its reference count.
				 */
				ipop(returnedObject);
				decr(returnedObject);
				break;

			case Branch:
				/* Unconditional branch to another bytecode position
				 * This implements control flow like method endings and loops.
				 * The target bytecode offset is in the next byte.
				 */
				i = nextByte();
				byteOffset = i;
				break;

			case BranchIfTrue:
				/* Conditional branch if top of stack is true
				 * This implements "ifTrue:" blocks in Smalltalk.
				 * 1. Pop the condition from the stack
				 * 2. If it's the true object, branch to the target bytecode
				 * 3. If branching, push nil on stack as the result of the condition
				 */
				ipop(returnedObject);
				i = nextByte();
				if (returnedObject == trueobj)
				{
					/* leave nil on stack as the result of the condition */
					pst++;
					byteOffset = i;
				}
				decr(returnedObject);
				break;

			case BranchIfFalse:
				/* Conditional branch if top of stack is false
				 * This implements "ifFalse:" blocks in Smalltalk.
				 * 1. Pop the condition from the stack
				 * 2. If it's the false object, branch to the target bytecode
				 * 3. If branching, push nil on stack as the result of the condition
				 */
				ipop(returnedObject);
				i = nextByte();
				if (returnedObject == falseobj)
				{
					/* leave nil on stack as the result of the condition */
					pst++;
					byteOffset = i;
				}
				decr(returnedObject);
				break;

			case AndBranch:
				/* Short-circuit AND operation
				 * This implements "and:" in Smalltalk with short-circuit evaluation:
				 * 1. Pop the left operand from the stack
				 * 2. If it's false, we can skip evaluating the right operand:
				 *    - Push false back as the result
				 *    - Branch to skip the right operand evaluation
				 * 3. If it's true, we continue to the next bytecode to evaluate the right operand
				 */
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
				/* Short-circuit OR operation
				 * This implements "or:" in Smalltalk with short-circuit evaluation:
				 * 1. Pop the left operand from the stack
				 * 2. If it's true, we can skip evaluating the right operand:
				 *    - Push true back as the result
				 *    - Branch to skip the right operand evaluation
				 * 3. If it's false, we continue to the next bytecode to evaluate the right operand
				 */
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
				/* Send message to superclass implementation (super send)
				 * This implements "super message" sends in Smalltalk:
				 * 1. Get the message selector from the literals array
				 * 2. Set up the receiver
				 * 3. Get the method's class (where the method is defined)
				 * 4. Find the superclass of that class
				 * 5. Start method lookup from the superclass instead of the receiver's class
				 * 
				 * This mechanism allows a method to call the implementation of the same message
				 * in its superclass, bypassing its own class's implementation.
				 */
				i = nextByte();
				messageToSend = literalsAt(i);
				rcv = sysMemPtr(argumentsAt(0));
				methodClass = basicAt(method, methodClassInMethod);
				/* If there is a superclass, use it.
				 * Otherwise, for class Object (the only class that doesn't 
				 * have a superclass), use the class itself. */
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

	/* Reset the interrupt flag so it can be used again for future interrupts */
	_interruptInterpreter = false;

	/* Before returning, save the current execution state back to the process object.
	 * This is critical for the time-slicing mechanism, as it allows the process
	 * to be resumed later from exactly where it left off. We store:
	 * 
	 * 1. The current bytecode position (byteOffset) in the linkage area
	 * 2. The current stack top position in the process object
	 * 3. The current linkage pointer in the process object
	 * 
	 * These values effectively capture the complete execution state of the process,
	 * enabling the cooperative multitasking system to switch between processes
	 * and resume each one exactly where it left off.
	 */
	fieldAtPut(processStack, linkPointer + 4, newInteger(byteOffset));
	fieldAtPut(aProcess, stackTopInProcess, newInteger(processStackTop()));
	fieldAtPut(aProcess, linkPtrInProcess, newInteger(linkPointer));

	/* Return true to indicate the process is still active and should be
	 * scheduled again. A return value of false (which happens when a process
	 * completes its top-level method) indicates the process has terminated
	 * and should be removed from the scheduler.
	 */
	return true;
}
