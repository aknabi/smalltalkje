/*
    ESP-32 Little Smalltalk
    Written by Abdul Nabi, Feb. 2021

    Based on:
	Little Smalltalk, version 3
	Written by Tim Budd, January 1989

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
#define BLOCK_RUN_QUEUE_ITEM_SIZE   sizeof(run_block_queue_item)

// Note this does not use the interruptInterpreter mechanism.
extern void runBlock(object block, object arg);

#ifdef TARGET_ESP32

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"



static QueueHandle_t vmBlockToRunQueue;

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

static object vmBlockToRun;

object getNextVMBlockToRun()
{
    object nextToRun;
    if (isVMBlockQueued()) {
        BaseType_t result = xQueueReceive(vmBlockToRunQueue, &vmBlockToRun, portMAX_DELAY);
        return (result == pdPASS) ? vmBlockToRun : nilobj;
    }
    return nilobj;
}

static QueueHandle_t blockRunQueue;

void initBlockRunQueue()
{
blockRunQueue = xQueueCreate( BLOCK_RUN_QUEUE_DEPTH,
                            BLOCK_RUN_QUEUE_ITEM_SIZE );
}

boolean queueBlockItemToRun(run_block_queue_item blockItem, boolean isHighPrio)
{
    BaseType_t result = isHighPrio ?
        xQueueSendToBack( blockRunQueue, (void *) &blockItem, portMAX_DELAY ):
        xQueueSendToFront( blockRunQueue, (void *) &blockItem, portMAX_DELAY );
    return result == pdPASS;
}

boolean isBlockItemQueued()
{
    return uxQueueMessagesWaiting( blockRunQueue ) > 0;
}

extern boolean interruptInterpreter();
extern object vmBlockToRun;

static void taskRunBlockItem(run_block_queue_item *taskBlockArg)
{
    run_block_queue_item itemToRun;
    while(true) {
        BaseType_t result = xQueueReceive(blockRunQueue, &itemToRun, portMAX_DELAY);
        if (result == pdPASS) {
	        object block = taskBlockArg->block;
	        object arg = taskBlockArg->arg;
            // maybe have interruptInterpreter(object block)
            while (!interruptInterpreter()) {
		        vTaskDelay(2);
	        }
            // If we have interruptInterpreter(object block) don't need these
            while (vmBlockToRun != nilobj) {
                vTaskDelay(2);
            }
	        vmBlockToRun = block;
            // OR???
            // runBlock(itemToRun.block, itemToRun.arg);
        }
    }

	vTaskDelete(xTaskGetCurrentTaskHandle());
}

void startBlockQueue()
{
	xTaskCreate(
		taskRunBlockItem,	 /* Task function. */
		"taskRunBlockItem", /* name of task. */
		4096,				 /* Stack size of task */
		NULL,		 // parameter of the task (block, arg and delay until run)
		4,					 /* priority of the task */
		NULL);				 /* Task handle to keep track of created task */
}

#else

static run_block_queue_item runBlockQueue[BLOCK_RUN_QUEUE_DEPTH];
static int runBlockQueueIndex = 0;

void initVMBlockToRunQueue() {}
object getNextVMBlockToRun() { return nilobj; }
boolean queueVMBlockToRun(object block) { return true; }
boolean isVMBlockQueued() { return false; }

void initBlockRunQueue()
{
    // for (int i = 0; i < BLOCK_RUN_QUEUE_DEPTH; i++) runBlockQueue[i] = NULL;
    // static int runBlockQueueIndex = 0;
}

boolean queueBlockItemToRun(run_block_queue_item blockItem, boolean isHighPrio)
{
    // if (++runBlockQueueIndex == BLOCK_RUN_QUEUE_DEPTH) runBlockQueueIndex = 0;
    // if (runBlockQueue[runBlockQueueIndex) != NULL) {
    //     // Queue is full so don't add and revert index
    //     if (runBlockQueueIndex == 0) {
    //         runBlockQueueIndex = BLOCK_RUN_QUEUE_DEPTH - 1;
    //     } else {
    //         runBlockQueueIndex--;
    //     }
    //     return false;
    // } 
    // runBlockQueue[runBlockQueueIndex] = blockItem;
    return true;
}

boolean isBlockItemQueued()
{
    return false;
}

#endif
