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
extern void fileIn(FILE *fd, boolean printit);
extern noreturn writeObjectTable(FILE *fp);
extern noreturn writeObjectData(FILE *fp);

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
