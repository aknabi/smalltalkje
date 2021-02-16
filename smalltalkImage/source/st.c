/*
	Little Smalltalk, version 3
	Main Driver
	written By Tim Budd, September 1988
	Oregon State University
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "env.h"
#include "memory.h"
#include "names.h"

int initial = 0; /* not making initial image */

extern int objectCount();
boolean execute(object aProcess, int maxsteps);

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

void readImageObjects()
{
    FILE *fpObjTable;
    FILE *fpObjData;
    char *pOT, buffer1[32];
    char *pOD, buffer2[32];

    strcpy(buffer1, "objectTable");
    pOT = buffer1;

    fpObjTable = fopen(pOT, "r");
    if (fpObjTable == NULL)
    {
        sysError("cannot open object table", pOT);
        exit(1);
    }

    strcpy(buffer2, "objectData");
    pOD = buffer2;

    fpObjData = fopen(pOD, "r");
    if (fpObjData == NULL)
    {
        sysError("cannot open object data", pOD);
        exit(1);
    }

    readObjectFiles(fpObjTable, fpObjData);
}

int main(int argc, char **argv)
{
    // FILE *fp;
    object firstProcess;
    // char *p, buffer[120];

    initMemoryManager();

    // readImage();
    readImageObjects();

    initCommonSymbols();

    firstProcess = globalSymbol("systemProcess");
    if (firstProcess == nilobj)
    {
        sysError("no initial process", "in image");
        exit(1);
        return 1;
    }

    printf("Little Smalltalk, Version 3.1\n");
    printf("Written by Tim Budd, Oregon State University\n");
    printf("Updated for modern systems by Charles Childers\n");
    printf("Updated for embedded support by Abdul Nabi\n");

    while (execute(firstProcess, 15000))
        ;

    /* exit and return - belt and suspenders, but it keeps lint happy */
    exit(0);
    return 0;
}
