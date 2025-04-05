/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021
	Based on Little Smalltalk, version 2

	Image Management Module - Object Persistence
	
	This module handles the loading and saving of object images.
	It provides functionality for:
	- Reading a complete Smalltalk image from a file
	- Reading object table and data separately
	- Mapping objects between RAM and ROM (Flash memory)
	- Writing object images to files for persistence
	
	The image system in Smalltalkje uses a split approach where:
	- Object tables store metadata (class, size, etc.) and are loaded into RAM
	- Object data can either be:
	  a) Loaded entirely into RAM (traditional approach)
	  b) Split with some objects in RAM and others in Flash ROM
	    (particularly ByteArrays, Strings, Symbols, and Blocks)
	
	Originally written by Tim Budd, January 1988
	Modified extensively for ESP32 support
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Target defines (e.g. mac, esp32)
#include "build.h"
#include "target.h"

#include "env.h"
#include "memory.h"
#include "names.h"

#ifdef TARGET_ESP32

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "driver/uart.h"

#endif // TARGET_ESP32


void visit(register object x);
void setFreeLists(void);
void fileIn(FILE *fd, boolean printit);

/* Flag to mark objects that should be stored in ROM (flash memory) */
#define DUMMY_OBJ_FLAG_ROM 0x01;

/* Class indices in the object table for common classes
   These constants are used to identify specific classes during
   image loading/saving to determine which objects can be 
   stored in ROM vs RAM */
#define BYTE_ARRAY_CLASS 18  /* ByteArray instances can be stored in ROM */
#define STRING_CLASS 34      /* String instances can be stored in ROM */
#define SYMBOL_CLASS 8       /* Symbol instances can be stored in ROM */
#define BLOCK_CLASS 182      /* Block instances can be stored in ROM */
#define METHOD_CLASS 264     /* Methods have special handling in some cases */
#define CLASS_CLASS 10       /* Class constant for Class objects */

/* Temporary structure used during image file operations 
   This holds the metadata for a single object when reading/writing
   the object table to/from a file */
struct
{
	int di;        /* Object index in the object table */
	object cl;     /* Class of the object (as object reference) */
	short ds;      /* Size of the object (number of object slots) */
	short flags;   /* Flags for special handling (e.g., ROM storage) */
} dummyObject;


/**
 * Utility function for reading data from an image file
 * 
 * This is a wrapper around fread() that performs error checking
 * to ensure the correct amount of data was read.
 * 
 * @param fp File pointer to read from
 * @param p Pointer to buffer where data will be stored
 * @param s Size of each element to read
 * @return Number of items read (typically 1, or 0 on EOF)
 */
static int fr(FILE *fp, char *p, int s)
{
	int r;
	r = (int) fread(p, s, 1, fp);
	if (r && (r != 1))
	{
		sysError("imageRead count error", "");
	}
	return r;
}

/**
 * Finalizes the image after loading
 * 
 * This function performs two critical tasks after reading an image:
 * 1. Restores reference counts by visiting all objects reachable from the symbols table,
 *    which ensures only needed objects are kept in memory
 * 2. Rebuilds the free lists after discarding unreachable objects
 */
noreturn cleanupImage(void)
{
	/* now restore ref counts, getting rid of unneeded junk */
	visit(symbols);
	/* toss out the old free lists, build new ones */
	setFreeLists();
}


/**
 * Loads a complete Smalltalk image from a file
 * 
 * This function reads an image file containing the entire Smalltalk environment.
 * The image format consists of:
 * 1. The symbols table object reference
 * 2. A sequence of object metadata entries (index, class, size, flags)
 * 3. The object data for each entry
 * 
 * All objects are loaded into RAM using this function (traditional approach).
 * 
 * @param fp File pointer to the image file
 */
