/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021
	Based on:

	Little Smalltalk, version 2
	Written by Tim Budd, Oregon State University, July 1987

	Names Header
	
	This header defines the internal structure of the core object types in the
	Smalltalk system. It specifies the layout of instance variables within each
	type of object and provides constants for accessing these variables.
	
	Key object types defined here include:
	- Class objects: Definition of classes in the system
	- Methods: Executable code containers
	- Contexts: Method activation records (stack frames)
	- Blocks: Closure objects for deferred execution
	- Processes: Execution threads
	
	The file also declares functions for creating and manipulating these objects,
	as well as utilities for name lookup, symbol table management, and common
	object access patterns.
*/
/**
 * Object Structure Definitions
 * 
 * These constants define the internal structure of core Smalltalk objects.
 * Each object type has a defined size (number of instance variables) and
 * named offsets for accessing those variables.
 * 
 * The naming convention is:
 * - [name]Size: Total number of instance variables in the object
 * - [variableName]In[ObjectType]: Index of a specific instance variable
 * 
 * All indices are 1-based to match Smalltalk's convention.
 */

/**
 * Class Object Structure
 * 
 * Class objects contain the definition of a class in the system.
 * They hold information about the name, size, methods, superclass,
 * and instance variables of a class.
 */
#define classSize 5 	        /* Total number of instance variables in a Class object */
#define nameInClass 1           /* Index of the class name (a Symbol) */
#define sizeInClass 2           /* Index of the instance size (number of instance variables) */
#define methodsInClass 3        /* Index of the methods dictionary */
#define superClassInClass 4     /* Index of the superclass reference */
#define variablesInClass 5      /* Index of the array of instance variable names */

/**
 * Method Object Structure
 * 
 * Method objects contain the executable code and metadata for a method.
 * They include the source text, message pattern, bytecodes, literals,
 * and information about stack and temporary variable usage.
 */
#define methodSize 8                /* Total number of instance variables in a Method object */
#define textInMethod 1              /* Index of the source code text */
#define messageInMethod 2           /* Index of the message pattern (selector) */
#define bytecodesInMethod 3         /* Index of the compiled bytecodes (ByteArray) */
#define literalsInMethod 4          /* Index of the literals array used by the method */
#define stackSizeInMethod 5         /* Index of the maximum stack size needed */
#define temporarySizeInMethod 6     /* Index of the number of temporary variables */
#define methodClassInMethod 7       /* Index of the class this method belongs to */
#define watchInMethod 8             /* Index of the watch flag (for debugging) */

/* Convenience macros for accessing method properties */
#define methodStackSize(x) intValue(basicAt(x, stackSizeInMethod))    /* Get method's stack size */
#define methodTempSize(x) intValue(basicAt(x, temporarySizeInMethod)) /* Get method's temp variable count */

/**
 * Context Object Structure
 * 
 * Context objects represent method activations (stack frames).
 * They contain links to the calling context, the executing method,
 * and storage for arguments and temporary variables.
 */
#define contextSize 6               /* Total number of instance variables in a Context object */
#define linkPtrInContext 1          /* Index of the link to caller's context */
#define methodInContext 2           /* Index of the currently executing method */
#define argumentsInContext 3        /* Index of the array of arguments */
#define temporariesInContext 4      /* Index of the array of temporary variables */

/**
 * Block Object Structure
 * 
 * Block objects represent closures (code + lexical environment).
 * They contain a reference to their defining context, argument information,
 * and the bytecode position where their code begins.
 */
#define blockSize 6                  /* Total number of instance variables in a Block object */
#define contextInBlock 1             /* Index of the defining context (lexical scope) */
#define argumentCountInBlock 2       /* Index of the number of arguments */
#define argumentLocationInBlock 3    /* Index of the location where arguments are stored */
#define bytecountPositionInBlock 4   /* Index of the starting bytecode position */

/**
 * Process Object Structure
 * 
 * Process objects represent execution threads in the Smalltalk system.
 * They contain a stack for execution, a pointer to the stack top,
 * and a link pointer for context management.
 */
