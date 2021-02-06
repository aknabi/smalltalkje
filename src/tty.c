/*
	Little Smalltalk, version 3
	Written by Tim Budd, January 1989

	tty interface routines
	this is used by those systems that have a bare tty interface
	systems using another interface, such as the stdwin interface
	will replace this file with another.
*/

#include <stdio.h>
#include <stdlib.h>

#include "env.h"
#include "memory.h"
#include "target.h"
#include "names.h"

#include "build.h"

#ifdef TARGET_ESP32

#include "driver/gpio.h"

#if TARGET_DEVICE == DEVICE_ESP32_SSD1306
#include "ssd1306_oled.h"
#elif TARGET_DEVICE == DEVICE_M5STICKC
#include "tft.h"
#endif


#endif

#define getIntArg(i) intValue(arguments[i])

#define checkIntArg(i) if (!isInteger(arguments[i])) \
 { sysError("non integer index", "x"); }
 
#define checkArgClass(i, classStr) if ( classField(arguments[i] ) != findClass(classStr) ) \
 { sysError("Argument is not the expected class", classStr); }
 
extern boolean parseok;

/* report a fatal system error */
noreturn sysError(char *s1, char *s2)
{
    ignore fprintf(stderr, "%s\n%s\n", s1, s2);
    ignore abort();
}

/* report a nonfatal system error */
noreturn sysWarn(char *s1, char *s2)
{
    ignore fprintf(stderr, "%s\n%s\n", s1, s2);
}

void compilWarn(char *selector, char *str1, char *str2)
{
    ignore fprintf(stderr, "compiler warning: Method %s : %s %s\n",
		   selector, str1, str2);
}

void compilError(char *selector, char *str1, char *str2)
{
    ignore fprintf(stderr, "compiler error: Method %s : %s %s\n",
		   selector, str1, str2);
    parseok = false;
}

noreturn dspMethod(char *cp, char *mp)
{
    /*ignore fprintf(stderr,"%s %s\n", cp, mp); */
}

void givepause()
{
    char buffer[80];

    ignore fprintf(stderr, "push return to continue\n");
    ignore fgets(buffer, 80, stdin);
}

#ifdef TARGET_ESP32

extern object processStack;
extern int linkPointer;

int counter = 0;

void runTask(void *process)
{
    if (counter == 0) {
        while (counter++ < 10) {
            printf( "in runTask with loop number: %d", counter );
            vTaskDelay( 500 );
        }
        counter = 0;
    }

    // int i, j, saveLinkPointer;
    // object returnedObject, saveProcessStack;

    // /* first save the values we are about to clobber */
	// saveProcessStack = processStack;
	// saveLinkPointer = linkPointer;

    // printf( "in runTask with process object: %d", (object) process );
    // execute((object) process, 15000);
    
    // processStack = saveProcessStack;
	// linkPointer = saveLinkPointer;

    /* delete a task when finish */
    ignore fprintf(stderr, "Done with runTask... deleting task\n");
    vTaskDelete( NULL );
}

#endif

object sysPrimitive(int number, object * arguments)
{
    object returnedObject = nilobj;

