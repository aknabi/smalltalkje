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

#define BLOCK_RUN_QUEUE_DEPTH       16

// Note this does not use the interruptInterpreter mechanism.
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

#else

static int runBlockQueueIndex = 0;

void initVMBlockToRunQueue() {}
object getNextVMBlockToRun() { return nilobj; }
boolean queueVMBlockToRun(object block) { return true; }
boolean isVMBlockQueued() { return false; }

#endif
