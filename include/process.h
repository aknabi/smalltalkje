/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021

	Process Management Header
	
	This header defines the interface for Smalltalkje's process execution and 
	scheduling system. It provides functions for managing the execution of 
	Smalltalk blocks and processes, which form the basis of Smalltalk's 
	concurrency model.
	
	The system implements a simple cooperative multitasking approach where
	VM blocks (executable chunks of Smalltalk code) can be queued for execution
	and run according to the scheduler's decisions. This enables asynchronous
	operation and event-driven programming in the Smalltalk environment.
	
	This subsystem is especially important for embedded applications on the ESP32
	where background tasks, timers, and event handlers need to coexist with the
	main Smalltalk execution environment.
*/

#include "memory.h"

/**
 * VM Block Queue Management Functions
 * 
 * These functions manage the queue of VM blocks waiting to be executed
 * by the Smalltalk virtual machine. The queue acts as a scheduler for
 * pending blocks of code.
 */

/**
 * Initialize the VM block execution queue
 * 
 * Sets up the queue data structure for blocks that will be executed
 * by the VM. This must be called before any other queue operations.
 */
void initVMBlockToRunQueue();

/**
 * Get the next VM block to execute
 * 
 * Retrieves and removes the next block from the execution queue.
 * 
 * @return The next block object to run, or nil if the queue is empty
 */
object getNextVMBlockToRun();

/**
 * Add a block to the VM execution queue
 * 
 * Places a block in the queue to be executed by the VM when it gets a chance.
 * 
 * @param block The Smalltalk block object to queue for execution
 * @return true if the block was successfully queued, false otherwise
 */
boolean queueVMBlockToRun(object block);

/**
 * Check if any VM blocks are queued
 * 
 * Tests whether there are any blocks waiting in the execution queue.
 * 
 * @return true if there are blocks in the queue, false if the queue is empty
 */
boolean isVMBlockQueued();

/**
 * Scheduling and Execution Functions
 * 
 * These functions handle the actual execution of blocks and processes,
 * including delayed execution and process switching.
 */

/**
 * Schedule a block to run after a delay
 * 
 * Queues a block for execution after a specified number of VM ticks have elapsed.
 * This provides a simple timer mechanism for deferred execution.
 * 
 * @param block The Smalltalk block object to execute
 * @param arg The argument to pass to the block when executed
 * @param ticks The number of VM ticks to wait before execution
 */
void runBlockAfter(object block, object arg, int ticks);

/**
 * Run a Smalltalk process
 * 
 * Switches execution to the specified Smalltalk process object.
 * This is the core of Smalltalk's process switching mechanism.
 * 
 * @param processToRun The process object to start or resume execution of
 */
void runSmalltalkProcess(object processToRun);

/**
 * Block Execution Helpers
 * 
 * These functions provide convenience methods for working with blocks,
 * either executing them immediately or queueing them for later execution.
 */

/**
 * Queue a block for execution with an argument
 * 
 * Adds a block to the execution queue, associating it with an argument
 * that will be passed when the block is eventually executed.
 * 
 * @param block The Smalltalk block object to queue
 * @param arg The argument to pass to the block when executed
 */
void queueBlock(object block, object arg);

/**
 * Execute a block immediately with an argument
 * 
 * Runs the specified block right away, passing it the given argument.
 * This is a synchronous operation that doesn't involve the queue.
 * 
 * @param block The Smalltalk block object to execute
 * @param arg The argument to pass to the block
 */
void runBlock(object block, object arg);