noreturn imageRead(FILE *fp)
{
	short i, size;
    object *mBlockAlloc(INT); /* Memory block allocator function */

	/* First read the symbols table reference - this is the root object 
	   for the entire Smalltalk environment */
	ignore fr(fp, (char *)&symbols, sizeof(object));
	i = 0;

	/* Read each object's metadata and data by iterating through the file.
	   The fr() function returns 0 at EOF, terminating the loop. */
	while (fr(fp, (char *)&dummyObject, sizeof(dummyObject)))
	{
		i = dummyObject.di; /* Get the object index in the table */

		/* Validate object index is within allowed range */
		if ((i < 0) || (i > ObjectTableMax))
		{
			sysError("reading index out of range", "");
		}

		/* Set the class for this object in the object table */
		setObjTableClass(i, dummyObject.cl);
		
		/* Validate class reference is within range
		   The >> 1 operation is because object references have their LSB set to 1
		   to distinguish them from integers in the Smalltalk VM */
		if ((dummyObject.cl < 0) || ((dummyObject.cl >> 1) > ObjectTableMax))
		{
			sysError("class out of range", "imageRead");
		}

		/* Set the size for this object in the object table */
		size = dummyObject.ds;
		setObjTableSize(i, size);
		
		/* Adjust size if negative (special case for certain object types) */
		adjustSizeIfNeg(size);
		
		if (size != 0)
		{
			/* Allocate memory for the object data and read it from the file
			   objTableMemory(i) returns the pointer to memory for object i */
			setObjTableMemory(i, mBlockAlloc((int)size));
			ignore fr(fp, (char *)objTableMemory(i), sizeof(object) * (int)size);
		}
		else
		{
			/* For zero-size objects, just set a null pointer */
			setObjTableMemory(i, (object *)0);
		}
	}

	/* After loading all objects, restore reference counts and free lists */
	cleanupImage();
}

// #define INCLUDE_DEBUG_DATA_FILE

#ifdef INCLUDE_DEBUG_DATA_FILE
static const char *objectDataDebugString = "OBJECT_FILE_DEBUG";
#endif

/**
 * Debug function to print the object IDs of important classes
 * 
 * This function looks up and prints the object table indices for
 * several commonly used classes, which is helpful for debugging
 * the image loading/saving process.
 */
noreturn printClassNumbers()
{
    object byteArrayClass = findClass("ByteArray");
    fprintf(stderr, "ByteArray Class: %d\n", byteArrayClass);
    object stringClass = findClass("String");
    fprintf(stderr, "String Class: %d\n", stringClass);
    object listClass = findClass("List");
    fprintf(stderr, "List Class: %d\n", listClass);
    object arrayClass = findClass("Array");
    fprintf(stderr, "Array Class: %d\n", arrayClass);
    object setClass = findClass("Set");
    fprintf(stderr, "Set Class: %d\n", setClass);
    object blockClass = findClass("Block");
    fprintf(stderr, "Block Class: %d\n", blockClass);
    object methodClass = findClass("Method");
    fprintf(stderr, "Method Class: %d\n", methodClass);
    object classClass = findClass("Class");
    fprintf(stderr, "Class Class: %d\n", classClass);
}


/**
 * Specialized image reader that loads object table with data from Flash
 * 
 * This function implements the memory-saving split approach where:
 * 1. All object table entries are loaded into RAM
 * 2. Immutable objects (ByteArray, String, Symbol, Block) point to Flash memory
 * 3. Mutable objects are copied into RAM
 * 
 * This is particularly important for ESP32 and other memory-constrained devices,
 * as it allows large parts of the image to remain in Flash memory rather than
 * consuming precious RAM.
 * 
 * @param fp File pointer to the object table file
 * @param objectData Pointer to the object data in Flash memory
 */
