/*
	Smalltalkje, version 1 based on:
	Little Smalltalk, version 3
	Main Driver
	
	Original written by Tim Budd, September 1988
	Oregon State University
	
	Updated for embedded support by Abdul Nabi
	
	Main Program Entry Point
	
	This module contains the main entry point for the Smalltalkje system.
	It handles system initialization, image loading, and the main execution
	loop that drives the Smalltalk environment.
	
	The implementation supports:
	- Initializing the memory manager
	- Loading Smalltalk object image from file
	- Setting up the initial execution environment
	- Running the main Smalltalk process
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "env.h"
#include "memory.h"
#include "names.h"

/** Flag indicating if we're building an initial image */
int initial = 0; /* not making initial image */

/** Forward declarations */
extern int objectCount(void);
boolean execute(object aProcess, int maxsteps);

/**
 * Read the Smalltalk system image from file
 * 
 * This function opens and reads the standard system image file, which
 * contains the serialized Smalltalk object memory. It uses the imageRead
 * function to deserialize the objects into memory.
 */
void readImage()
{
    FILE *fp;
    char *p, buffer[32];

    strcpy(buffer, "systemImage");
    p = buffer;

    fp = fopen(p, "r");
    if (fp == NULL)
    {
        sysError("cannot open image", p);
        exit(1);
    }

    imageRead(fp);
}

/**
 * Read Smalltalk objects from separate table and data files
 * 
 * This function loads the object system from two separate files:
 * - objectTable: Contains object metadata and references
 * - objectData: Contains the actual object data
 * 
 * This is used instead of readImage() when the system is using the
 * split object file format rather than a single image file.
 */
void readImageObjects()
{
    FILE *fpObjTable;
    FILE *fpObjData;
    char *pOT, buffer1[32];
    char *pOD, buffer2[32];

    // Open the object table file
    strcpy(buffer1, "objectTable");
    pOT = buffer1;

    fpObjTable = fopen(pOT, "r");
    if (fpObjTable == NULL)
    {
        sysError("cannot open object table", pOT);
        exit(1);
    }

    // Open the object data file
    strcpy(buffer2, "objectData");
    pOD = buffer2;

    fpObjData = fopen(pOD, "r");
    if (fpObjData == NULL)
    {
        sysError("cannot open object data", pOD);
        exit(1);
    }

    // Read both files to construct the object memory
    readObjectFiles(fpObjTable, fpObjData);
}

/**
 * Main entry point for the Smalltalkje system
 * 
 * This function initializes the Smalltalkje system, loads the object image,
 * and starts the main execution loop. It performs the following steps:
 * 
 * 1. Initialize the memory manager
 * 2. Load the Smalltalk object image
 * 3. Initialize common symbols
 * 4. Find and start the system process
 * 5. Run the main execution loop until completion
 * 
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @return Exit status (always 0 on normal exit)
 */
int main(int argc, char **argv)
{
    object firstProcess;

    // Initialize the memory management system
    initMemoryManager();

    // Load the object system
    // readImage();  // Single image file approach
    readImageObjects();  // Split object files approach

    // Initialize common symbols needed by the system
    initCommonSymbols();

    // Find the main system process to execute
    firstProcess = globalSymbol("systemProcess");
    if (firstProcess == nilobj)
    {
        sysError("no initial process", "in image");
        exit(1);
        return 1;
    }

    // Print system banner
    printf("Little Smalltalk, Version 3.1\n");
    printf("Written by Tim Budd, Oregon State University\n");
    printf("Updated for modern systems by Charles Childers\n");
    printf("Updated for embedded support by Abdul Nabi\n");

    // Run the main process until it terminates
    while (execute(firstProcess, 15000))
        ;  // Execute with a step limit of 15000 instructions

    // Normal exit
    exit(0);
    return 0;
}
