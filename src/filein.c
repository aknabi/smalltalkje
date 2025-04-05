/*
	Smalltalkje, version 1 based on:

	Little Smalltalk, version 3
	Written by Tim Budd, Oregon State University, June 1988

	File Input System
	
	This module provides routines for reading in textual descriptions of classes
	from files. It handles parsing class declarations, method definitions, and 
	immediate code evaluation from Smalltalk source files.
	
	The implementation supports:
	- Reading and parsing class declarations 
	- Finding or creating classes
	- Reading method definitions
	- Direct evaluation of Smalltalk code snippets
	- File inclusion from external sources
	
	This system forms the foundation of the Smalltalk image building process
	by parsing source code that defines the class hierarchy and methods.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "env.h"
#include "memory.h"
#include "names.h"
#include "lex.h"

/* Forward declarations */
void setInstanceVariables(object aClass);
boolean parse(object method, char *text, boolean savetext);
void sysDecr(object z);
void givepause(void);

/** Default size for method tables */
#define MethodTableSize 39

/** Flag to control whether method source text should be saved */
boolean savetext = false;

/** 
 * Buffer for text input
 * 
 * All input is read a line at a time into this buffer.
 * The buffer must be large enough to hold entire method definitions.
 */
#define TextBufferSize 1024
static char textBuffer[TextBufferSize];

/**
 * Find or create a class with the given name
 * 
 * This function looks for a class with the specified name in the global symbols.
 * If not found, it creates a new class. It also ensures the class has a size
 * field, initializing it to zero if needed.
 * 
 * @param name The name of the class to find or create
 * @return The class object
 */
object findClass(name) char *name;
{
	object newobj;

	// Look up the class in global symbols
	newobj = globalSymbol(name);
	
	// If not found, create a new class
	if (newobj == nilobj)
		newobj = newClass(name);
	
	// Ensure the class has a size field
	if (basicAt(newobj, sizeInClass) == nilobj)
	{
		basicAtPut(newobj, sizeInClass, newInteger(0));
	}
	
	return newobj;
}

/**
 * Directly execute a string of Smalltalk code
 * 
 * This function compiles and executes the given text as Smalltalk code.
 * It creates a temporary method, process, and stack to run the code,
 * then executes it until completion.
 * 
 * @param text The Smalltalk code to execute
 */
void justDoIt(text)
char *text;
{
    object process, stack, method;

    // Create a new method to hold the compiled code
    method = newMethod();
    incr(method);
    setInstanceVariables(nilobj);
    ignore parse(method, text, false);

    // Create a process and stack to execute the method
    process = allocObject(processSize);
    incr(process);
    stack = newArray(50);
    incr(stack);

    // Set up the process structure
    basicAtPut(process, stackInProcess, stack);
    basicAtPut(process, stackTopInProcess, newInteger(10));
    basicAtPut(process, linkPtrInProcess, newInteger(2));

    // Set up the stack with the method and execution context
    basicAtPut(stack, 1, nilobj);              // argument
    basicAtPut(stack, 2, nilobj);              // previous link
    basicAtPut(stack, 3, nilobj);              // context object (nil = stack)
    basicAtPut(stack, 4, newInteger(1));       // return point
    basicAtPut(stack, 5, method);              // method
    basicAtPut(stack, 6, newInteger(1));       // byte offset

    // Execute the code until completion
    while (execute(process, 15000))
        fprintf(stderr, "..");
}

/**
 * Read and execute a line of Smalltalk code from a file
 * 
 * This function reads the remainder of the current line and stores it
 * in a global variable for later execution. Instead of directly executing
 * the code, it stores it in a global variable that Smalltalk code can access.
 * 
 * Note: Direct execution with justDoIt() is currently disabled due to
 * crashing issues during image building.
 */
const char *fileInEvalKeyStr = "fileInEvalStr";

static void readAndExecute()
{
	char *execLine = toEndOfLine();
	
	// TODO: evaluating the text (as we do in image building) crashes, so for now
	// store in a global and let Smalltalk look for the global to run.
	// The only issue is that this only allows for a single evaluation line in a filein

	// Broken :(
	// justDoIt(execLine);

	// Store the code in a global variable
    object nameObj = newSymbol(fileInEvalKeyStr);
    nameTableInsert(symbols, strHash(fileInEvalKeyStr), nameObj, newStString(execLine));
}

/**
 * Read a class declaration from the input
 * 
 * This function parses a class declaration, which includes:
 * - The class name
 * - Optionally, a superclass name
 * - Optionally, a list of instance variable names
 * 
 * It creates or updates the class structure accordingly, setting the
 * superclass reference, instance variables, and size.
 */