noreturn readTableWithObjects(FILE *fp, void *objectData)
{
	short i, size;
	object *mBlockAlloc(INT); /* Memory allocator function */

	// TT_LOG_INFO(TAG, "Reading flash object data from: %d", objectData );

	/* Read the symbols table reference - the root object for the environment */
	ignore fr(fp, (char *)&symbols, sizeof(object));
	i = 0;

#ifdef INCLUDE_DEBUG_DATA_FILE
	/* Skip past the debug header if present */
	fprintf(stderr, "Object Header Debug String: %s\n", objectData);
	objectData += strlen(objectDataDebugString) + 1;
#endif

	/* Counters to track memory usage statistics */
	int numROMObjects = 0;    /* Number of objects kept in Flash/ROM */
	int numRAMObjects = 0;    /* Number of objects copied to RAM */
	int totalROMBytes = 0;    /* Total bytes of objects in Flash/ROM */
	int totalRAMBytes = 0;    /* Total bytes of objects in RAM */

	/* Process each object entry from the object table file */
	while (fr(fp, (char *)&dummyObject, sizeof(dummyObject)))
	{
		i = dummyObject.di; /* Object index */

		/* Validate the object index is within bounds */
		if ((i < 0) || (i > ObjectTableMax))
		{
			sysError("reading index out of range", "");
		}

		/* Set the class for this object */
		setObjTableClass(i, dummyObject.cl);
		
		/* Validate the class reference - references have LSB set to distinguish from integers */
		if ((dummyObject.cl < 0) || ((dummyObject.cl >> 1) > ObjectTableMax))
		{
			sysError("class out of range", "imageRead");
		}

		/* Set the size for this object */
		size = dummyObject.ds;
		setObjTableSize(i, size);
		adjustSizeIfNeg(size);
		
		if (size != 0)
		{
			int sizeInBytes = ((int)sizeof(object)) * (int)size;
			
			/* CRITICAL MEMORY OPTIMIZATION: 
			   The key optimization in this function - determine if the object
			   should remain in Flash memory (ROM) or be copied to RAM.
			   
			   Immutable objects (ByteArray, String, Symbol, Block) can safely remain in Flash
			   since they will never be modified. This saves significant RAM. */
			   
			/* Check if the object is of a class that can be kept in ROM */
			if (dummyObject.cl == BYTE_ARRAY_CLASS || 
			    dummyObject.cl == STRING_CLASS || 
			    dummyObject.cl == SYMBOL_CLASS || 
			    dummyObject.cl == BLOCK_CLASS)
			{
				/* For ROM-eligible objects:
				   1. Point the object table entry directly to the Flash memory location
				   2. Set reference count to 0x7F (maximum) to prevent garbage collection */
				setObjTableMemory(i, (object *)objectData);
				setObjTableRefCount(i, 0x7F); /* Mark as permanent - prevents GC */
				numROMObjects++;
				totalROMBytes += sizeInBytes;
			}
			else
			{
				/* For objects that need to be mutable:
				   1. Allocate RAM for the object
				   2. Copy the object data from Flash to RAM */
				setObjTableMemory(i, mBlockAlloc((int)size));
				memcpy(objTableMemory(i), objectData, sizeInBytes);
				numRAMObjects++;
				totalRAMBytes += sizeInBytes;
			}
			
			/* Move the objectData pointer to the next object in Flash */
			objectData += sizeInBytes;
		}
		else
		{
			/* For zero-size objects, just set a null pointer */
			setObjTableMemory(i, (object *)0);
		}
	}

	/* Output memory usage statistics */
	fprintf(stderr, "Number of ROM Object read: %d size in bytes: %d\n", numROMObjects, totalROMBytes);
	fprintf(stderr, "Number of RAM Object read: %d size in bytes: %d\n", numRAMObjects, totalRAMBytes);

	/* Restore reference counts and rebuild free lists */
	cleanupImage();
	/* Debug output - print the object IDs of important classes */
    printClassNumbers();
}

