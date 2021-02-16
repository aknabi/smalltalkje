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

#ifdef TARGET_ESP32

#include "freertos/FreeRTOS.h"
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
