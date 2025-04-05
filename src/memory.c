/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021
	Based on Little Smalltalk, version 2
	Written by Tim Budd, Oregon State University, July 1987

	Improved incorporating suggestions by 
		Steve Crawley, Cambridge University, October 1987
		Steven Pemberton, CWI, Amsterdam, Oct 1987

	Memory Management Module
	
	This module implements the memory management system for Smalltalkje,
	handling object allocation, reference counting, and memory reclamation.
	
	Core features:
	
	1. Reference Counting:
	   - Uses a simple reference counting approach for memory management
	   - When an object's reference count drops to zero, it's reclaimed
	   - No cycle detection (potential memory leak if circular references exist)
	   - Reference counts are not stored in image files but reconstructed on load
	
	2. Object Allocation:
	   - Uses free lists of various sizes for efficient memory reuse
	   - Can allocate normal objects or byte objects (for strings, etc.)
	   - Handles object size up to FREELISTMAX (2048) elements
	   - Tries to reuse existing objects before allocating new memory
	
	3. Object Memory Structure:
	   - Objects are referenced by indexes into an object table
	   - The object table stores metadata (class, size, reference count)
	   - Actual object data is stored in separately allocated memory
	   - Integers are represented directly in the object reference (not in the table)
	
	4. Memory Optimization:
	   - Uses block allocation for small objects to reduce malloc overhead
	   - Free lists allow reuse of previously allocated objects
	   - Special handling for byte objects and strings
	
	ESP32 Extensions:
	- Support for objects in both RAM and ROM (flash memory)
	- Special handling to avoid modifying ROM-based objects
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "env.h"
#include "memory.h"

void setFreeLists(void);
void sysDecr(object z);
void visit(register object x);

boolean debugging = false; /* right now looks like this is not being used... use it or lose it */
object sysobj; /* temporary used to avoid rereference in macros */
object intobj;

object symbols; /* table of all symbols created */

/*
	in theory the objectTable should only be accessible to the memory
	manager.  Indeed, given the right macro definitions, this can be
	made so.  Never the less, for efficiency sake some of the macros
	can also be defined to access the object table directly

	Some systems (e.g. the Macintosh) have static limits (e.g. 32K)
	which prevent the object table from being declared.
	In this case the object table must first be allocated via
	calloc during the initialization of the memory manager.
*/

// MOT: Will need a pointer to ROM OT, and this will be RAM OT

#ifdef obtalloc
struct objectStruct *objectTable;
#endif
#ifndef obtalloc
struct objectStruct objectTable[ObjectTableMax];
#endif

/*
	The following variables are strictly local to the memory
	manager module

	FREELISTMAX defines the maximum size of any object.
*/

#define FREELISTMAX 2048
static object objectFreeList[FREELISTMAX]; /* free list of objects */

#ifndef mBlockAlloc

#define MemoryBlockSize 2048
/* Memory Block Allocation System
 * Rather than individually allocating small blocks of memory with malloc,
 * which has high overhead, this system:
 * 1. Allocates large chunks (MemoryBlockSize) at once
 * 2. Subdivides these chunks as needed for small object allocations
 * 3. Only calls malloc when the current chunk is exhausted
 * This significantly improves allocation performance and reduces fragmentation.
 */
static object *memoryBlock;		  /* Current malloc'ed chunk of memory being used */
static int currentMemoryPosition; /* Current position (offset) within memoryBlock */
#endif

/**
 * Initializes the memory management subsystem
 * 
 * This function prepares the memory manager for use by:
 * 1. Allocating the object table if needed (when obtalloc is defined)
 * 2. Clearing all free list pointers
 * 3. Setting all reference counts to zero
 * 4. Building the initial free lists
 * 5. Setting up the nil object (index 0 with reference count 1)
 * 
 * This must be called before any other memory operations are performed.
 */
noreturn initMemoryManager(void)
{
	int i;

#ifdef obtalloc
	objectTable = obtalloc(ObjectTableMax, sizeof(struct objectStruct));
	if (!objectTable)
		sysError("cannot allocate", "object table");
#endif

	/* set all the free list pointers to zero */
	for (i = 0; i < FREELISTMAX; i++)
	{
		objectFreeList[i] = nilobj;
	}

	/* set all the reference counts to zero */
	for (i = 0; i < ObjectTableMax; i++)
	{
		setObjTableRefCount(i, 0);
		setObjTableSize(i, 0);
	}

	/* MOT: No need if we are starting with ROM OT, but then need to clean on image save */
	/* make up the initial free lists */
	setFreeLists();

#ifndef mBlockAlloc
	/* force an allocation on first object assignment */
	currentMemoryPosition = MemoryBlockSize + 1;
#endif

	/* object at location 0 is the nil object, so give it nonzero ref */
	setObjTableRefCount(nilobj, 1);
	setObjTableSize(nilobj, 0);
}

