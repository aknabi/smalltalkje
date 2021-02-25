/*
	Little Smalltalk, version 2

	Unix specific input and output routines
	written by tim budd, January 1988
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

#define DUMMY_OBJ_FLAG_ROM 0x01;

#define BYTE_ARRAY_CLASS 18
#define STRING_CLASS 34
#define SYMBOL_CLASS 8
#define BLOCK_CLASS 182
#define METHOD_CLASS 264

struct
{
	int di;
	object cl;
	short ds;
	short flags;
} dummyObject;

/*
	imageRead - read in an object image
		we toss out the free lists built initially,
		reconstruct the linkages, then rebuild the free
		lists around the new objects.
		The only objects with nonzero reference counts
		will be those reachable from either symbols
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

noreturn imageRead(FILE *fp)
{
	short i, size;
    object *mBlockAlloc(INT);

	ignore fr(fp, (char *)&symbols, sizeof(object));
	i = 0;

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

	/* now restore ref counts, getting rid of unneeded junk */
	visit(symbols);
	/* toss out the old free lists, build new ones */
	setFreeLists();
}

// #define INCLUDE_DEBUG_DATA_FILE

#ifdef INCLUDE_DEBUG_DATA_FILE
static const char *objectDataDebugString = "OBJECT_FILE_DEBUG";
#endif

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
			if (dummyObject.cl == BYTE_ARRAY_CLASS || dummyObject.cl == STRING_CLASS || dummyObject.cl == SYMBOL_CLASS || dummyObject.cl == BLOCK_CLASS || dummyObject.cl == METHOD_CLASS)
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

	/* now restore ref counts, getting rid of unneeded junk */
	visit(symbols);
	/* toss out the old free lists, build new ones */
	setFreeLists();

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
}

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

	/* now restore ref counts, getting rid of unneeded junk */
	visit(symbols);
	/* toss out the old free lists, build new ones */
	setFreeLists();
}

/*
	imageWrite - write out an object image
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

/* i/o primitives - necessarily rather UNIX dependent;
	basically, files are all kept in a large array.
	File operations then just give an index into this array 
*/
#define MAXFILES 20
/* we assume this is initialized to NULL */
static FILE *fp[MAXFILES];

// Functions to support command line input

object lastInputLine = nilobj;
extern boolean _interruptInterpreter;

extern char getInputCharacter(void);

object getInputLine(char *prompt)
{
	char c = 0;

	size_t bufsize = 80;
	int bufIndex = 0;
	char buffer[bufsize];

	boolean lineDone = false;

	while (!lineDone && c == 0)
	{
		// c = fgetc(stdin);
		c = getInputCharacter();
		if (c > 0)
		{
			if (c == 0x08)
			{
				if (bufIndex >= 0)
				{
					if (bufIndex > 0)
						bufIndex--;
					buffer[bufIndex] = c;
					putchar(0x8);
					putchar(0x20);
					putchar(0x8);
				}
			}
			else if (c != 0x0D)
			{
				if (c == 0xA)
				{
					lineDone = true;
				}
				else
				{
					buffer[bufIndex++] = c;
				}
				putchar(c);
			}
			c = 0;
			// printf("Buffer: %s\n", buffer);
			fflush(stdout);
		}
#ifdef TARGET_ESP32
		vTaskDelay(5);
#endif
		// Check for the VM Interrupt flag and bounce out if true
	}

	buffer[bufIndex] = 0;
	// since we're keeping a vm reference, decrement pointer if an old line
	// if (lastInputLine != nilobj)
	// 	decr(lastInputLine);
	lastInputLine = newStString(buffer);
	// since we're keeping a vm reference, increment the pointer
	// incr(lastInputLine);
	return lastInputLine;
}

object ioPrimitive(int number, object *arguments)
{
	int i, j;
	char *p, buffer[1024];
	object returnedObject;

	returnedObject = nilobj;

	i = intValue(arguments[0]);

	switch (number)
	{
	case 0: /* file open */
		i = intValue(arguments[0]);
		p = charPtr(arguments[1]);
		if (streq(p, "stdin"))
			fp[i] = stdin;
		else if (streq(p, "stdout"))
			fp[i] = stdout;
		else if (streq(p, "stderr"))
			fp[i] = stderr;
		else
		{
			fp[i] = fopen(p, charPtr(arguments[2]));
		}
		if (fp[i] == NULL)
			returnedObject = nilobj;
		else
			returnedObject = newInteger(i);
		break;

	case 1: /* file close - recover slot */
		if (fp[i])
			ignore fclose(fp[i]);
		fp[i] = NULL;
		break;

	case 2: /* file size */
	case 3: /* file in */
		if (fp[i])
			fileIn(fp[i], true);
		break;

	case 4: /* prim 124 get a input line from the console (blocking/nonblocking) */
		returnedObject = getInputLine(charPtr(arguments[0]));
		break;

	case 5: /* prim 125 - get string */
		if (!fp[i])
			break;
		j = 0;
		buffer[j] = '\0';
		while (true)
		{
			if (fgets(&buffer[j], 512, fp[i]) == NULL)
				return (nilobj); /* end of file */
			if (fp[i] == stdin)
			{
				/* delete the newline */
				j = (int) strlen(buffer);
				if (buffer[j - 1] == '\n')
					buffer[j - 1] = '\0';
			}
			j = (int) strlen(buffer) - 1;
			if (buffer[j] != '\\')
				break;
			/* else we loop again */
		}
		returnedObject = newStString(buffer);
		break;

	case 6: /* prim 126 get the last input line from the console */
		returnedObject = lastInputLine;
		break;

	case 7: /* prim 127 - write an object image */
		if (fp[i])
			imageWrite(fp[i]);
		returnedObject = trueobj;
		break;

	case 8: /* prim 128 - print no return */
	case 9: /* prim 129 - print string */
		if (!fp[i])
			break;
		ignore fputs(charPtr(arguments[1]), fp[i]);
		if (number == 8)
		{
			ignore fflush(fp[i]);
		}
		else
		{
			ignore fputc('\n', fp[i]);
		}
		break;

	case 10: /* primtive 130: write the object table to file */
		if (fp[i])
			writeObjectTable(fp[i]);
		returnedObject = trueobj;
		break;

	case 11: /* primtive 131: write the object data to file */
		if (fp[i])
			writeObjectData(fp[i]);
		returnedObject = trueobj;
		break;

	case 12: /* primitive 132: get a single character from console (or 0 if timeout) */

		returnedObject = newInteger(getInputCharacter());

		// // if (arguments[0] != nilobj) {
		// // 	setRefCountField(arguments[0], 1);
		// // 	decr(arguments[0]);
		// // }

		// // decr(arguments[0]);
		// c = 0;
		// while (c == 0) {
		// 	c = getInputCharacter();
		// }
		// returnedObject = newChar(c);
		// // returnedObject = newInteger(getInputCharacter());

		break;

	case 13: /* prim 133: print the char of the integer passed in */
		putc(intValue(arguments[0]), stdout);
		fflush(stdout);
		break;

	default:
		sysError("unknown primitive", "filePrimitive");
	}

	return (returnedObject);
}