/**
 * Reads the object table and object data from separate files
 * 
 * This function loads an image from two separate files, which allows more 
 * flexibility in image management:
 * 1. Object table file containing metadata (index, class, size, flags)
 * 2. Object data file containing the actual object content
 * 
 * Unlike readTableWithObjects which keeps some objects in Flash memory,
 * this function loads all objects into RAM. It still tracks which objects
 * could have been kept in ROM via their flags, but doesn't actually implement
 * the split memory strategy.
 * 
 * This approach is useful for debugging or when preparing image files for
 * later use with the split memory approach.
 * 
 * @param fpObjTable File pointer to the object table file
 * @param fpObjData File pointer to the object data file
 */
noreturn readObjectFiles(FILE *fpObjTable, FILE *fpObjData)
{
	short i, size;
	object *mBlockAlloc(INT); /* Memory allocator function */

	/* Counter for objects marked as ROM-eligible in the flags */
	int numROMObjects = 0;

#ifdef INCLUDE_DEBUG_DATA_FILE
	/* Read and verify the debug header marker if it's enabled */
	char debugString[20];
	ignore fr(fpObjData, debugString, strlen(objectDataDebugString) + 1);
	fprintf(stderr, "Object Header Debug Length: %d String: %s\n", strlen(debugString), debugString);
#endif

	/* Read the symbols table reference - the root object for the environment */
	ignore fr(fpObjTable, (char *)&symbols, sizeof(object));
	i = 0;

	/* Read each object entry from the object table file */
	while (fr(fpObjTable, (char *)&dummyObject, sizeof(dummyObject)))
	{
		i = dummyObject.di; /* Get object index */

		/* Validate the object index is within allowed range */
		if ((i < 0) || (i > ObjectTableMax))
		{
			sysError("reading index out of range", "");
		}
		
		/* Set the class for this object */
		setObjTableClass(i, dummyObject.cl);
		
		/* Validate the class reference is valid
		   The >> 1 operation handles the tag bit in object references */
		if ((dummyObject.cl < 0) || ((dummyObject.cl >> 1) > ObjectTableMax))
		{
			fprintf(stderr, "index %d\n", dummyObject.cl);
			sysError("class out of range", "imageRead");
		}

		/* Count objects that are marked as ROM-eligible via their flags */
		if (dummyObject.flags > 0)
			numROMObjects++;

		/* Set the size for this object */
		size = dummyObject.ds;
		setObjTableSize(i, size);
		adjustSizeIfNeg(size);
		
		if (size != 0)
		{
			/* Allocate RAM for the object and read its data from the data file */
			setObjTableMemory(i, mBlockAlloc((int)size));
			ignore fr(fpObjData, (char *)objTableMemory(i), sizeof(object) * (int)size);
		}
		else
		{
			/* For zero-size objects, just set a null pointer */
			setObjTableMemory(i, (object *)0);
		}
	}

	/* Report how many objects were identified as ROM-eligible via their flags */
	fprintf(stderr, "Number of ROM Objects: %d\n", numROMObjects);

#ifdef INCLUDE_DEBUG_DATA_FILE
	/* Read and verify the debug footer marker if it's enabled */
	ignore fr(fpObjData, debugString, strlen(objectDataDebugString) + 1);
	fprintf(stderr, "Object Footer Debug Length: %d String: %s\n", strlen(debugString), debugString);
#endif

	/* Restore reference counts and rebuild free lists */
	cleanupImage();
	/* Debug output - print the object IDs of important classes */
    printClassNumbers();
}

/*
	imageWrite - write out an object image
*/

/**
 * Utility function for writing data to an image file
 * 
 * This is a wrapper around fwrite() that performs error checking
 * to ensure the correct amount of data was written.
 * 
 * @param fp File pointer to write to
 * @param p Pointer to data to be written
 * @param s Size of each element to write
 */
static void fw(FILE *fp, char *p, int s)
{
	if (fwrite(p, s, 1, fp) != 1)
	{
		sysError("imageWrite size error", "");
	}
}