/**
 * Initializes the free lists of unused objects
 * 
 * This function builds lists of available objects for each size by:
 * 1. Scanning the object table for objects with zero reference counts
 * 2. Organizing them into lists by size for quick allocation
 * 3. Cleaning any instance variables in the free objects
 * 
 * The free lists allow fast reuse of previously allocated objects
 * without requiring new memory allocation.
 */
noreturn setFreeLists(void)
{
	int i, size;
	register int z;
	register struct objectStruct *p;

	objectFreeList[0] = nilobj;

	/*
		MOT: No need for ROM objects
		For each object, check if the reference count is 0.
		if so, adjust size (if byte obj), set the obj class
		to the OFL[size] (?), set the OFL entry at the obj
		size to the object and clear the object's inst var
	 */
	for (z = ObjectTableMax - 1; z > 0; z--)
	{
		if (objTableRefCount(z) == 0)
		{
			/* Unreferenced, so do a sort of sysDecr: */
			p = &objectTable[z];
			size = p->size;
			// TODO: Rename this to adjustSizeIfByte()
			adjustSizeIfNeg(size);
			p->class = objectFreeList[size];
			objectFreeList[size] = z;
			for (i = size; i > 0;)
				p->memory[--i] = nilobj;
		}
	}
}

/**
 * Allocates a block of memory for object storage
 * 
 * This function manages memory allocation in large blocks:
 * 1. When the current memory block is full, allocates a new large block
 * 2. Carves out a section of the current block for the requested object
 * 3. Returns a pointer to the newly allocated memory region
 * 
 * This approach reduces malloc overhead by amortizing it across many
 * small allocations, significantly improving allocation performance
 * and reducing memory fragmentation.
 *
 * @param memorySize The size of memory block needed (in object units)
 * @return Pointer to the allocated memory block
 */
#ifndef mBlockAlloc

object *mBlockAlloc(int memorySize)
{
	object *objptr;

	if (currentMemoryPosition + memorySize >= MemoryBlockSize)
	{
		/* we toss away space here.  Space-Frugal users may want to
	   fix this by making a new object of size
	   MemoryBlockSize - currentMemoryPositon - 1
	   and putting it on the free list, but I think
	   the savings is potentially small */

		memoryBlock =
			(object *)calloc((unsigned)MemoryBlockSize, sizeof(object));
		if (!memoryBlock)
			sysError("out of memory", "malloc failed");
		currentMemoryPosition = 0;
	}
	objptr = (object *)&memoryBlock[currentMemoryPosition];
	currentMemoryPosition += memorySize;
	return (objptr);
}
#endif

/* MOT: Check if "normal exec" (vs image building) requires any logic */
/**
 * Allocates a new object with the specified size
 * 
 * This function finds or creates space for a new object by:
 * 1. First checking the free list for an object of the exact size
 * 2. If not found, trying to use a size 0 object and expanding it
 * 3. If still not found, looking for a larger object to shrink
 * 4. If still not found, looking for a smaller object to expand
 * 5. If all strategies fail, reports an "out of objects" error
 * 
 * The function handles all the object table bookkeeping for the new object,
 * setting its reference count, class, and size appropriately.
 *
 * @param memorySize The size of the object to allocate (in object units)
 * @return The newly allocated object reference (shifted left by 1)
 */
object allocObject(memorySize) int memorySize;
{
	int i;
	register int position;
	boolean done;

	if (memorySize >= FREELISTMAX)
	{
		fprintf(stderr, "size %d\n", memorySize);
		sysError("allocation bigger than permitted", "allocObject");
	}

	/* first try the free lists, this is fastest */
	if ((position = objectFreeList[memorySize]) != 0)
	{
		objectFreeList[memorySize] = objTableClass(position);
	}

