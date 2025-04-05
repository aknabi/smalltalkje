/*
	Smalltalkje, version 1 based on:
	Little Smalltalk, version 2

	File Input/Output Implementation
	
	This module provides file I/O functionality for the Smalltalkje system.
	It handles file operations such as opening, closing, reading, and writing,
	as well as command-line input processing. The implementation includes both
	standard POSIX file operations and ESP32-specific adaptations.
	
	The module exposes its functionality to Smalltalk through primitive functions
	that can be called from Smalltalk code, allowing interaction with the file system
	and terminal input.
	
	Originally written by Tim Budd, January 1988
	Updated for embedded systems by Abdul Nabi, March 2021
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

/**
 * File I/O primitives
 * 
 * This implementation is somewhat UNIX-dependent. Files are kept in a
 * large array, and file operations use indices into this array.
 * 
 * The maximum number of open files is defined by MAXFILES.
 */
#define MAXFILES 20

/** Array of file pointers (initialized to NULL) */
static FILE *fp[MAXFILES];

/** Last line of input from the command line */
object lastInputLine = nilobj;

/** Flag to interrupt the interpreter */
extern boolean _interruptInterpreter;

/** Forward declarations */
extern char getInputCharacter(void);
extern void fileIn(FILE *fd, boolean printit);
extern noreturn writeObjectTable(FILE *fp);
extern noreturn writeObjectData(FILE *fp);

/**
 * Get a line of input from the user
 * 
 * This function reads a line of input from the terminal, handling
 * backspace and other control characters appropriately. It returns
 * a Smalltalk string object containing the input line.
 * 
 * @param prompt The prompt to display (unused)
 * @return A Smalltalk string object containing the input line
 */
object getInputLine(char *prompt)
{
	char c = 0;
	size_t bufsize = 80;
	int bufIndex = 0;
	char buffer[bufsize];
	boolean lineDone = false;

	while (!lineDone && c == 0)
	{
		// Get a character from input
		c = getInputCharacter();
		
		if (c > 0)
		{
			if (c == 0x08)  // Backspace
			{
				if (bufIndex >= 0)
				{
					if (bufIndex > 0)
						bufIndex--;
					buffer[bufIndex] = c;
					
					// Handle backspace display (move back, print space, move back again)
					putchar(0x8);
					putchar(0x20);
					putchar(0x8);
				}
			}
			else if (c != 0x0D)  // Not carriage return
			{
				if (c == 0xA)  // Line feed (newline)
				{
					lineDone = true;
				}
				else
				{
					// Add character to buffer
					buffer[bufIndex++] = c;
				}
				putchar(c);
			}
			
			c = 0;  // Reset character for next iteration
			fflush(stdout);
		}
		
#ifdef TARGET_ESP32
		// Give other tasks a chance to run
		vTaskDelay(5);
#endif
	}

	// Null-terminate the buffer
	buffer[bufIndex] = 0;
	
	// Create a new Smalltalk string object with the input line
	lastInputLine = newStString(buffer);
	
	return lastInputLine;
}

/**
 * File I/O primitive functions
 * 
 * This function implements various file I/O primitive operations that
 * can be called from Smalltalk code. It handles file opening, closing,
 * reading, writing, and other operations.
 * 
 * @param number The primitive number to execute
 * @param arguments Array of Smalltalk objects as arguments
 * @return Result of the primitive operation
 */
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
		// Get file index and path
		i = intValue(arguments[0]);
		p = charPtr(arguments[1]);
		
		// Handle standard streams
		if (streq(p, "stdin"))
			fp[i] = stdin;
		else if (streq(p, "stdout"))
			fp[i] = stdout;
		else if (streq(p, "stderr"))
			fp[i] = stderr;
		else
		{
			// Open regular file with specified mode
			fp[i] = fopen(p, charPtr(arguments[2]));
		}
		
		// Return file index or nil if failed
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
		// Process a Smalltalk source file
		if (fp[i])
			fileIn(fp[i], true);
		break;

	case 4: /* prim 124 get a input line from the console (blocking/nonblocking) */
		returnedObject = getInputLine(charPtr(arguments[0]));
		break;

	case 5: /* prim 125 - get string */
		if (!fp[i])
			break;
			
		// Read a string from file, handling line continuation with backslash
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
			/* else we loop again for continuation */
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
			
		// Write string to file
		ignore fputs(charPtr(arguments[1]), fp[i]);
		
		if (number == 8)
		{
			// Flush without newline
			ignore fflush(fp[i]);
		}
		else
		{
			// Add newline
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
		// Get a single character (may return 0 if timeout)
		returnedObject = newInteger(getInputCharacter());
		break;

	case 13: /* prim 133: print the char of the integer passed in */
		// Print a single character
		putc(intValue(arguments[0]), stdout);
		fflush(stdout);
		break;

	default:
		sysError("unknown primitive", "filePrimitive");
	}

	return (returnedObject);
}
