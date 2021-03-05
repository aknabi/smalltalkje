/*
	Little Smalltalk, version 2
	Written by Tim Budd, Oregon State University, July 1987

	Improved incorporating suggestions by 
		Steve Crawley, Cambridge University, October 1987
		Steven Pemberton, CWI, Amsterdam, Oct 1987

	memory management module

	This is a rather simple, straightforward, reference counting scheme.
	There are no provisions for detecting cycles, nor any attempt made
	at compaction.  Free lists of various sizes are maintained.
	At present only objects up to 255 bytes can be allocated, 
	which mostly only limits the size of method (in text) you can create.

	reference counts are not stored as part of an object image, but
	are instead recreated when the object is read back in.
	This is accomplished using a mark-sweep algorithm, similar
	to those used in garbage collection.

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
/* the current memory block being hacked up */
static object *memoryBlock;		  /* malloc'ed chunck of memory */
static int currentMemoryPosition; /* last used position in above */
#endif

/* initialize the memory management module */
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

/* setFreeLists - initialise the free lists */
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

/*
  mBlockAlloc - rip out a block (array) of object of the given size from
	the current malloc block 
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
/* allocate a new memory object */
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

object allocByte(size) int size;
{
	object newObj;

	newObj = allocObject((size + 1) / 2);
	/* negative size fields indicate bit objects */
	sizeField(newObj) = -size;
	return newObj;
}

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

/* do the real work in the decr procedure */
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

/*
Written by Steven Pemberton:
The following routine assures that objects read in are really referenced,
eliminating junk that may be in the object file but not referenced.
It is essentially a marking garbage collector algorithm using the 
reference counts as the mark
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

int objectCount()
{
	register int i, j;
	j = 0;
	for (i = 0; i < ObjectTableMax; i++)
		if (objTableRefCount(i) > 0)
			j++;
	return j;
}

int classInstCount(object aClass)
{
	register int i, j;
	j = 0;
	for (i = 0; i < ObjectTableMax; i++)
		if (objTableRefCount(i) > 0 && objTableClass(i) == aClass)
			j++;
	return j;
}

int maxObjectSize()
{
	int max = 0;
	for (int i = 0; i < FREELISTMAX; i++)
		if (objectFreeList[i] != nilobj) max = i;
	return max;
}