/*
noreturn writeObject(short i, FILE *fp) 
{
	short size;

	if (objTableRefCount(i) > 0) {
	    dummyObject.di = i;
	    dummyObject.cl = objTableClass(i);
	    dummyObject.ds = size = objTableSize(i);
	    fw(fp, (char *) &dummyObject, sizeof(dummyObject));
		adjustSizeIfNeg(size);
	    if (size != 0)
			fw(fp, (char *) objTableMemory(i), sizeof(object) * size);
	}
}
*/

/**
 * Sets flags in the dummyObject structure for ROM-eligible objects
 * 
 * This function determines if the current object (in dummyObject)
 * should be stored in ROM based on its class. Only immutable objects
 * like ByteArrays, Strings, Symbols, and Blocks are marked for ROM storage.
 * 
 * The implementation involves:
 * 1. Finding the class objects for the ROM-eligible classes
 * 2. Comparing the current object's class with these ROM-eligible classes
 * 3. Setting the appropriate flag if it's one of these special classes
 * 
 * This is an important part of the memory optimization strategy for ESP32,
 * as it identifies objects that can safely remain in Flash memory.
 */
noreturn setDummyObjFlags()
{
	/* Look up the class objects for ROM-eligible classes */
	object byteArrayClass = findClass("ByteArray");
	object stringClass = findClass("String");
	object symbolClass = findClass("Symbol");
	object blockClass = findClass("Block");

	/* Check if the current object belongs to one of the ROM-eligible classes */
	if (dummyObject.cl == byteArrayClass || 
	    dummyObject.cl == stringClass || 
	    dummyObject.cl == blockClass || 
	    dummyObject.cl == symbolClass)
	{
		/* Set the ROM flag for this object, marking it for Flash storage */
		dummyObject.flags = DUMMY_OBJ_FLAG_ROM;
	}
	else
	{
		/* Not ROM-eligible, clear the flags */
		dummyObject.flags = 0;
	}
}

/**
 * Writes only the object table to a file
 * 
 * This function writes the metadata for all objects (index, class, size, flags)
 * to a file, but not the actual object data. It's used in the split approach
 * where object data is stored separately.
 * 
 * The implementation:
 * 1. First writes the symbols table reference (root object)
 * 2. Iterates through the entire object table to find live objects (refCount > 0)
 * 3. For each live object:
 *    a. Prepares the dummyObject structure with metadata
 *    b. Calls setDummyObjFlags() to mark ROM-eligible objects
 *    c. Writes the object metadata to the file
 * 4. Tracks and reports statistics about ROM-eligible objects
 * 
 * This approach enables the "split memory" optimization for ESP32,
 * where immutable objects can be kept in Flash memory.
 * 
 * @param fp File pointer to write the object table to
 */
noreturn writeObjectTable(FILE *fp)
{
	/* Write the symbols table reference (root object) */
	fw(fp, (char *)&symbols, sizeof(object));

	/* Counters for statistics */
	int numROMObjects = 0;     /* Number of ROM-eligible objects */
	int numTotalObjects = 0;   /* Total number of objects written */
	
	/* Iterate through all possible object indices */
	for (short i = 0; i < ObjectTableMax; i++)
	{
		/* Only process live objects (those with non-zero reference counts) */
		if (objTableRefCount(i) > 0)
		{
			/* Fill the dummyObject structure with this object's metadata */
			dummyObject.di = i;                     /* Object index */
			dummyObject.cl = objTableClass(i);      /* Class reference */
			dummyObject.ds = objTableSize(i);       /* Size (in object slots) */
			
			/* Determine if this object can be stored in ROM */
			setDummyObjFlags();
			
			/* Count ROM-eligible objects */
			if (dummyObject.flags > 0)
				numROMObjects++;
				
			/* Write the metadata for this object */
			fw(fp, (char *)&dummyObject, sizeof(dummyObject));
			numTotalObjects++;
		}
	}
	
	/* Report statistics on ROM vs RAM objects */
	fprintf(stderr, "Number of ROM Object written: %d total objects: %d\n", numROMObjects, numTotalObjects);
}

