/*
    Smalltalkje, version 1 based on:
    Little Smalltalk, version 3
    Written by Tim Budd, January 1989

    Terminal Interface Header
    
    This header defines the interface for terminal I/O and error reporting
    in the Smalltalkje system. It provides functions for displaying error
    and warning messages, reporting compilation issues, and interacting
    with the user through a text-based terminal.
    
    The functions declared here form the primary interface between the
    Smalltalk system and the user for error reporting and debugging
    output. On embedded platforms like the ESP32, these functions may
    output to a serial console, while on desktop platforms they typically
    output to stdout/stderr.
    
    These functions can be replaced with platform-specific implementations
    when using alternative interfaces (e.g., graphical interfaces or
    custom display systems).
*/

/**
 * Report a system error and terminate execution
 * 
 * This function displays a critical error message to the user and
 * terminates the execution of the system. It's used for reporting
 * unrecoverable errors that prevent further execution.
 * 
 * @param s1 First part of the error message
 * @param s2 Second part of the error message (may be empty)
 */
noreturn sysError(char *s1, char *s2);

/**
 * Report a system warning without terminating
 * 
 * This function displays a warning message to the user but allows
 * execution to continue. It's used for reporting non-critical issues
 * that don't prevent the system from functioning.
 * 
 * @param s1 First part of the warning message
 * @param s2 Second part of the warning message (may be empty)
 */
noreturn sysWarn(char *s1, char *s2);

/**
 * Report a compilation warning
 * 
 * This function displays a warning message related to the compilation
 * of Smalltalk code. It includes information about the selector (method name)
 * where the warning occurred.
 * 
 * @param selector The method selector where the warning occurred
 * @param str1 First part of the warning message
 * @param str2 Second part of the warning message (may be empty)
 */
void compilWarn(char *selector, char *str1, char *str2);

/**
 * Report a compilation error
 * 
 * This function displays an error message related to the compilation
 * of Smalltalk code. It includes information about the selector (method name)
 * where the error occurred.
 * 
 * @param selector The method selector where the error occurred
 * @param str1 First part of the error message
 * @param str2 Second part of the error message (may be empty)
 */
void compilError(char *selector, char *str1, char *str2);

/**
 * Display method information
 * 
 * This function displays information about a method, typically for
 * debugging or tracing purposes. It shows the class and method name.
 * 
 * @param cp The class name (as a string)
 * @param mp The method name (as a string)
 */
noreturn dspMethod(char *cp, char *mp);

/**
 * Pause execution and wait for user input
 * 
 * This function pauses the execution of the system and waits for
 * the user to press a key before continuing. It's used for debugging
 * or to give the user time to read messages before proceeding.
 */
void givepause();