	/* if not there, next try making a size zero object and
       making it bigger */
	else if ((position = objectFreeList[0]) != 0)
	{
		objectFreeList[0] = objTableClass(position);
		setObjTableSize(position, memorySize);
		setObjTableMemory(position, mBlockAlloc(memorySize));
	}
	/* not found, must work a bit harder */
	else
	{
		done = false;

		/* first try making a bigger object smaller */
		for (i = memorySize + 1; i < FREELISTMAX; i++)
		{
			if ((position = objectFreeList[i]) != 0)
			{
				objectFreeList[i] = objTableClass(position);
				/* just trim it a bit */
				setObjTableSize(position, memorySize);
				done = true;
				break;
			}
		}

		/* next try making a smaller object bigger */
		if (!done)
		{
			for (i = 1; i < memorySize; i++)
			{
				if ((position = objectFreeList[i]) != 0)
				{
					objectFreeList[i] = objTableClass(position);
					setObjTableSize(position, memorySize);
#ifdef mBlockAlloc
					free(objTableMemory(position))
#endif
						setObjTableMemory(position, mBlockAlloc(memorySize));
					done = true;
					break;
				}
			}
		}

		/* if we STILL don't have it then there is nothing more we can do */
		if (!done)
		{
			sysError("out of objects", "alloc");
		}
	}

	/* set class and type */
	setObjTableRefCount(position, 0);
	setObjTableClass(position, nilobj);
	setObjTableSize(position, memorySize);
	return (position << 1);
}

/**
 * Allocates a byte object (for strings, ByteArrays, etc.)
 * 
 * This function creates an object for storing bytes by:
 * 1. Calculating how many object units are needed to store the bytes
 * 2. Allocating a normal object of that size
 * 3. Setting a negative size field to indicate it's a byte object
 * 
 * Byte objects store raw bytes rather than object references,
 * and are used for strings, byte arrays, and other binary data.
 *
 * @param size The number of bytes to allocate space for
 * @return A newly allocated byte object
 */
object allocByte(size) int size;
{
	object newObj;

	newObj = allocObject((size + 1) / 2);
	/* negative size fields indicate bit objects */
	sizeField(newObj) = -size;
	return newObj;
}

/**
 * Allocates a string object containing the specified C string
 * 
 * This function creates a string object by:
 * 1. Allocating a byte object large enough for the string plus null terminator
 * 2. Copying the string content into the byte object
 * 
 * This is a convenience function for creating String objects directly
 * from C strings without manual byte object management.
 *
 * @param str The C string to copy into the new string object
 * @return A newly allocated string object containing a copy of the input string
 */
object allocStr(str) register char *str;
{
	register object newSym;
	char *c;

	newSym = allocByte(1 + (int) strlen(str));
	c = charPtr(newSym);
	ignore strcpy(c, str);
	return (newSym);
}

#ifdef incr
object incrobj; /* buffer for increment macro */
#endif
#ifndef incr
void incr(z)
	object z;
{
	if (z && !isInteger(z))
	{
		refCountField(z >> 1)++
	}
}
#endif

#ifndef decr
void decr(z)
	object z;
{
	if (z && !isInteger(z))
	{
		if (--refCountField(z) <= 0)
		{
			sysDecr(z);
		}
	}
}
#endif

/**
 * Handles object deallocation when reference count reaches zero
 * 
 * This function reclaims an unreferenced object by:
 * 1. Verifying the reference count is not negative (error if it is)
 * 2. Decrementing the reference count of the object's class
 * 3. Adding the object to the appropriate free list for its size
 * 4. Decrementing the reference count of all instance variables
 * 5. Clearing all instance variables to nilobj
 * 
 * This is the core of the reference counting system, responsible for
 * cascading deallocation of objects that are no longer referenced.
 *
 * @param z The object reference to deallocate (shifted left by 1)
 */
void sysDecr(object z)
{
	register struct objectStruct *p;
	register int i;
	int size;

	p =  &objectTable(z >> 1);
	if (p->referenceCount < 0)
	{
		fprintf(stderr, "object %d has a negative reference count\n", z);
		sysError("negative reference count", "");
	}
	decr(p->class);
	size = p->size;
	adjustSizeIfNeg(size);
	p->class = objectFreeList[size];
	objectFreeList[size] = z >> 1;
	if (size > 0)
	{
		if (p->size > 0)
			for (i = size; i;)
				decr(p->memory[--i]);
		for (i = size; i > 0;)
		{
			p->memory[--i] = nilobj;
		}
	}
	p->size = size;
}

#ifndef basicAt
object basicAt(z, i)
	object z;
register int i;
{
	if (isInteger(z))
		sysError("attempt to index", "into integer");
	else if ((i <= 0) || (i > sizeField(z)))
	{
		ignore fprintf(stderr, "index %d size %d\n", i,
					   (int)sizeField(z));
		sysError("index out of range", "in basicAt");
	}
	else
		return (sysMemPtr(z)[i - 1]);
	return (0);
}
#endif

#ifndef simpleAtPut

void simpleAtPut(z, i, v)
	object z,
	v;
