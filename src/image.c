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


/*
	imageRead - read in an object image
		we toss out the free lists built initially,
		reconstruct the linkages, then rebuild the free
		lists around the new objects.
		The only objects with nonzero reference counts
		will be those reachable from either symbols
*/
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
    object *mBlockAlloc(INT);

	/* First read the symbols table reference */
	ignore fr(fp, (char *)&symbols, sizeof(object));
	i = 0;

	/* Read each object's metadata and data */
	while (fr(fp, (char *)&dummyObject, sizeof(dummyObject)))
	{
		i = dummyObject.di;

		if ((i < 0) || (i > ObjectTableMax))
		{
			sysError("reading index out of range", "");
		}

		setObjTableClass(i, dummyObject.cl);
		if ((dummyObject.cl < 0) || ((dummyObject.cl >> 1) > ObjectTableMax))
		{
			sysError("class out of range", "imageRead");
		}

		size = dummyObject.ds;
		setObjTableSize(i, size);
		adjustSizeIfNeg(size);
		if (size != 0)
		{
			setObjTableMemory(i, mBlockAlloc((int)size));
			ignore fr(fp, (char *)objTableMemory(i), sizeof(object) * (int)size);
		}
		else
		{
			setObjTableMemory(i, (object *)0);
		}
	}

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
 * This function implements the split memory approach where:
 * 1. All object table entries are loaded into RAM
 * 2. Immutable objects (ByteArray, String, Symbol, Block) point to Flash memory
 * 3. Mutable objects are copied into RAM
 * 
 * This approach saves RAM by keeping immutable objects in Flash.
 * 
 * @param fp File pointer to the object table file
 * @param objectData Pointer to the object data in Flash memory
 */
noreturn readTableWithObjects(FILE *fp, void *objectData)
{
	short i, size;
	object *mBlockAlloc(INT);

	// TT_LOG_INFO(TAG, "Reading flash object data from: %d", objectData );

	ignore fr(fp, (char *)&symbols, sizeof(object));
	i = 0;

#ifdef INCLUDE_DEBUG_DATA_FILE
	fprintf(stderr, "Object Header Debug String: %s\n", objectData);
	objectData += strlen(objectDataDebugString) + 1;
#endif

	int numROMObjects = 0;
	int numRAMObjects = 0;
	int totalROMBytes = 0;
	int totalRAMBytes = 0;

	while (fr(fp, (char *)&dummyObject, sizeof(dummyObject)))
	{
		i = dummyObject.di;

		if ((i < 0) || (i > ObjectTableMax))
		{
			sysError("reading index out of range", "");
		}

		setObjTableClass(i, dummyObject.cl);
		if ((dummyObject.cl < 0) || ((dummyObject.cl >> 1) > ObjectTableMax))
		{
			sysError("class out of range", "imageRead");
		}

		size = dummyObject.ds;
		setObjTableSize(i, size);
		adjustSizeIfNeg(size);
		if (size != 0)
		{
			int sizeInBytes = ((int)sizeof(object)) * (int)size;
			// if (dummyObject.flags > 0) {
			if (dummyObject.cl == BYTE_ARRAY_CLASS || dummyObject.cl == STRING_CLASS || dummyObject.cl == SYMBOL_CLASS || dummyObject.cl == BLOCK_CLASS)
			{
				setObjTableMemory(i, (object *)objectData);
				setObjTableRefCount(i, 0x7F);
				numROMObjects++;
				totalROMBytes += sizeInBytes;
			}
			else
			{
				setObjTableMemory(i, mBlockAlloc((int)size));
				memcpy(objTableMemory(i), objectData, sizeInBytes);
				numRAMObjects++;
				totalRAMBytes += sizeInBytes;
			}
			objectData += sizeInBytes;
		}
		else
		{
			setObjTableMemory(i, (object *)0);
		}
	}

	fprintf(stderr, "Number of ROM Object read: %d size in bytes: %d\n", numROMObjects, totalROMBytes);
	fprintf(stderr, "Number of RAM Object read: %d size in bytes: %d\n", numRAMObjects, totalRAMBytes);

	cleanupImage();
    printClassNumbers();
}

/**
 * Reads the object table and object data from separate files
 * 
 * This function loads an image from two separate files:
 * 1. Object table file containing metadata (index, class, size)
 * 2. Object data file containing the actual object content
 * 
 * All objects are loaded into RAM in this approach.
 * 
 * @param fpObjTable File pointer to the object table file
 * @param fpObjData File pointer to the object data file
 */