static void readClassDeclaration()
{
	object classObj, super, vars;
	int i, size, instanceTop;
	object instanceVariables[15];

	// Read the class name
	if (nextToken() != nameconst)
		sysError("bad file format", "no name in declaration");
	
	// Find or create the class
	classObj = findClass(tokenString);
	size = 0;
	
	// Check for a superclass specification
	if (nextToken() == nameconst)
	{ 
		// Read superclass name
		super = findClass(tokenString);
		basicAtPut(classObj, superClassInClass, super);
		size = intValue(basicAt(super, sizeInClass));
		ignore nextToken();
	}
	
	// Check for instance variable declarations
	if (token == nameconst)
	{ 
		// Read instance variable names
		instanceTop = 0;
		while (token == nameconst)
		{
			instanceVariables[instanceTop++] = newSymbol(tokenString);
			size++;
			ignore nextToken();
		}
		
		// Create and populate the variables array
		vars = newArray(instanceTop);
		for (i = 0; i < instanceTop; i++)
		{
			basicAtPut(vars, i + 1, instanceVariables[i]);
		}
		basicAtPut(classObj, variablesInClass, vars);
	}
	
	// Set the class size
	basicAtPut(classObj, sizeInClass, newInteger(size));
}

/**
 * Read method definitions for a class
 * 
 * This function reads one or more method definitions for a class from the input.
 * Each method is parsed, compiled, and added to the class's method dictionary.
 * 
 * @param fd File to read from
 * @param printit Whether to print method names as they are compiled
 */
static void readMethods(fd, printit)
	FILE *fd;
boolean printit;
{
	object classObj, methTable, theMethod, selector;
#define LINEBUFFERSIZE 512
    char *cp = NULL, *eoftest, lineBuffer[LINEBUFFERSIZE];

	// Read the class name
	if (nextToken() != nameconst)
		sysError("missing name", "following Method keyword");
	
	// Find the class and set up for method compilation
	classObj = findClass(tokenString);
	setInstanceVariables(classObj);
	if (printit)
		cp = charPtr(basicAt(classObj, nameInClass));

	// Find or create the class's method table
	methTable = basicAt(classObj, methodsInClass);
	if (methTable == nilobj)
	{ 
		methTable = newDictionary(MethodTableSize);
		basicAtPut(classObj, methodsInClass, methTable);
	}

	// Read methods until we reach the end marker
	do
	{
		// Handle continuation of text from previous line
		if (lineBuffer[0] == '|') 
			strcpy(textBuffer, &lineBuffer[1]);
		else
			textBuffer[0] = '\0';
		
		// Read lines until we hit a method boundary or end marker
		while ((eoftest = fgets(lineBuffer, LINEBUFFERSIZE, fd)) != NULL)
		{
			if ((lineBuffer[0] == '|') || (lineBuffer[0] == ']'))
				break;
			ignore strcat(textBuffer, lineBuffer);
		}
		
		// Check for unexpected end of file
		if (eoftest == NULL)
		{
			sysError("unexpected end of file", "while reading method");
			break;
		}

		// Parse and add the method
		theMethod = newMethod();
		if (parse(theMethod, textBuffer, savetext))
		{
			selector = basicAt(theMethod, messageInMethod);
			basicAtPut(theMethod, methodClassInMethod, classObj);
			
			if (printit)
				dspMethod(cp, charPtr(selector));
				
			// Add the method to the class's method table
			nameTableInsert(methTable, (int)selector, selector, theMethod);
		}
		else
		{
			// Clean up if parsing failed
			incr(theMethod);
			decr(theMethod);
			givepause();
		}

	} while (lineBuffer[0] != ']');
}

/**
 * Process a Smalltalk source file
 * 
 * This function reads a Smalltalk source file and processes its contents.
 * It handles various types of input lines:
 * - Empty lines (ignored)
 * - Comments (lines starting with '*')
 * - Code execution directives (lines starting with '!')
 * - Class declarations (lines starting with "Class")
 * - Method definitions (lines starting with "Methods")
 * 
 * @param fd File to read from
 * @param printit Whether to print progress information
 */
void fileIn(FILE *fd, boolean printit)
{
	while (fgets(textBuffer, TextBufferSize, fd) != NULL)
	{
		// Initialize lexical analyzer with the current line
		lexinit(textBuffer);
		
		// Process based on the first token
		if (token == inputend)
		{
			// Empty line - skip
		}
		else if ((token == binary) && streq(tokenString, "*"))
		{
			// Comment line - skip
		}
		else if ((token == binary) && streq(tokenString, "!"))
		{
			// Code execution directive
			readAndExecute();
		}
		else if ((token == nameconst) && streq(tokenString, "Class"))
		{
			// Class declaration
			readClassDeclaration();
		}
		else if ((token == nameconst) && streq(tokenString, "Methods"))
		{
			// Method definitions
			readMethods(fd, printit);
		}
		else
		{
			// Unrecognized line
			sysError("unrecognized line", textBuffer);
		}
	}
}
