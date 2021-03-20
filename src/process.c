/*
    ESP-32 Little Smalltalk interrupt driven execution support
    Written by Abdul Nabi, Feb. 2021

	process (blocks, etc) handling functions
	this has been added for supporting device events/interrupts
	and running smalltalk handler block by interrupting the
	interpreter loop.

    It uses message queues. FreeRTOS provides support for these.
    POSIX does as well, but since the Mac doesn't support it, for
    non FreeRTOS platforms we will need to roll a simple version
    that supports the methods in process.h
*/

#include "build.h"
#include "process.h"
#include "names.h"

#define BLOCK_RUN_QUEUE_DEPTH       16

// Note this does not use the interruptInterpreter mechanism.
extern void runMethodOrBlock(object method, object block, object arg);
extern void runBlock(object block, object arg);
extern boolean execute(object aProcess, int maxsteps);

void runSmalltalkProcess(object processToRun)
{
	if (processToRun != nilobj)
	{
        while (execute(processToRun, 15000));
	}
	else
	{
		fprintf(stderr, "<%s>: %s\n", "runSmalltalkProcess", "trying to run nil process");
	}
}

/*
 * Execute either a method or block (the other param will be nilobj)
 * Used by functions to wrap common code
 * The argument arg will be passed in as a block temp.
 * TODO: Arg should also be passed to method... also check to see block/method takes args
 */
void runMethodOrBlock(object method, object block, object arg)
{
	object process, stack;

	process = allocObject(processSize);
	// incr(process);
	stack = newArray(50);
	// incr(stack);

	/* make a process */
	basicAtPut(process, stackInProcess, stack);
	basicAtPut(process, stackTopInProcess, newInteger(10));
	basicAtPut(process, linkPtrInProcess, newInteger(2));

	basicAtPut(stack, 1, method == nilobj ? nilobj : arg); /* argument if method */

	/* now make a linkage area in stack */
	basicAtPut(stack, 2, nilobj); /* previous link */

	object ctxObj = method == nilobj ? basicAt(block, contextInBlock) : nilobj;
	basicAtPut(stack, 3, ctxObj); /* context object (nil = stack) */

	basicAtPut(stack, 4, newInteger(1)); /* return point */

	basicAtPut(stack, 5, method); /* method if there is one (otherwise nil) */

	object bytecountPos = method == nilobj ? basicAt(block, bytecountPositionInBlock) : newInteger(1);
	basicAtPut(stack, 6, bytecountPos); /* byte offset */

	/* now go execute it */
	while (execute(process, 15000));
	// unaryPrims(9, process);
}

void addArgToBlock(object block, object arg)
{
	if (block != nilobj)
	{
		// object argArray;
		// if (arg != nilobj) {
			// argArray = newArray(1);
			// basicAtPut(argArray, 1, arg);
			basicAtPut(basicAt(block, contextInBlock), temporariesInContext, arg); // block
		// }
	}
}

object queueBlockArray = nilobj;

void queueBlock(object block, object arg)
{
	if (block != nilobj)
	{
		object queueObject = block;
		if (arg != nilobj) {
			// aBlock - context at: argLoc put: x. ( context - temporaries at: argLoc put: value)
			// This replicates what we do in Smalltalk, but doesn't seem to work :/
			// object context = basicAt(queueObject, contextInBlock);
			// object contextTemps = basicAt(context, temporariesInContext);
			// object argLoc = basicAt(queueObject, argumentLocationInBlock);
			// basicAtPut(contextTemps, argLoc, arg);

			// Right now creating an Array and letting Smalltalk call value: with the arg... works...

			// TODO: Create the array once and reuse as these accumulate in the obj table... revisit
			if (queueBlockArray == nilobj) queueBlockArray = newArray(2);
			basicAtPut(queueBlockArray, 1, block);
			basicAtPut(queueBlockArray, 2, arg);
			queueObject = queueBlockArray;
		}
		// addArgToBlock(block, arg);
		queueVMBlockToRun(queueObject);
	}
}

void runBlock(object block, object arg)
{
	/* put argument in block temps */
	if (block != nilobj)
	{
		addArgToBlock(block, arg);
		runMethodOrBlock(nilobj, block, arg);
	}
}

#ifdef TARGET_ESP32

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

typedef struct
{
	object block; // block to run
	object arg;	  // and block argument
	int ticks;	  // ticks to delay before running
} task_block_arg;

static QueueHandle_t vmBlockToRunQueue;

static void taskRunBlockAfter(task_block_arg *taskBlockArg)
{
	object block = taskBlockArg->block;
	object arg = taskBlockArg->arg;
	int ticks = taskBlockArg->ticks;
	vTaskDelay(ticks);
	while (!interruptInterpreter())
	{
		vTaskDelay(20 / portTICK_PERIOD_MS);
	}
	queueVMBlockToRun(block);
	vTaskDelete(xTaskGetCurrentTaskHandle());
}

// prim 152 calls this
void runBlockAfter(object block, object arg, int ticks)
{
	// Since VM has a reference to the block
	task_block_arg taskBlockArg;

	incr(block);

	taskBlockArg.block = block;
	taskBlockArg.arg = arg;
	taskBlockArg.ticks = ticks;

	xTaskCreate(
		taskRunBlockAfter,	 /* Task function. */
		"taskRunBlockAfter", /* name of task. */
		8096,				 /* Stack size of task */
		&taskBlockArg,		 // parameter of the task (block, arg and delay until run)
		1,					 /* priority of the task */
		NULL);				 /* Task handle to keep track of created task */
}

void initVMBlockToRunQueue()
{
    vmBlockToRunQueue = xQueueCreate( BLOCK_RUN_QUEUE_DEPTH, sizeof(object) );
}

boolean queueVMBlockToRun(object block)
{
	// TODO: Don't think we need to inc the ref count on the block.
    incr(block);
    BaseType_t result = xQueueSend( vmBlockToRunQueue, &block, portMAX_DELAY);
    return result == pdPASS;
}


boolean isVMBlockQueued()
{
    return uxQueueMessagesWaiting( vmBlockToRunQueue ) > 0;
}

object getNextVMBlockToRun()
{
    object nextToRun;
    if (isVMBlockQueued()) {
        BaseType_t result = xQueueReceive(vmBlockToRunQueue, &nextToRun, portMAX_DELAY);
        return (result == pdPASS) ? nextToRun : nilobj;
    }
    return nilobj;
}

void evalTask(void *evalText, object arg)
{
	doIt(evalText, arg);
	vTaskDelete(NULL);
}

extern boolean interruptInterpreter();

void forkEval(char *evalText, object arg)
{
	xTaskCreate(
		evalTask,	/* Task function. */
		"evalTask", /* name of task. */
		8096,		/* Stack size of task */
		evalText,	/* parameter of the task (the Smalltalk exec string to run) */
		1,			/* priority of the task */
		NULL);		/* Task handle to keep track of created task */
}

#else

static int runBlockQueueIndex = 0;

void initVMBlockToRunQueue() {}
object getNextVMBlockToRun() { return nilobj; }
boolean queueVMBlockToRun(object block) { return true; }
boolean isVMBlockQueued() { return false; }

extern void doIt(char *text, object arg);

// When not running on a ESP32 do single thread versions
void forkEval(char *evalText, object arg)
{
	doIt(evalText, arg);
}

#endif
