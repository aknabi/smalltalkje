#include "memory.h"

typedef struct
{
	object block; // block to run
	object arg;	  // and block argument
} run_block_queue_item;

void initBlockRunQueue();
boolean queueBlockItemToRun(run_block_queue_item blockItem, boolean isHighPrio);
boolean isBlockItemQueued();
void runNextBlockItem();
