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


static QueueHandle_t blockRunQueue;

void initBlockRunQueue()
{
blockRunQueue = xQueueCreate( BLOCK_RUN_QUEUE_DEPTH,
                            BLOCK_RUN_QUEUE_ITEM_SIZE );
}

boolean queueBlockItemToRun(run_block_queue_item blockItem, boolean isHighPrio)
{
    BaseType_t result = isHighPrio ?
        xQueueSendToBack( blockRunQueue, &blockItem, portMAX_DELAY ):
        xQueueSendToFront( blockRunQueue, &blockItem, portMAX_DELAY );
    return result == pdPASS;
}

boolean isBlockItemQueued()
{
    return uxQueueMessagesWaiting( blockRunQueue ) > 0;
}

void runNextBlockItem()
{
    run_block_queue_item itemToRun;
    BaseType_t result = xQueueReceive(blockRunQueue, &itemToRun, portMAX_DELAY);
    if (result == pdPASS)
        runBlock(itemToRun.block, itemToRun.arg);
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

void initBlockRunQueue()
{
    for (int i = 0; i < BLOCK_RUN_QUEUE_DEPTH; i**) runBlockQueue[i] = NULL;
    static int runBlockQueueIndex = 0;
}

boolean queueBlockItemToRun(run_block_queue_item blockItem, boolean isHighPrio)
{
    if (++runBlockQueueIndex == BLOCK_RUN_QUEUE_DEPTH) runBlockQueueIndex = 0;
    if (runBlockQueue[runBlockQueueIndex) != NULL) {
        // Queue is full so don't add and revert index
        if (runBlockQueueIndex == 0) {
            runBlockQueueIndex = BLOCK_RUN_QUEUE_DEPTH - 1;
        } else {
            runBlockQueueIndex--;
        }
        return false;
    } 
    runBlockQueue[runBlockQueueIndex] = blockItem;
}

boolean isBlockItemQueued()
{
    return false;
}

// THIS IS FLAWED LOGIC... HERE AS PLACEHOLDER UNTIL WE GET AROUND TO NON ESP32 QUEUE IMPLEMENTATION
void runNextBlockItem()
{
    run_block_queue_item itemToRun;
    int nextQueueSlot = runBlockQueueIndex + 1;
    if (nextQueueSlot == BLOCK_RUN_QUEUE_DEPTH) nextQueueSlot = 0;
    // WRONG TEST/LOGIC
    while(nextQueueSlot != runBlockQueueIndex {
        if (runBlockQueue[nextQueueSlot] != NULL) {
            itemToRun = runBlockQueue[nextQueueSlot];
            runBlockQueue[nextQueueSlot] = NULL
            runBlock(runBlockQueue[nextQueueSlot]->block, runBlockQueue[nextQueueSlot]->arg);
            break;
        }
    })
}

#endif