    /* someday there will be more here */
    switch (number - 150) {
    case 0:			/* do a system() call */
	    returnedObject = newInteger(system(charPtr(arguments[0])));
	    break;

#ifdef TARGET_ESP32
    case 1:         /* prim 151 create a OS task with a ST process */
        ; // Semicolon solves for "error: a label can only be part of a statement and a declaration is not a statement"
        // TaskHandle_t *taskHandle = NULL;;
        // BaseType_t xReturned = xTaskCreate(
        object processToRun = arguments[0];
        if ( classField(processToRun) != findClass("Process") ) {
            sysError("forkTask argument must be a process", "taskDelay");
        }
        xTaskCreate(
            runTask, /* Task function. */
            "runTask", /* name of task. */
            8096, /* Stack size of task */
            arguments[0], /* parameter of the task (the Smalltalk process to run) */
            1, /* priority of the task */
            NULL); /* Task handle to keep track of created task */

        // We'd like to return the handle in order to manage the process.
        break;

    case 2:         /* prim 152 delays the current OS task with a ST process for a given number of milliseconds */
        checkIntArg(0)
        vTaskDelay( intValue(arguments[0]) );
        // We'd like to return the handle in order to manage the process.
        break;

    /* prim 153 initializes the OLED display. Must be called before displaying */
    case 3:
#if TARGET_DEVICE == DEVICE_ESP32_SSD1306
        SSD1306_Begin();
#elif TARGET_DEVICE == DEVICE_M5STICKC
#ifndef TEST_M5STICK
        m5StickInit();
#endif
#endif
        break;

    // prim 154 Clear the display
    case 4:
#if TARGET_DEVICE == DEVICE_ESP32_SSD1306
        SSD1306_ClearDisplay();
#elif TARGET_DEVICE == DEVICE_M5STICKC
        TFT_fillScreen(current_paint.backgroundColor);
#endif
        break;

    // Prim 155 Render the buffer to the display
    case 5:
#if TARGET_DEVICE == DEVICE_ESP32_SSD1306
        SSD1306_Display();
#elif TARGET_DEVICE == DEVICE_M5STICKC
        // M5StickC doesn't have offscren render
#endif

        break;

    // Prim 156 Display the string at the x,y location passed in
    case 6:
        checkIntArg(1)
        checkIntArg(2)

#if TARGET_DEVICE == DEVICE_ESP32_SSD1306
        SSD1306_DrawText(
            getIntArg(1), 
            getIntArg(2), 
            charPtr(arguments[0]),
            1);
#elif TARGET_DEVICE == DEVICE_M5STICKC
        TFT_resetclipwin();
        TFT_setFont(DEFAULT_FONT, NULL);
        TFT_print(charPtr(arguments[0]), getIntArg(1), getIntArg(2));
#endif


        /* Set GPIO PIN in first argument to value in second argument */
        // gpio_set_level(intValue(arguments[0]), intValue(arguments[1]));

        // We'd like to return the handle in order to manage the process.
        break;

    // Prim 157 rectangleX: x y: y width: w height: h isFilled: aBoolean
    case 7:
        checkIntArg(0)
        checkIntArg(1)
        checkIntArg(2)
        checkIntArg(3)
        if (arguments[4] != trueobj && arguments[4] != falseobj) {
	        sysError("non boolean argument", "isFilled");
        }

#if TARGET_DEVICE == DEVICE_ESP32_SSD1306
        if (arguments[4] == trueobj) {
            SSD1306_FillRect(
                getIntArg(0), 
                getIntArg(1), 
                getIntArg(2), 
                getIntArg(3), 
                oled_color_white);
        } else {
            SSD1306_DrawRect(
                getIntArg(0), 
                getIntArg(1), 
                getIntArg(2), 
                getIntArg(3));
        }
#elif TARGET_DEVICE == DEVICE_M5STICKC
        if (arguments[4] == trueobj) {
            TFT_fillRect(
                getIntArg(0),
                getIntArg(1),
                getIntArg(2),
                getIntArg(3),
                TFT_WHITE);
        } else {
            TFT_drawRect(
                getIntArg(0),
                getIntArg(1),
                getIntArg(2),
                getIntArg(3),
                TFT_WHITE);
        }
        break;

    // Prim 158 circleX: x y: y radius: r isFilled: aBoolean
    case 8:
        checkIntArg(0)
        checkIntArg(1)
        checkIntArg(2)

        if (arguments[3] != trueobj && arguments[3] != falseobj) {
	        sysError("non boolean argument", "isFilled");
        }

#if TARGET_DEVICE == DEVICE_ESP32_SSD1306
        if (arguments[4] == trueobj) {
            SSD1306_FillCircle(
                getIntArg(0), 
                getIntArg(1), 
                getIntArg(2), 
                oled_color_white);
        } else {
            SSD1306_DrawCircle(
                getIntArg(0), 
                getIntArg(1), 
                getIntArg(2));
        }
#elif TARGET_DEVICE == DEVICE_M5STICKC
        if (arguments[4] == trueobj) {
            TFT_fillCircle(
                getIntArg(0),
                getIntArg(1),
                getIntArg(2),
                TFT_WHITE);
        } else {
            TFT_drawCircle(
                getIntArg(0),
                getIntArg(1),
                getIntArg(2),
                TFT_WHITE);
        }
#endif

        break;

#endif

    // Prim 159 set GPIO pin in first arg to mode in second arg
    case 9:
        checkIntArg(0)
        checkIntArg(1)

        gpio_mode_t gpioMode;
        gpio_pad_select_gpio(getIntArg(0));

        switch(getIntArg(1)) {
            case 0: gpioMode = GPIO_MODE_DISABLE; break; // disable input and output
            case 1: gpioMode = GPIO_MODE_INPUT; break; // input only
            case 2: gpioMode = GPIO_MODE_OUTPUT; break; // output only mode
            case 3: gpioMode = GPIO_MODE_OUTPUT_OD; break; // output only with open-drain mode
            case 4: gpioMode = GPIO_MODE_INPUT_OUTPUT_OD; break; // output and input with open-drain mode
            case 5: gpioMode = GPIO_MODE_INPUT_OUTPUT; break; // output and input mode
            default: gpioMode = GPIO_MODE_OUTPUT; break;
        }
        gpio_set_direction(getIntArg(0), gpioMode);
        break;

    // Prim 160 set GPIO pin in first arg to value in second arg
    case 10:
        checkIntArg(0)
        checkIntArg(1)
        gpio_set_level(getIntArg(0), getIntArg(1));
        break;
#endif

    default:
	    sysError("unknown primitive", "sysPrimitive");
    }
    return (returnedObject);
}
