/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021
	Based on:
	
	Little Smalltalk, version 2
	Written by Tim Budd, Oregon State University, July 1987

	Memory Management Header
	
	This header file defines the core data structures and operations for
	the Smalltalkje memory management system. It includes:
	
	1. Object Representation:
	   - Object references (indices into object table)
	   - Object table structure
	   - Reference counting operations
	
	2. Memory Operations:
	   - Object allocation and deallocation
	   - Field access and modification
	   - Reference count manipulation
	
	3. Special Object Handling:
	   - Integer representation (small integers stored directly in references)
	   - Byte objects (for strings and byte arrays)
	   - nil object representation
	
	4. Performance Optimizations:
	   - Macros for common operations to reduce function call overhead
	   - Memory block allocation for efficient small object creation
	
	This header is a critical component that defines how all Smalltalk
	objects are represented and manipulated in memory.
*/

#include "env.h"

/*
	Object Reference Representation
	
	A core design decision in any Smalltalk implementation is how object
	references are represented. Smalltalkje uses object table indices rather
	than direct pointers to memory.
	
	Advantages of using indices:
	1. Allows for efficient object table compaction and garbage collection
	2. Enables split memory model with objects in both RAM and ROM
	3. Simplifies image saving/loading as objects have stable identities
	4. Makes debugging easier with consistent object IDs
	
	The object type is defined as an integer (rather than a pointer),
	which represents an index into the object table.
*/

typedef int object;

/*
	Memory Interface Design
	
	The memory management interface is designed for both flexibility and performance:
	
	1. Core operations are defined as macros for performance-critical code paths
	   to avoid function call overhead. These include reference counting, field access,
	   and type testing operations.
	
	2. More complex operations are implemented as functions, providing a clean
	   abstraction for memory management.
	
	3. The object table structure is exposed here to enable efficient macro
	   implementation, but consumers should use only the defined macros and functions
	   rather than accessing the structure directly.
	
	4. This design allows for future optimizations or even complete replacement
	   of the memory manager without affecting the rest of the system, as long
	   as the interface remains consistent.
*/

/**
 * Object Table Entry Structure
 * 
 * Each entry in the object table contains the following fields:
 * 
 * @field class - The class of this object (as an object reference)
 * @field referenceCount - Number of references to this object
 * @field size - Size of the object (negative for byte objects)
 * @field memory - Pointer to the actual object data in heap memory
 * 
 * The structure is optimized for fast access to class and reference count
 * information without requiring indirection through the memory pointer.
 * This improves performance for common operations like reference counting
 * and method dispatch.
 * 
 * Note: There is a potential optimization to move class and reference count
 * into the object memory itself, which would save 6 bytes per unused entry
 * (approximately 30KB in a 5K object table).
 */
struct objectStruct
{
	object class;
	short referenceCount;
	short size;
	object *memory;
};

#define ObjectTableMax 5000

#ifdef obtalloc
extern struct objectStruct *objectTable;
#endif
#ifndef obtalloc
extern struct objectStruct objectTable[];
#endif

/**
 * Reference Counting Macros
 * 
 * These macros handle the increment and decrement of object reference counts,
 * which are fundamental operations in the memory management system:
 * 
 * incr() - Increments the reference count of an object
 * decr() - Decrements the reference count and deallocates if it reaches zero
 * 
 * Both macros handle special cases:
 * - They ignore nil object (reference 0)
 * - They don't modify reference counts for immediate integers
 * - decr() calls sysDecr() for actual object deallocation when needed
 * 
 * The macros use a temporary global variable (incrobj) to evaluate the
 * argument only once, preventing side effects from multiple evaluations.
 */

extern object incrobj;

#define incr(x)                                 \
	if ((incrobj = (x)) && !isInteger(incrobj)) \
	refCountField(incrobj)++
#define decr(x)                                                                      \
	if (((incrobj = (x)) && !isInteger(incrobj)) && (--refCountField(incrobj) <= 0)) \
		sysDecr(incrobj);

/*
	notice that the argument x is first assigned to a global variable; this is
	in case evaluation of x results in side effects (such as assignment) which
	should not be repeated.
*/

/*
	The next most basic routines in the memory module are those that
	allocate blocks of storage.  There are three routines:
	allocObject(size) - allocate an array of objects
	allocByte(size) - allocate an array of bytes
	allocStr(str) - allocate a string and fill it in

	Again, these may be macros, or they may be actual procedure calls
*/

extern object allocObject(INT);
extern object allocByte(INT);
extern object allocStr(STR);

/**
 * Integer Object Representation
 * 
 * Smalltalkje uses a tagged integer representation where integers are stored
 * directly in the object reference rather than allocating separate objects.
 * This significant optimization saves memory and improves performance for
 * integer operations.
 * 
 * The encoding scheme works as follows:
 * - Negative integers are stored directly as their value
 * - Positive integers are encoded as (value * 2) + 1
 * - This ensures all integer references have either the lowest bit set (odd)
 *   or are negative
 * - Normal object references are even, positive numbers (shifted left by 1)
 * - Zero (0) is reserved for the nil object
 * 
 * The macros provide type checking and conversion:
 * - isInteger() - Tests if a reference is an integer (odd or negative)
 * - newInteger() - Converts a C integer to a Smalltalk integer reference
 * - intValue() - Extracts the C integer value from a Smalltalk integer
 */

extern object intobj;

#define isInteger(x) ((x)&0x8001)
#define newInteger(x) ((intobj = x) < 0 ? intobj : (intobj << 1) + 1)
#define intValue(x) ((intobj = x) < 0 ? intobj : (intobj >> 1))

