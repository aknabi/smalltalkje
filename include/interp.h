/*
	Smalltalkje, version 1 based on:

	Little Smalltalk, version 2
	Written by Tim Budd, Oregon State University, July 1987

	Bytecode Interpreter Definitions
	
	This header defines the bytecodes used by the Smalltalk virtual machine.
	The bytecode set forms the instruction set of the VM and determines
	how Smalltalk methods are compiled and executed.
	
	Key components:
	1. Bytecode operation codes (opcodes) - Primary instruction types
	2. Push constants - Special constants that can be pushed with PushConstant
	3. Special operation types - Sub-operations for the DoSpecial opcode
	
	These definitions are used by both the interpreter (execute() function) 
	and the compiler which generates bytecodes from Smalltalk source code.
	
	(This file has remained unchanged for Smalltalkje)
*/

/**
 * Bytecode Instruction Set
 * 
 * These define the primary instruction types that the Smalltalk VM can execute.
 * Each bytecode has a 4-bit opcode (high nibble) and a 4-bit operand (low nibble).
 * For operations that need a larger operand, the Extended opcode is used,
 * which means the next byte is the full operand value.
 * 
 * The bytecode set is designed to efficiently support common Smalltalk operations:
 * - Pushing values onto the stack (variables, literals, constants)
 * - Assigning values to variables
 * - Message sending operations (unary, binary, keyword)
 * - Control flow (via DoSpecial operations)
 * - Primitive operations (direct C implementations for performance)
 */

/* Primary bytecode operations (opcode values 0-15) */
#define Extended 0       /* Use next byte as operand for current operation */
#define PushInstance 1   /* Push instance variable onto stack */
#define PushArgument 2   /* Push method argument onto stack */
#define PushTemporary 3  /* Push temporary variable onto stack */
#define PushLiteral 4    /* Push literal value from literal frame */
#define PushConstant 5   /* Push a constant value (nil, true, etc.) */
#define AssignInstance 6 /* Assign stack top to instance variable */
#define AssignTemporary 7 /* Assign stack top to temporary variable */
#define MarkArguments 8  /* Mark where arguments start on stack for message send */
#define SendMessage 9    /* Send message with literal selector to receiver */
#define SendUnary 10     /* Send common unary message (optimized) */
#define SendBinary 11    /* Send common binary message (optimized) */
#define DoPrimitive 13   /* Execute primitive operation in C code */
#define DoSpecial 15     /* Execute special operations (return, branch, etc.) */

/**
 * Push Constants
 * 
 * These are operand values for the PushConstant bytecode that represent
 * commonly used constants. The numbers 0-2 can be pushed directly,
 * along with other common constants including -1, nil, true, and false.
 * 
 * The contextConst value is special - it creates a BlockContext object
 * for the current activation if needed.
 */
#define minusOne 3      /* Push the integer value -1 */
#define contextConst 4  /* Push the current context (or create a block context) */
#define nilConst 5      /* Push the nil object (null) */
#define trueConst 6     /* Push the true object */
#define falseConst 7    /* Push the false object */

/**
 * Special Operations
 * 
 * These are operand values for the DoSpecial bytecode that represent
 * various control flow and stack manipulation operations.
 * 
 * Control flow operations (Branch, BranchIf*, etc.) are followed by
 * an additional byte that contains the branch offset.
 */
#define SelfReturn 1    /* Return the receiver (self) from current method */
#define StackReturn 2   /* Return top of stack from current method */
#define Duplicate 4     /* Duplicate the top stack value */
#define PopTop 5        /* Discard the top stack value */
#define Branch 6        /* Unconditional branch (jump) */
#define BranchIfTrue 7  /* Branch if top of stack is true */
#define BranchIfFalse 8 /* Branch if top of stack is false */
#define AndBranch 9     /* Logical AND with short-circuit (for 'and:' message) */
#define OrBranch 10     /* Logical OR with short-circuit (for 'or:' message) */
#define SendToSuper 11  /* Send message to superclass implementation */
