/*
    Smalltalkje, version 1
    Written by Abdul Nabi, code krafters, February 2021

    Process Management and Execution Module
    
    This module provides concurrency support for Smalltalkje, enabling the system to:
    1. Execute Smalltalk blocks in response to hardware events and interrupts
    2. Queue blocks for deferred execution
    3. Manage the scheduling of multiple Smalltalk processes
    4. Support timer-based execution of code blocks
    
    The implementation uses an interrupt-driven model where events can trigger
    the execution of Smalltalk blocks by interrupting the main interpreter loop.
    This is particularly important for embedded applications on the ESP32 where
    hardware events need immediate attention.
    
    For ESP32 targets, the implementation leverages FreeRTOS message queues and
    task facilities to provide true concurrent execution. On non-ESP32 platforms
    (like Mac), simplified single-threaded versions of these functions are provided
    for development purposes.
    
    This architecture allows Smalltalk code to respond to external events
    (such as button presses, sensor readings, or network events) while
    maintaining the illusion of seamless execution within the Smalltalk
    environment.
*/

#include "build.h"
#include "process.h"
#include "names.h"

/**
 * Block run queue depth
 * 
 * Defines the maximum number of blocks that can be queued for execution
 * at any given time. This limit prevents memory exhaustion in case of
 * a flood of events triggering many blocks.
 * 
 * The queue acts as a buffer between the event producers (hardware
 * interrupts, timers, etc.) and the Smalltalk interpreter that consumes
 * and executes the blocks.
 */
#define BLOCK_RUN_QUEUE_DEPTH       16

// External function declarations
extern void runMethodOrBlock(object method, object block, object arg);
extern void runBlock(object block, object arg);
extern boolean execute(object aProcess, int maxsteps);

/**
 * Run a Smalltalk process to completion
 * 
 * This function executes a Smalltalk process object until it naturally
 * terminates. It repeatedly calls the bytecode interpreter with a fixed
 * time slice (15000 bytecodes), allowing the process to execute in
 * manageable chunks.
 * 
 * The process runs until execute() returns false, indicating the process
 * has terminated (typically by returning from its top-level method).
 * 
 * @param processToRun The Smalltalk process object to execute
 */
void runSmalltalkProcess(object processToRun)
{
	if (processToRun != nilobj)
	{
        // Run the process with 15000 bytecodes per time slice until it's complete
        while (execute(processToRun, 15000));
	}
	else
	{
		fprintf(stderr, "<%s>: %s\n", "runSmalltalkProcess", "trying to run nil process");
	}
}

/**
 * Execute either a method or block with an argument
 * 
 * This function creates a new Smalltalk process to execute either a method or
 * a block (one of which must be nil). It sets up the process stack and linkage
 * area appropriately for the execution, and then runs the process to completion.
 * 
 * This is a core utility function used by various parts of the system to execute
 * Smalltalk code on demand, such as in response to events or scheduled tasks.
 * 
 * @param method The method to execute (or nilobj if executing a block)
 * @param block The block to execute (or nilobj if executing a method)
 * @param arg The argument to pass to the method or block
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

/**
 * Set an argument for a block
 * 
 * This helper function adds an argument to a block's context by storing it in
 * the block's temporary variables array. This makes the argument accessible
 * to the block when it executes.
 * 
 * The function operates directly on the block's context structure, modifying
 * its temporaries field to contain the argument value.
 * 
 * @param block The block object to receive the argument
 * @param arg The argument to add to the block
 */
void addArgToBlock(object block, object arg)
{
	if (block != nilobj)
	{
		// Store the argument in the block's context temporaries
		// Note: There are commented alternatives that were tried previously
		basicAtPut(basicAt(block, contextInBlock), temporariesInContext, arg);
	}
}

/**
 * Reusable array for block + argument queueing
 * 
 * This global variable holds an array used to package a block and its argument
 * when queueing it for execution. Rather than creating a new array each time,
 * this single instance is reused for efficiency.
 */
object queueBlockArray = nilobj;