noreturn readObjectFiles(FILE *fpObjTable, FILE *fpObjData)
{
	short i, size;
	object *mBlockAlloc(INT);

	int numROMObjects = 0;

#ifdef INCLUDE_DEBUG_DATA_FILE
	char debugString[20];
	ignore fr(fpObjData, debugString, strlen(objectDataDebugString) + 1);
	fprintf(stderr, "Object Header Debug Length: %d String: %s\n", strlen(debugString), debugString);
#endif

	ignore fr(fpObjTable, (char *)&symbols, sizeof(object));
	i = 0;

	while (fr(fpObjTable, (char *)&dummyObject, sizeof(dummyObject)))
	{
		i = dummyObject.di;

		if ((i < 0) || (i > ObjectTableMax))
		{
			sysError("reading index out of range", "");
		}
		setObjTableClass(i, dummyObject.cl);
		if ((dummyObject.cl < 0) || ((dummyObject.cl >> 1) > ObjectTableMax))
		{
			fprintf(stderr, "index %d\n", dummyObject.cl);
			sysError("class out of range", "imageRead");
		}

		if (dummyObject.flags > 0)
			numROMObjects++;

		size = dummyObject.ds;
		setObjTableSize(i, size);
		adjustSizeIfNeg(size);
		if (size != 0)
		{
			setObjTableMemory(i, mBlockAlloc((int)size));
			ignore fr(fpObjData, (char *)objTableMemory(i), sizeof(object) * (int)size);
		}
		else
		{
			setObjTableMemory(i, (object *)0);
		}
	}

	fprintf(stderr, "Number of ROM Objects: %d\n", numROMObjects);

#ifdef INCLUDE_DEBUG_DATA_FILE
	ignore fr(fpObjData, debugString, strlen(objectDataDebugString) + 1);
	fprintf(stderr, "Object Footer Debug Length: %d String: %s\n", strlen(debugString), debugString);
#endif

	cleanupImage();
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
 */
noreturn setDummyObjFlags()
{
	object byteArrayClass = findClass("ByteArray");
	object stringClass = findClass("String");
	object symbolClass = findClass("Symbol");
	object blockClass = findClass("Block");

	// fprintf(stderr, "ByteArray Class: %d\n", byteArrayClass);
	if (dummyObject.cl == byteArrayClass || dummyObject.cl == stringClass || dummyObject.cl == blockClass || dummyObject.cl == symbolClass)
	{
		// fprintf(stderr, "Matched class: %d\n", dummyObject.cl);
		dummyObject.flags = DUMMY_OBJ_FLAG_ROM;
	}
	else
	{
		dummyObject.flags = 0;
	}
}

/**
 * Writes only the object table to a file
 * 
 * This function writes the metadata for all objects (index, class, size, flags)
 * to a file, but not the actual object data. Used in the split approach
 * where object data is stored separately.
 * 
 * @param fp File pointer to write the object table to
 */
noreturn writeObjectTable(FILE *fp)
{
	fw(fp, (char *)&symbols, sizeof(object));

	int numROMObjects = 0;
	int numTotalObjects = 0;
	for (short i = 0; i < ObjectTableMax; i++)
	{
		if (objTableRefCount(i) > 0)
		{
			dummyObject.di = i;
			dummyObject.cl = objTableClass(i);
			dummyObject.ds = objTableSize(i);
			setDummyObjFlags();
			if (dummyObject.flags > 0)
				numROMObjects++;
			fw(fp, (char *)&dummyObject, sizeof(dummyObject));
			numTotalObjects++;
		}
	}
	fprintf(stderr, "Number of ROM Object written: %d total objects: %d\n", numROMObjects, numTotalObjects);
}

/**
 * Writes only the object data to a file
 * 
 * This function writes the actual content of all objects to a file,
 * but not their metadata. Used in the split approach where object table
 * is stored separately.
 * 
 * @param fp File pointer to write the object data to
 */
noreturn writeObjectData(FILE *fp)
{
	short i, size;

#ifdef INCLUDE_DEBUG_DATA_FILE
	fw(fp, objectDataDebugString, strlen(objectDataDebugString) + 1);
#endif

	for (i = 0; i < ObjectTableMax; i++)
	{
		if (objTableRefCount(i) > 0)
		{
			size = objTableSize(i);
			adjustSizeIfNeg(size);
			if (size != 0)
			{
				fw(fp, (char *)objTableMemory(i), sizeof(object) * size);
			}
		}
	}

#ifdef INCLUDE_DEBUG_DATA_FILE
	fw(fp, objectDataDebugString, strlen(objectDataDebugString) + 1);
#endif
}

/**
 * Writes a complete Smalltalk image to a file
 * 
 * This function writes both the object metadata and data to a single file.
 * The image format consists of:
 * 1. The symbols table object reference
 * 2. A sequence of object metadata entries (index, class, size)
 * 3. The object data for each entry with non-zero size
 * 
 * @param fp File pointer to write the image to
 */
noreturn imageWrite(FILE *fp)
{
	short i, size;

	fw(fp, (char *)&symbols, sizeof(object));

	for (i = 0; i < ObjectTableMax; i++)
	{
		// writeObject(i, fp);
		if (objTableRefCount(i) > 0)
		{
			dummyObject.di = i;
			dummyObject.cl = objTableClass(i);
			dummyObject.ds = size = objTableSize(i);
			fw(fp, (char *)&dummyObject, sizeof(dummyObject));
			adjustSizeIfNeg(size);
			if (size != 0)
			{
				fw(fp, (char *)objTableMemory(i), sizeof(object) * size);
			}
		}
	}
}
