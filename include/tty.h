/*
    Little Smalltalk, version 3
    Written by Tim Budd, January 1989

    tty.h (header file added for smallatlkje)
    
    tty interface routines
    this is used by those systems that have a bare tty interface
    systems using another interface, such as the stdwin interface
    will replace this file with another.
*/

noreturn sysError(char *s1, char *s2);
noreturn sysWarn(char *s1, char *s2);
void compilWarn(char *selector, char *str1, char *str2);
void compilError(char *selector, char *str1, char *str2);
noreturn dspMethod(char *cp, char *mp);
void givepause();