int i;
{
	if (isInteger(z))
		sysError("assigning index to", "integer value");
	else if ((i <= 0) || (i > sizeField(z)))
	{
		ignore fprintf(stderr, "index %d size %d\n", i,
					   (int)sizeField(z));
		sysError("index out of range", "in basicAtPut");
	}
	else
	{
		sysMemPtr(z)[i - 1] = v;
	}
}
#endif

#ifndef basicAtPut

void basicAtPut(z, i, v)
	object z,
	v;
register int i;
{
	simpleAtPut(z, i, v);
	incr(v);
}
#endif

#ifdef fieldAtPut
int f_i;
#endif

#ifndef fieldAtPut
void fieldAtPut(z, i, v)
	object z,
	v;
register int i;
{
	decr(basicAt(z, i));
	basicAtPut(z, i, v);
}
#endif

#ifndef byteAt
int byteAt(z, i)
	object z;
register int i;
{
	byte *bp;
	unsigned char t;

	if (isInteger(z))
		sysError("indexing integer", "byteAt");
	else if ((i <= 0) || (i > 2 * -sizeField(z)))
	{
		fprintf(stderr, "index %d size %d\n", i, sizeField(z));
		sysError("index out of range", "byteAt");
	}
	else
	{
		bp = bytePtr(z);
		t = bp[i - 1];
		fprintf(stderr, "byte at %d returning %d\n", i, (int)t);
		i = (int)t;
	}
	return (i);
}
#endif

#ifndef byteAtPut
void byteAtPut(object z, int i, int x)
{
	byte *bp;

    // FIX: Just using -sizeField causes ByteArray  crash as sign is inverted... next task, why?
    int sizeField = sizeField(z);
    int negSizeField = sizeField < 0 ? -sizeField : sizeField;

	if (isInteger(z))
		sysError("indexing integer", "byteAtPut");
	else if ((i <= 0) || (i > 2 * -sizeField(z)))
	{
		fprintf(stderr, "index %d size %d\n", i, negSizeField);
		sysError("index out of range", "byteAtPut");
	}
	else
	{
		bp = bytePtr(z);
		bp[i - 1] = x;
	}
}
#endif

/**
 * Marks an object and all objects it references as being in use
 * 
 * This function implements a mark phase of a mark-sweep algorithm by:
 * 1. Incrementing the reference count of the visited object
 * 2. If this is the first visit (count was 0), recursively visiting:
 *    a. The object's class
 *    b. All objects referenced by this object's instance variables
 * 
 * Written by Steven Pemberton, this function is used during image loading
 * to rebuild reference counts, ensuring only reachable objects are retained.
 * It essentially performs a depth-first traversal of the object graph.
 *
 * @param x The object to visit and mark as in use
 */
void visit(register object x)
{
	int i, s;
	object *p;

	if (x && !isInteger(x))
	{		
		if (++(refCountField(x)) == 1)
		{
			/* first time we've visited it, so: */
			visit(classField(x));
			s = sizeField(x);
			if (s > 0)
			{
				p = sysMemPtr(x);
				for (i = s; i; --i)
					visit(*p++);
			}
		}
	}
}

/**
 * Counts the number of live objects in the system
 * 
 * This function counts objects with non-zero reference counts,
 * which indicates they are in use by the Smalltalk system.
 * Useful for diagnostic and monitoring purposes.
 *
 * @return The total count of live objects
 */
int objectCount()
{
	register int i, j;
	j = 0;
	for (i = 0; i < ObjectTableMax; i++)
		if (objTableRefCount(i) > 0)
			j++;
	return j;
}

/**
 * Counts the number of instances of a specific class
 * 
 * This function counts live objects (reference count > 0) 
 * that belong to the specified class. Useful for diagnostic
 * purposes and memory usage analysis.
 *
 * @param aClass The class object whose instances should be counted
 * @return The number of instances of the specified class
 */
int classInstCount(object aClass)
{
	register int i, j;
	j = 0;
	for (i = 0; i < ObjectTableMax; i++)
		if (objTableRefCount(i) > 0 && objTableClass(i) == aClass)
			j++;
	return j;
}

/**
 * Determines the maximum size of objects in the free lists
 * 
 * This function scans through the free lists to find the largest
 * size bucket that contains at least one free object. This provides
 * information about the largest object that can be allocated without
 * requiring new memory allocation.
 *
 * @return The size of the largest available free object
 */
int maxObjectSize()
{
	int max = 0;
	for (int i = 0; i < FREELISTMAX; i++)
		if (objectFreeList[i] != nilobj) max = i;
	return max;
}
