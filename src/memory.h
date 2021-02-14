/*
	Little Smalltalk, version 2
	Written by Tim Budd, Oregon State University, July 1987
*/

#include "env.h"

/*
	The first major decision to be made in the memory manager is what
	an entity of type object really is.  Two obvious choices are a pointer (to 
	the actual object memory) or an index into an object table.  We decided to
	use the latter, although either would work.

	Similarly, one can either define the token object using a typedef,
	or using a define statement.  Either one will work (check this?)
*/

typedef int object;

/*
	The memory module itself is defined by over a dozen routines.

	All of these could be defined by procedures, and indeed this was originally
	done.  However, for efficiency reasons, many of these procedures can be
	replaced by macros generating in-line code.  For the latter approach
	to work, the structure of the object table must be known.  For this reason,
	it is given here.  Note, however, that outside of the files memory.c and
	unixio.c (or macio.c on the macintosh) ONLY the macros described in this
	file make use of this structure: therefore modifications or even complete
	replacement is possible as long as the interface remains consistent
*/

/*
 * This represents the basic entry in the object table. Currently it's a "standard"
 * object table struct and optimized for speed with the class and ref count available
 * without indirection into the object memory
 * 
 * TODO: Look at moving the class and and reference count to the first bytes of the object
 * This would eliminate 6 bytes per unused entry (so in a 5k free table 30k).
 * 
 */
struct objectStruct
{
	object class;
	short referenceCount;
	short size;
	object *memory;
};

#define ObjectTableMax 6144

#ifdef obtalloc
extern struct objectStruct *objectTable;
#endif
#ifndef obtalloc
extern struct objectStruct objectTable[];
#endif

/*
	The most basic routines to the memory manager are incr and decr,
	which increment and decrement reference counts in objects.  By separating
	decrement from memory freeing, we could replace these as procedure calls
	by using the following macros (thereby saving procedure calls):
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

/*
	Integer objects are (but need not be) treated specially.

	In this memory manager, negative integers are just left as is, but
	positive integers are changed to x*2+1.  Either a negative or an odd
	number is therefore an integer, while a nonzero even number is an
	object pointer (multiplied by two).  Zero is reserved for the object ``nil''
	Since newInteger does not fill in the class field, it can be given here.
	If it was required to use the class field, it would have to be deferred
	until names.h
*/

extern object intobj;

#define isInteger(x) ((x)&0x8001)
#define newInteger(x) ((intobj = x) < 0 ? intobj : (intobj << 1) + 1)
#define intValue(x) ((intobj = x) < 0 ? intobj : (intobj >> 1))

#define adjustSizeIfNeg(size)     \
	if (size < 0)                 \
	{                             \
		size = ((-size) + 1) / 2; \
	}

/*
	There are four routines used to access fields within an object.

	Again, some of these could be replaced by macros, for efficiency
	basicAt(x, i) - ith field (start at 1) of object x
	basicAtPut(x, i, v) - put value v in object x
	byteAt(x, i) - ith field (start at 0) of object x
	byteAtPut(x, i, v) - put value v in object x
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

/*
	Routines (or macros) are used to access or set object table fields
	This allows for modification of the object table implemntation requiring
	only changes to these macros

	These are used internally in memory routines and in unixio.c
*/
#define objectTable(x) objectTable[x]

#define objTableClass(x) objectTable(x).class
#define setObjTableClass(x, y) (objectTable(x).class = y)
#define objTableSize(x) objectTable(x).size
#define setObjTableSize(x, y) (objectTable(x).size = y)
#define objTableMemory(x) objectTable(x).memory
#define setObjTableMemory(x, y) (objectTable(x).memory = y)
#define objTableRefCount(x) objectTable(x).referenceCount
#define setObjTableRefCount(x, y) (objectTable(x).referenceCount = y)

/*
	Finally, a few routines (or macros) are used to access or set
	class fields and size fields of objects

	These are used outside of the memory routines (thus the index is shifted right)
*/

#define classField(x) objTableClass(x >> 1)
#define setClass(x, y) incr(classField(x) = y)
#define sizeField(x) objectTable[x >> 1].size
#define sysMemPtr(x) objectTable[x >> 1].memory
#define refCountField(x) objTableRefCount(x >> 1)
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
	the dictionary symbols is the source of all symbols in the system
*/
extern object symbols;

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
extern void sysDecr(object z);
extern boolean execute(object aProcess, int maxsteps);