/**
 * Queue a block for execution with an argument
 * 
 * This function adds a block to the execution queue, associating it with an
 * argument that will be passed when the block is eventually executed. The
 * block will be picked up and executed when the interpreter next checks the
 * queue.
 * 
 * If the block needs an argument, the function creates an array containing both
 * the block and its argument, then queues that array. When executed, the Smalltalk
 * system will recognize this array format and call value: on the block with the
 * argument.
 * 
 * @param block The Smalltalk block object to queue
 * @param arg The argument to pass to the block when executed
 */
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

/**
 * Execute a block immediately with an argument
 * 
 * This function runs the specified block right away, passing it the given
 * argument. The execution happens synchronously in the current thread.
 * 
 * The function first adds the argument to the block's context, then creates
 * a new process to execute the block. This is a convenience wrapper around
 * addArgToBlock and runMethodOrBlock.
 * 
 * @param block The Smalltalk block object to execute
 * @param arg The argument to pass to the block
 */
void runBlock(object block, object arg)
{
	/* put argument in block temps */
	if (block != nilobj)
	{
		addArgToBlock(block, arg);
		runMethodOrBlock(nilobj, block, arg);
	}
}

/**
 * ESP32-specific Implementation
 * 
 * The following code is conditionally compiled only for ESP32 targets.
 * It provides FreeRTOS-based implementations of the concurrency functions
 * defined in process.h, using tasks and queues for block execution.
 */
#ifdef TARGET_ESP32

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

/**
 * Task Block Argument Structure
 * 
 * This structure encapsulates the information needed by a task
 * to execute a delayed block. It contains:
 * - The block to execute
 * - The argument to pass to the block
 * - The number of ticks to delay before execution
 * 
 * This structure is passed to the taskRunBlockAfter function when
 * creating a new FreeRTOS task for delayed execution.
 */
typedef struct
{
	object block; // block to run
	object arg;	  // and block argument
	int ticks;	  // ticks to delay before running
} task_block_arg;

/**
 * Queue handle for VM blocks
 * 
 * This FreeRTOS queue holds blocks that are waiting to be executed
 * by the Smalltalk interpreter. It serves as the communication channel
 * between various tasks that queue blocks and the main interpreter task
 * that executes them.
 */
static QueueHandle_t vmBlockToRunQueue;

/**
 * Task function to run a block after a delay
 * 
 * This function is the entry point for a FreeRTOS task that executes
 * a block after a specified delay. It:
 * 1. Waits for the specified number of ticks
 * 2. Attempts to interrupt the interpreter (waiting if necessary)
 * 3. Queues the block for execution
 * 4. Deletes itself (the task) once the block is queued
 * 
 * @param taskBlockArg Pointer to a task_block_arg structure containing
 *                     the block, argument, and delay information
 */
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

/**
 * Execute a string of Smalltalk code
 * 
 * This function compiles and executes a string of Smalltalk code,
 * passing the specified argument to it. It creates a new method
 * on the fly from the provided text, then executes that method.
 * 
 * This is useful for executing dynamically generated Smalltalk code
 * or for evaluating code strings provided from external sources.
 * 
 * @param text The Smalltalk code to execute as a null-terminated string
 * @param arg The argument to pass to the compiled method
 */
void doIt(char *text, object arg)
{
	object method;

	method = newMethod();
	incr(method);
	setInstanceVariables(nilobj);
	ignore parse(method, text, false);

	runMethodOrBlock(method, nilobj, arg);
}

/**
 * Schedule a block to run after a delay
 * 
 * This function creates a new FreeRTOS task that will execute the specified
 * block after waiting for the given number of ticks. This provides a simple
 * timer mechanism for deferred execution of Smalltalk code.
 * 
 * The function is typically called by primitive 152 from Smalltalk code.
 * 
 * @param block The Smalltalk block object to execute
 * @param arg The argument to pass to the block when executed
 * @param ticks The number of ticks to wait before execution
 */
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

/**
 * Initialize the VM block execution queue
 * 
 * This function creates a FreeRTOS queue that will hold blocks waiting
 * to be executed by the Smalltalk interpreter. The queue has a fixed
 * depth defined by BLOCK_RUN_QUEUE_DEPTH and stores object references.
 * 
 * This must be called before any other queue operations can be performed.
 */
void initVMBlockToRunQueue()
{
    vmBlockToRunQueue = xQueueCreate( BLOCK_RUN_QUEUE_DEPTH, sizeof(object) );
}