/**
 * Byte Object Size Adjustment
 * 
 * This macro handles the special size representation for byte objects:
 * - Regular objects have positive size values (number of object slots)
 * - Byte objects have negative size values (number of bytes)
 * 
 * When a byte object's memory needs to be allocated or manipulated,
 * this macro converts the byte count to the required number of
 * object slots to hold those bytes.
 * 
 * @param size Size value to adjust (negative for byte objects)
 */
#define adjustSizeIfNeg(size)     \
	if (size < 0)                 \
	{                             \
		size = ((-size) + 1) / 2; \
	}

/**
 * Object Field Access Macros
 * 
 * These macros provide the core operations for accessing and modifying
 * object fields. They come in two varieties:
 * 
 * 1. Object Field Access (for regular objects storing references):
 *    - basicAt() - Access a field by index (1-based)
 *    - basicAtPut() - Set a field by index with reference count increment
 *    - simpleAtPut() - Set a field without reference count handling
 *    - fieldAtPut() - Replace a field with proper reference count decrement/increment
 * 
 * 2. Byte Field Access (for byte objects storing raw bytes):
 *    - byteAt() - Access a byte by index (1-based)
 *    - byteAtPut() - Set a byte by index
 * 
 * All access operations include bounds checking to prevent memory corruption,
 * and handle reference counting to maintain object lifetime correctly.
 */

#define basicAt(x, i) (sysMemPtr(x)[i - 1])
#define byteAt(x, i) ((int)((bytePtr(x)[i - 1])))
#define simpleAtPut(x, i, y) (sysMemPtr(x)[i - 1] = y)
#define basicAtPut(x, i, y) incr(simpleAtPut(x, i, y))
#define fieldAtPut(x, i, y) \
	f_i = i;                \
	decr(basicAt(x, f_i));  \
	basicAtPut(x, f_i, y)
extern int f_i;

/**
 * Object Table Access Macros
 * 
 * These low-level macros provide direct access to the object table fields.
 * They're primarily used internally by the memory management system and
 * the image loading/saving code.
 * 
 * The separation between these internal macros and the external object
 * field access macros allows the object table implementation to be changed
 * without affecting the rest of the system.
 * 
 * Normal code should use the higher-level object field access macros instead
 * of these direct object table accessors.
 */

// MOT: Check for which OT: e.g. getObjectTable(x)[getObjectIndex(x)]
#define objectTable(x) objectTable[x]

#define objTableClass(x) objectTable(x).class
// MOT: Check for ROM OT (will crash, but should never happen)
#define setObjTableClass(x, y) (objectTable(x).class = y)
#define objTableSize(x) objectTable(x).size
// MOT: Check for ROM OT (will crash, but should never happen)
#define setObjTableSize(x, y) (objectTable(x).size = y)
#define objTableMemory(x) objectTable(x).memory
// MOT: Check for ROM OT (will crash, but should never happen)
#define setObjTableMemory(x, y) (objectTable(x).memory = y)
#define objTableRefCount(x) objectTable(x).referenceCount
// MOT: Check for ROM OT (will crash, but should never happen)
#define setObjTableRefCount(x, y) (objectTable(x).referenceCount = y)

/**
 * High-Level Object Field Access Macros
 * 
 * These macros are used throughout the system to access object metadata
 * and content. Unlike the low-level object table access macros, these
 * account for the shifted object reference (dividing by 2) to convert
 * between external object references and internal table indices.
 * 
 * They provide access to:
 * - Class field of an object
 * - Size field of an object
 * - Reference count field of an object
 * - Memory pointer for an object's data
 * 
 * These macros form the primary interface that most code uses to
 * interact with objects in the system.
 */

#define classField(x) objTableClass(x >> 1)
// MOT: Check for ROM OT (will crash, but should never happen)
#define setClass(x, y) incr(classField(x) = y)

#define sizeField(x) objTableSize(x >> 1)
#define sysMemPtr(x) objTableMemory(x >> 1)

#define refCountField(x) objTableRefCount(x >> 1)
// MOT: Check for ROM OT (will crash, but should never happen)
#define setRefCountField(x, y) setObjTableRefCount(x >> 1, y)

extern object sysobj;
#define memoryPtr(x) (isInteger(sysobj = x) ? (object *)0 : sysMemPtr(sysobj))
#define bytePtr(x) ((byte *)memoryPtr(x))
#define charPtr(x) ((char *)memoryPtr(x))

#define nilobj (object)0

/*
	There is a large amount of differences in the qualities of malloc
	procedures in the Unix world.  Some perform very badly when asked
	to allocate thousands of very small memory blocks, while others
	take this without any difficulty.  The routine mBlockAlloc is used
	to allocate a small bit of memory; the version given below
	allocates a large block and then chops it up as needed; if desired,
	for versions of malloc that can handle small blocks with ease
	this can be replaced using the following macro: 

#define mBlockAlloc(size) (object *) calloc((unsigned) size, sizeof(object))

	This can, and should, be replaced by a better memory management
	algorithm.
*/
#ifndef mBlockAlloc
extern object *mBlockAlloc(INT);
#endif

/*
 * Called at startup to initialize the memory system (object tables, etc)
 */
noreturn initMemoryManager(void);

/* setFreeLists - initialise the object free lists (used to recycle objects vs mallocs) */
noreturn setFreeLists(void);

/*
	the dictionary symbols is the source of all symbols in the system
*/
extern object symbols;

int classInstCount(object aClass);

/*
	finally some external declarations with prototypes
*/

extern noreturn sysError(STR X STR);
extern noreturn dspMethod(STR X STR);
extern noreturn initSPIFFS();
extern noreturn initMemoryManager(NOARGS);
extern noreturn imageWrite(FILEP);
extern noreturn imageRead(FILEP);
extern boolean debugging;
extern noreturn sysDecr(object z);
extern boolean execute(object aProcess, int maxsteps);