/**
 * Writes only the object data to a file
 * 
 * This function writes the actual content of all live objects to a file,
 * but not their metadata. It's the complementary function to writeObjectTable()
 * in the split approach for memory optimization.
 * 
 * The implementation:
 * 1. Optionally writes a debug header marker for data validation
 * 2. Iterates through the entire object table to find live objects (refCount > 0)
 * 3. For each live object with non-zero size:
 *    a. Writes the raw object data from memory to the file
 * 4. Optionally writes a debug footer marker
 * 
 * In the split memory approach, this object data file can be embedded in Flash,
 * and the readTableWithObjects() function can selectively map certain objects
 * directly to Flash while copying others to RAM.
 * 
 * @param fp File pointer to write the object data to
 */
noreturn writeObjectData(FILE *fp)
{
	short i, size;

#ifdef INCLUDE_DEBUG_DATA_FILE
	/* Write debug header marker if debugging is enabled */
	fw(fp, objectDataDebugString, strlen(objectDataDebugString) + 1);
#endif

	/* Iterate through all possible object indices */
	for (i = 0; i < ObjectTableMax; i++)
	{
		/* Only process live objects (those with non-zero reference counts) */
		if (objTableRefCount(i) > 0)
		{
			/* Get the object's size and adjust if necessary (special case for negative sizes) */
			size = objTableSize(i);
			adjustSizeIfNeg(size);
			
			/* Only write data for objects with non-zero size */
			if (size != 0)
			{
				/* Write the raw object data from memory to the file */
				fw(fp, (char *)objTableMemory(i), sizeof(object) * size);
			}
		}
	}

#ifdef INCLUDE_DEBUG_DATA_FILE
	/* Write debug footer marker if debugging is enabled */
	fw(fp, objectDataDebugString, strlen(objectDataDebugString) + 1);
#endif
}

/**
 * Writes a complete Smalltalk image to a file
 * 
 * This function writes both the object metadata and data to a single file,
 * representing the traditional approach for saving a Smalltalk image.
 * 
 * Unlike the split approach (writeObjectTable and writeObjectData), this function:
 * 1. Creates a single file containing both metadata and object data
 * 2. Does not perform any ROM/RAM classification of objects
 * 3. Is simpler but uses more RAM when the image is loaded later
 * 
 * The image format consists of:
 * 1. The symbols table object reference (root object)
 * 2. A sequence of object metadata entries (index, class, size)
 * 3. The object data for each entry with non-zero size, immediately following its metadata
 * 
 * This is the original image format from Little Smalltalk, preserved for
 * compatibility with systems that don't need the memory optimizations
 * of the split approach.
 * 
 * @param fp File pointer to write the image to
 */
noreturn imageWrite(FILE *fp)
{
	short i, size;

	/* Write the symbols table reference (root object for the environment) */
	fw(fp, (char *)&symbols, sizeof(object));

	/* Iterate through all possible object indices */
	for (i = 0; i < ObjectTableMax; i++)
	{
		/* Only process live objects (those with non-zero reference counts) */
		if (objTableRefCount(i) > 0)
		{
			/* Prepare metadata for this object */
			dummyObject.di = i;                     /* Object index */
			dummyObject.cl = objTableClass(i);      /* Class reference */
			dummyObject.ds = size = objTableSize(i); /* Size (in object slots) */
			
			/* Write the metadata for this object */
			fw(fp, (char *)&dummyObject, sizeof(dummyObject));
			
			/* Adjust size if negative (special case for certain object types) */
			adjustSizeIfNeg(size);
			
			/* Only write data for objects with non-zero size */
			if (size != 0)
			{
				/* Write the raw object data from memory to the file */
				fw(fp, (char *)objTableMemory(i), sizeof(object) * size);
			}
		}
	}
}