/**
 * Add a block to the VM execution queue
 * 
 * This function places a block in the queue to be executed by the VM
 * when it gets a chance. It increments the reference count on the block
 * to ensure it isn't garbage collected before execution.
 * 
 * The function will wait indefinitely (portMAX_DELAY) if the queue is full.
 * 
 * @param block The Smalltalk block object to queue for execution
 * @return true if the block was successfully queued, false otherwise
 */
boolean queueVMBlockToRun(object block)
{
	// We increment the reference count to ensure the block isn't garbage collected
    incr(block);
    BaseType_t result = xQueueSend( vmBlockToRunQueue, &block, portMAX_DELAY);
    return result == pdPASS;
}


/**
 * Check if any VM blocks are queued
 * 
 * This function tests whether there are any blocks waiting in the execution queue.
 * It's used by the main interpreter loop to determine if it needs to check for
 * blocks to execute.
 * 
 * @return true if there are blocks in the queue, false if the queue is empty
 */
boolean isVMBlockQueued()
{
    return uxQueueMessagesWaiting( vmBlockToRunQueue ) > 0;
}

/**
 * Get the next VM block to execute
 * 
 * This function retrieves and removes the next block from the execution queue.
 * If the queue is empty, it returns nilobj. Otherwise, it waits indefinitely
 * (portMAX_DELAY) for a block to become available.
 * 
 * Note that the caller is responsible for decrementing the reference count
 * on the returned block after it's no longer needed.
 * 
 * @return The next block object to run, or nilobj if the queue is empty
 */
object getNextVMBlockToRun()
{
    object nextToRun;
    if (isVMBlockQueued()) {
        BaseType_t result = xQueueReceive(vmBlockToRunQueue, &nextToRun, portMAX_DELAY);
        return (result == pdPASS) ? nextToRun : nilobj;
    }
    return nilobj;
}

/**
 * Task function for executing Smalltalk code
 * 
 * This function is the entry point for a FreeRTOS task that executes
 * a string of Smalltalk code. It calls doIt() with the provided text
 * and argument, then deletes itself when finished.
 * 
 * @param evalText The Smalltalk code to execute as a null-terminated string
 * @param arg The argument to pass to the compiled method
 */
void evalTask(void *evalText, object arg)
{
	doIt(evalText, arg);
	vTaskDelete(NULL);
}

extern boolean interruptInterpreter();

/**
 * Create a task to evaluate Smalltalk code asynchronously
 * 
 * This function creates a new FreeRTOS task that will execute the
 * specified Smalltalk code string with the given argument. The execution
 * happens in a separate task, allowing it to run concurrently with
 * the main interpreter loop.
 * 
 * This is useful for running background operations or responding to
 * events without blocking the main Smalltalk environment.
 * 
 * @param evalText The Smalltalk code to execute as a null-terminated string
 * @param arg The argument to pass to the compiled method
 */
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

#else /* Non-ESP32 Platforms */

/**
 * Dummy queue index for non-ESP32 platforms
 * 
 * This variable isn't actually used in the simplified non-ESP32 implementation,
 * but is included to maintain consistency with the ESP32 version.
 */
static int runBlockQueueIndex = 0;

/**
 * Non-ESP32 implementations of the process management functions
 * 
 * These are simplified stubs of the ESP32 functions for use on development
 * platforms like Mac. They don't provide actual concurrency, but allow the
 * code to compile and run in a single-threaded manner.
 */
void initVMBlockToRunQueue() {}
object getNextVMBlockToRun() { return nilobj; }
boolean queueVMBlockToRun(object block) { return true; }
boolean isVMBlockQueued() { return false; }

extern void doIt(char *text, object arg);

/**
 * Synchronous implementation of forkEval for non-ESP32 platforms
 * 
 * This function simply executes the provided Smalltalk code immediately
 * in the current thread, rather than creating a separate task as in the
 * ESP32 version.
 * 
 * @param evalText The Smalltalk code to execute as a null-terminated string
 * @param arg The argument to pass to the compiled method
 */
void forkEval(char *evalText, object arg)
{
	doIt(evalText, arg);
}

#endif /* TARGET_ESP32 */
