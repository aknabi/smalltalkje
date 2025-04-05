/*
	Smalltalkje, version 1 based on:
	Little Smalltalk, version 3
	Written by Tim Budd, January 1989

	Terminal Interface Routines
	
	This module provides terminal interface routines for the Smalltalkje system.
	It handles error reporting, warnings, and user interaction through a text-based
	terminal interface. These routines are used by systems with a bare TTY interface.
	
	The implementation provides:
	- Error and warning message display functions
	- Compiler error and warning reporting
	- Method display for debugging
	- Simple user interaction for pausing execution
	
	Systems using another interface (e.g., a graphical interface) would replace
	this file with their own implementation.
*/

#include <stdio.h>
#include <stdlib.h>

#include "memory.h"

/** Flag indicating if parsing was successful */
extern boolean parseok;

/**
 * Report a fatal system error and terminate execution
 * 
 * This function displays an error message to the user and terminates
 * the execution of the system. It's used for reporting unrecoverable
 * errors that prevent further execution.
 * 
 * @param s1 First part of the error message
 * @param s2 Second part of the error message
 */
noreturn sysError(char *s1, char *s2)
{
    ignore fprintf(stderr, "Error <%s>: %s\n", s1, s2);
    ignore abort();
}

/**
 * Report a non-fatal system warning
 * 
 * This function displays a warning message to the user but allows
 * execution to continue. It's used for reporting non-critical issues
 * that don't prevent the system from functioning.
 * 
 * @param s1 First part of the warning message
 * @param s2 Second part of the warning message
 */
noreturn sysWarn(char *s1, char *s2)
{
    ignore fprintf(stderr, "Warning <%s>: %s\n", s1, s2);
}

/**
 * Report a compiler warning
 * 
 * This function displays a warning message related to the compilation
 * of Smalltalk code. It includes information about the method selector
 * where the warning occurred.
 * 
 * @param selector The method selector where the warning occurred
 * @param str1 First part of the warning message
 * @param str2 Second part of the warning message
 */
void compilWarn(char *selector, char *str1, char *str2)
{
    ignore fprintf(stderr, "compiler warning: Method %s : %s %s\n",
                   selector, str1, str2);
}

/**
 * Report a compiler error
 * 
 * This function displays an error message related to the compilation
 * of Smalltalk code. It includes information about the method selector
 * where the error occurred and sets the parseok flag to false to indicate
 * that parsing failed.
 * 
 * @param selector The method selector where the error occurred
 * @param str1 First part of the error message
 * @param str2 Second part of the error message
 */
void compilError(char *selector, char *str1, char *str2)
{
    ignore fprintf(stderr, "compiler error: Method %s : %s %s\n",
                   selector, str1, str2);
    parseok = false;
}

/**
 * Display method information
 * 
 * This function would typically display information about a method for
 * debugging or tracing purposes. In this implementation, it's disabled
 * (commented out) but could be enabled to show the class and method names.
 * 
 * @param cp The class name
 * @param mp The method name
 */
noreturn dspMethod(char *cp, char *mp)
{
    /*ignore fprintf(stderr,"%s %s\n", cp, mp); */
}

/**
 * Pause execution and wait for user input
 * 
 * This function pauses the execution of the system and waits for
 * the user to press Enter before continuing. It's used for debugging
 * or to give the user time to read messages before proceeding.
 */
void givepause()
{
    char buffer[80];

    ignore fprintf(stderr, "push return to continue\n");
    ignore fgets(buffer, 80, stdin);
}