#define processSize 3                /* Total number of instance variables in a Process object */
#define stackInProcess 1             /* Index of the process stack (Array) */
#define stackTopInProcess 2          /* Index of the current stack top position */
#define linkPtrInProcess 3           /* Index of the current link pointer (for context) */

/**
 * Special Object References
 * 
 * These external references point to important singleton objects
 * in the Smalltalk system. They correspond to the true and false
 * pseudo-variables in Smalltalk code.
 */
extern object trueobj;	/* The true object (singleton instance of True) */
extern object falseobj; /* The false object (singleton instance of False) */

/**
 * Core Object Operations
 * 
 * These functions provide fundamental operations on Smalltalk objects.
 */

/**
 * Get the class object of the object
 *
 * Returns the class of any Smalltalk object, handling special cases for
 * integers and other primitive types that don't have direct class fields.
 *
 * @param object An instance object to return the class for
 * @return The class object of the receiver
 */
extern object getClass(object);

/**
 * Copy elements of an array
 *
 * Creates a new array containing a subset of elements from the source array.
 * This is used for various operations including block creation (for capturing variables).
 *
 * @param obj Array to copy from
 * @param start Start index to copy elements from (Smalltalk 1-based)
 * @param size Number of elements to copy
 * @return A new array object with the copied elements
 */
extern object copyFrom(object obj, int start, int size);

/**
 * Object Creation Functions
 * 
 * These functions create new instances of various Smalltalk classes.
 * They handle the allocation and initialization of objects.
 */
extern object newArray(INT);             /* Create a new Array of given size */
extern object newBlock();                /* Create a new Block (closure) */
extern object newByteArray(INT);         /* Create a new ByteArray of given size */
extern object newClass(STR);             /* Create a new Class with given name */
extern object newChar(INT);              /* Create a new Character with given ASCII value */
extern object newContext(INT X OBJ X OBJ X OBJ); /* Create a new Context */
extern object newDictionary(INT);        /* Create a new Dictionary with given size */
extern object newFloat(FLOAT);           /* Create a new Float with given value */
extern object newMethod();               /* Create a new Method */
extern object newLink(OBJ X OBJ);        /* Create a new Link (for Dictionary chains) */
extern object newStString(STR);          /* Create a new String from C string */
extern object newSymbol(STR);            /* Create a new Symbol from C string */

/**
 * Object Manipulation Functions
 * 
 * These functions provide additional operations on Smalltalk objects.
 */
extern object shallowCopy(OBJ);          /* Create a shallow copy of an object */
extern double floatValue(OBJ);           /* Extract double value from a Float object */

/**
 * System Initialization and Symbol Management
 * 
 * These functions and variables handle symbol management and initialization.
 */
extern noreturn initCommonSymbols();     /* Initialize commonly used symbols */
noreturn readObjectFiles(FILEP, FILEP);  /* Read object files (table and data) */
extern object unSyms[], binSyms[];       /* Arrays of common unary and binary message symbols */

/**
 * Name Table and Symbol Management
 * 
 * These functions handle the lookup and management of names and symbols 
 * in the Smalltalk environment.
 */
extern noreturn nameTableInsert(OBJ X INT X OBJ X OBJ); /* Insert key-value pair into name table */
extern int strHash(STR);                /* Compute hash value for a string */
extern object globalKey(STR);           /* Find symbol in global symbols table by string */
extern object nameTableLookup(OBJ X STR); /* Look up value by string key in dictionary */
extern object findClass(STR);           /* Find a class by name string */

/**
 * Name Lookup Convenience Macros
 * 
 * These macros provide simplified access to common name lookup operations.
 */
#define globalSymbol(s) nameTableLookup(symbols, s) /* Look up symbol in global symbols table */

/**
 * Class Testing Macros
 * 
 * These macros provide convenient ways to test an object's class.
 */
#define isClassNameEqual(c, s) (c == globalSymbol(s)) /* Test if class matches name */
#define isObjectOfClassName(o, s) isClassNameEqual(getClass(o), s) /* Test object's class name */
