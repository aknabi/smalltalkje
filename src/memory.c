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

	// Initialize free lists by finding unused objects and organizing them by size
	// We iterate backward through the object table (optimization for locality)
	for (z = ObjectTableMax - 1; z > 0; z--)
	{
		// Only process objects with zero reference count (unreferenced/unused)
		if (objTableRefCount(z) == 0)
		{
			// Get this object's entry in the object table
			p = &objectTable[z];
			
			// Get object size (adjusting if it's a byte object with negative size)
			size = p->size;
			adjustSizeIfNeg(size);  // TODO: Rename this to adjustSizeIfByte()
			
			// Add this object to the appropriate free list:
			// 1. Set its class field to point to the current head of the list
			// 2. Make this object the new head of the list for its size
			p->class = objectFreeList[size];
			objectFreeList[size] = z;
			
			// Clear all instance variables to nil (prevents stale references)
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
 * Allocates a new object with specified size using a multi-strategy allocation algorithm
 * 
 * This function implements a sophisticated allocation strategy with four fallback methods:
 * 
 * 1. First attempt: Look for an exact-sized object in the free list (fastest path)
 * 2. Second attempt: Use a size-0 object and expand it to needed size
 * 3. Third attempt: Find a larger object and trim it down to size
 * 4. Fourth attempt: Find a smaller object, free its memory, and resize it
 * 
 * This multi-strategy approach maximizes memory reuse and minimizes fragmentation
 * while trying to avoid new memory allocations when possible. The function handles
 * all the object table bookkeeping for the new object, setting its reference count,
 * class, and size appropriately.
 *
 * @param memorySize Size of object to allocate (in object units)
 * @return Reference to newly allocated object (index shifted left by 1 bit)
 */
object allocObject(memorySize) int memorySize;
{
	int i;
	register int position;
	boolean done;

	// Ensure requested size is within allowed limits
	if (memorySize >= FREELISTMAX)
	{
		fprintf(stderr, "size %d\n", memorySize);
		sysError("allocation bigger than permitted", "allocObject");
	}

	// STRATEGY 1: Try to find an exact size match in free list (most efficient)
	if ((position = objectFreeList[memorySize]) != 0)
	{
		// Remove this object from the free list by updating the list head
		// The class field of free objects is used to store the next free object
		objectFreeList[memorySize] = objTableClass(position);
	}

	// STRATEGY 2: Try to repurpose a size-0 object by expanding it
	else if ((position = objectFreeList[0]) != 0)
	{
		objectFreeList[0] = objTableClass(position);
		setObjTableSize(position, memorySize);
		setObjTableMemory(position, mBlockAlloc(memorySize));
	}
	
	// If we get here, we need more elaborate strategies
	else
	{
		done = false;

		// STRATEGY 3: Find a larger object and shrink it (no memory allocation needed)
		for (i = memorySize + 1; i < FREELISTMAX; i++)
		{
			if ((position = objectFreeList[i]) != 0)
			{
				// Remove from its current free list
				objectFreeList[i] = objTableClass(position);
				
				// Simply update its size field to the smaller requested size
				// This may waste some memory but avoids allocation overhead
				setObjTableSize(position, memorySize);
				done = true;
				break;
			}
		}

		// STRATEGY 4: Find a smaller object and expand it (requires new memory allocation)
		if (!done)
		{
			for (i = 1; i < memorySize; i++)
			{
				if ((position = objectFreeList[i]) != 0)
				{
					// Remove from its current free list
					objectFreeList[i] = objTableClass(position);
					setObjTableSize(position, memorySize);
					
					// Free old memory block and allocate a new one of the right size
#ifdef mBlockAlloc
					free(objTableMemory(position))
#endif
						setObjTableMemory(position, mBlockAlloc(memorySize));
					done = true;
					break;
				}
			}
		}

		// If all allocation strategies failed, we're out of objects
		if (!done)
		{
			sysError("out of objects", "alloc");
		}
	}

	// Initialize the newly allocated object with clean state
	setObjTableRefCount(position, 0);   // New objects start with refcount = 0
	setObjTableClass(position, nilobj); // Class initially nil, caller will set
	setObjTableSize(position, memorySize);
	
	// Convert table index to object reference (shifted left by 1 bit)
	// This shift distinguishes object references from small integers
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
/**
 * Increments the reference count of an object
 * 
 * This function increases the reference count of an object by one,
 * indicating that there is one more pointer to this object in the system.
 * Integers and nil don't have reference counts and are ignored.
 *
 * @param z The object reference whose reference count should be incremented
 */
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
/**
 * Decrements the reference count of an object
 * 
 * This function decreases the reference count of an object by one,
 * indicating that there is one fewer pointer to this object in the system.
 * If the reference count drops to zero, the object is reclaimed by calling sysDecr.
 * Integers and nil don't have reference counts and are ignored.
 *
 * @param z The object reference whose reference count should be decremented
 */
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
 * This function reclaims an unreferenced object by placing it on the appropriate free list
 * and recursively decrementing the reference counts of all objects it references.
 * 
 * This is the core of the reference counting system, responsible for:
 * 1. Verifying the reference count is not negative (error if it is)
 * 2. Decrementing the reference count of the object's class
 * 3. Adding the object to the appropriate free list for its size
 * 4. Decrementing the reference count of all instance variables
 * 5. Clearing all instance variables to nilobj
 *
 * @param z The object reference to deallocate (left-shifted by 1)
 */
void sysDecr(object z)
{
	register struct objectStruct *p;
	register int i;
	int size;

	// Convert object reference to object table index and get pointer to object
	p = &objectTable(z >> 1);
	
	// Sanity check: reference count should never be negative
	if (p->referenceCount < 0)
	{
		fprintf(stderr, "object %d has a negative reference count\n", z);
		sysError("negative reference count", "");
	}
	
	// Decrement the class's reference count (since we won't be using it anymore)
	decr(p->class);
	
	// Get object size (and adjust if it's a byte object with negative size)
	size = p->size;
	adjustSizeIfNeg(size);
	
	// Add this object to the appropriate free list by:
	// 1. Setting its class field to point to the current first object in the free list
	// 2. Making this object the new first object in the free list
	p->class = objectFreeList[size];
	objectFreeList[size] = z >> 1;
	
	// For non-empty objects, we need to handle their contents
	if (size > 0)
	{
		// Only decrement instance variables if this is a regular object (positive size)
		// Byte objects (negative size) don't contain object references to decrement
		if (p->size > 0)
			for (i = size; i;)
				decr(p->memory[--i]);  // Decrement each instance variable's reference count
		
		// Clear all instance variables by setting them to nil
		// (This prevents dangling references and makes debugging easier)
		for (i = size; i > 0;)
		{
			p->memory[--i] = nilobj;
		}
	}
	
	// Restore the original size (which might have been negative for byte objects)
	p->size = size;
}

#ifndef basicAt
/**
 * Retrieves an instance variable from an object at a specified index
 * 
 * This function provides low-level access to object instance variables.
 * It performs bounds checking to ensure valid access and converts from
 * 1-based indexing (used in Smalltalk) to 0-based indexing (used in C).
 *
 * @param z The object to access
 * @param i The 1-based index of the instance variable to retrieve
 * @return The object reference stored at the specified index
 */
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
/**
 * Sets an instance variable in an object at a specified index without reference counting
 * 
 * This function provides low-level modification of object instance variables.
 * Unlike basicAtPut, it doesn't handle reference counting, making it suitable
 * for internal operations where reference counts are managed separately.
 * It performs bounds checking to ensure valid access.
 *
 * @param z The object to modify
 * @param i The 1-based index of the instance variable to set
 * @param v The object reference to store at the specified index
 */
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
/**
 * Sets an instance variable in an object at a specified index with reference counting
 * 
 * This function updates an instance variable and properly handles reference counting
 * by incrementing the reference count of the stored object. It's the standard way
 * to update object fields when the previous value has already been decremented elsewhere.
 *
 * @param z The object to modify
 * @param i The 1-based index of the instance variable to set
 * @param v The object reference to store at the specified index (will have ref count incremented)
 */
void basicAtPut(z, i, v)
	object z,
	v;
register int i;
{
	simpleAtPut(z, i, v);
	incr(v);  // Increment reference count of the stored object
}
#endif

#ifdef fieldAtPut
int f_i;
#endif

#ifndef fieldAtPut
/**
 * Replaces an instance variable with full reference count management
 * 
 * This function provides complete reference count management when replacing
 * an instance variable. It:
 * 1. Decrements the reference count of the current value (which may free it)
 * 2. Sets the new value and increments its reference count
 *
 * This is the safest way to replace fields in an object because it properly
 * handles reference counting for both the old and new values.
 *
 * @param z The object to modify
 * @param i The 1-based index of the instance variable to replace
 * @param v The new object reference to store (will have ref count incremented)
 */
void fieldAtPut(z, i, v)
	object z,
	v;
register int i;
{
	decr(basicAt(z, i));  // Decrement reference count of current value
	basicAtPut(z, i, v);  // Store new value and increment its reference count
}
#endif

#ifndef byteAt
/**
 * Gets the byte value at a specific index in a byte object
 * 
 * This function reads a byte value from a specific position in a byte object.
 * Byte objects (like strings, ByteArrays) store raw bytes rather than object
 * references, and have a negative size field to indicate they are byte objects.
 * 
 * The function performs bounds checking to ensure the index is valid
 * and converts the negative size field appropriately.
 *
 * @param z The byte object to read from
 * @param i The index into the byte object (1-based)
 * @return The byte value at the given index
 */
int byteAt(z, i)
	object z;
register int i;
{
	byte *bp;
	unsigned char t;
    
    // Get absolute size - byte objects have negative size fields
    int objSize = sizeField(z);
    int byteSize = objSize < 0 ? -objSize : objSize;

	if (isInteger(z))
		sysError("indexing integer", "byteAt");
	else if ((i <= 0) || (i > 2 * byteSize))
	{
		fprintf(stderr, "index %d size %d\n", i, byteSize);
		sysError("index out of range", "byteAt");
	}
	else
	{
		bp = bytePtr(z);
		t = bp[i - 1];
		i = (int)t;
	}
	return (i);
}
#endif

#ifndef byteAtPut
/**
 * Sets the byte value at a specific index in a byte object
 * 
 * This function writes a byte value to a specific position in a byte object.
 * Byte objects (like strings, ByteArrays) store raw bytes rather than object
 * references, and have a negative size field to indicate they are byte objects.
 * 
 * The function performs bounds checking to ensure the index is valid
 * and converts the negative size field appropriately.
 *
 * @param z The byte object to modify
 * @param i The index into the byte object (1-based)
 * @param x The byte value to write
 */
void byteAtPut(object z, int i, int x)
{
	byte *bp;
    
    // Get absolute size - byte objects have negative size fields
    int objSize = sizeField(z);
    int byteSize = objSize < 0 ? -objSize : objSize;

	if (isInteger(z))
		sysError("indexing integer", "byteAtPut");
	else if ((i <= 0) || (i > 2 * byteSize))
	{
		fprintf(stderr, "index %d size %d\n", i, byteSize);
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
 * This function implements a depth-first traversal of the object graph, incrementing
 * reference counts to mark live objects during image loading.
 * 
 * Written by Steven Pemberton, this function is used during image loading to:
 * 1. Increment the reference count of the visited object
 * 2. If this is the first visit (count was 0), recursively visit:
 *    a. The object's class
 *    b. All objects referenced by this object's instance variables
 * 
 * This rebuilds reference counts after loading an image file, ensuring
 * only reachable objects are retained in memory.
 *
 * @param x The object to visit and mark as in use
 */
void visit(register object x)
{
	int i, s;
	object *p;

	// Skip nil objects and small integers (they aren't in the object table)
	if (x && !isInteger(x))
	{		
		// Increment the reference count of this object
		if (++(refCountField(x)) == 1)
		{
			// First visit to this object (count was 0 before increment)
			// We need to recursively visit all objects it references
			
			// First, visit the object's class
			visit(classField(x));
			
			// Get object size
			s = sizeField(x);
			
			// For regular objects (size > 0), visit all instance variables
			if (s > 0)
			{
				p = sysMemPtr(x);
				for (i = s; i; --i)
					visit(*p++);  // Visit each referenced object
			}
			// Note: Byte objects (size < 0) don't contain object references
		}
		// If count was already > 0, this object was already visited,
		// so we don't need to visit its references again (prevents infinite recursion)
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
