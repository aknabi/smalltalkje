/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021
	Based on Little Smalltalk, version 2
	Written by Tim Budd, Oregon State University, July 1987

	Method Parser Module
	
	This module implements a recursive descent parser for Smalltalk methods,
	translating source code into bytecodes and literals. Key capabilities:
	
	1. Parsing and Code Generation:
	   - Parses Smalltalk method syntax (unary, binary, keyword)
	   - Generates bytecodes representing executable instructions
	   - Builds literal tables for constants (numbers, strings, symbols)
	
	2. Variable Resolution:
	   - Handles references to self, super
	   - Correctly encodes references to temporaries, arguments, and instance variables
	   - Resolves global variable lookup at runtime
	
	3. Method Structure Processing:
	   - Handles method selector patterns
	   - Processes temporary variable declarations
	   - Supports return statements (^)
	   - Implements blocks and optimized blocks
	
	4. Key Optimizations:
	   - Special handling for common control structures (ifTrue:ifFalse:, whileTrue:)
	   - Specialized bytecodes for common unary and binary messages
	
	Usage: To parse a method, first call setInstanceVariables() with the class object
	to properly encode instance variable references, then call parse() with the
	method object and source text.
	
	This is a recursive descent parser - the grammar rules are implemented in a
	top-down manner where each function corresponds to a non-terminal in the
	grammar. Functions call each other to parse nested constructs, following the
	syntactic structure of the language.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "env.h"
#include "memory.h"
#include "names.h"
#include "interp.h"
#include "lex.h"

/* all of the following limits could be increased (up to
		   256) without any trouble.  They are kept low 
		   to keep memory utilization down */

#define codeLimit 256	  /* maximum number of bytecodes permitted */
#define literalLimit 128  /* maximum number of literals permitted */
#define temporaryLimit 32 /* maximum number of temporaries permitted */
#define argumentLimit 32  /* maximum number of arguments permitted */
#define instanceLimit 32  /* maximum number of instance vars permitted */
#define methodLimit 64	  /* maximum number of methods permitted */

boolean parseok; /* parse still ok? */
extern char peek(void);
int codeTop;			   /* top position filled in code array */
byte codeArray[codeLimit]; /* bytecode array */
int literalTop;			   /*  ... etc. */
object literalArray[literalLimit];
int temporaryTop;
char *temporaryName[temporaryLimit];
int argumentTop;
char *argumentName[argumentLimit];
int instanceTop;
char *instanceName[instanceLimit];

int maxTemporary;  /* highest temporary see so far */
char selector[80]; /* message selector */

enum blockstatus
{
	NotInBlock,
	InBlock,
	OptimizedBlock
} blockstat;

void block(void);
void body(void);
void assignment(char *name);
void expression(void);
void genMessage(boolean toSuper, int argumentCount, object messagesym);
void genInteger(int val);

void compilWarn(char *selector, char *str1, char *str2);
void compilError(char *selector, char *str1, char *str2);
void parsePrimitive(void);
void sysDecr(object z);

/**
 * Initializes the instance variable table for parsing methods
 * 
 * This function builds a mapping of instance variable names for a given class.
 * It recursively processes the class hierarchy from top to bottom, so that
 * instance variables in superclasses appear first in the table. This mapping
 * is used during parsing to correctly encode instance variable references
 * in the bytecode.
 * 
 * @param aClass The class whose instance variables should be processed
 */
void setInstanceVariables(object aClass)
{
	int i, limit;
	object vars;

	if (aClass == nilobj)
		instanceTop = 0;
	else
	{
		setInstanceVariables(basicAt(aClass, superClassInClass));
		vars = basicAt(aClass, variablesInClass);
		if (vars != nilobj)
		{
			limit = sizeField(vars);
			for (i = 1; i <= limit; i++)
				instanceName[++instanceTop] = charPtr(basicAt(vars, i));
		}
	}
}

/**
 * Generates a single bytecode value
 * 
 * This low-level function appends a single byte to the bytecode array.
 * It performs bounds checking to ensure we don't exceed code size limits.
 * 
 * @param value The bytecode value to append (0-255)
 */
void genCode(int value)
{
	if (codeTop >= codeLimit)
		compilError(selector, "too many bytecode instructions in method",
					"");
	else
		codeArray[codeTop++] = value;
}

/**
 * Generates a bytecode instruction
 * 
 * Bytecode instructions in Smalltalkje consist of a high nibble (opcode)
 * and a low nibble (operand). This function handles the encoding and
 * packing of these values, and also handles the extension mechanism for
 * operands larger than 15.
 * 
 * @param high The high nibble (opcode) of the instruction
 * @param low The low nibble (operand) of the instruction
 */
void genInstruction(int high, int low)
{
	if (low >= 16)
	{
		genInstruction(Extended, high);
		genCode(low);
	}
	else
		genCode(high * 16 + low);
}

/**
 * Adds a literal to the literal table
 * 
 * This function adds a literal object (like a number, string, or symbol)
 * to the method's literal table and returns the index of the literal.
 * It handles reference counting by incrementing the object's reference count.
 * 
 * @param aLiteral The literal object to add
 * @return The index of the literal in the literal table
 */
int genLiteral(object aLiteral)
{
	if (literalTop >= literalLimit)
		compilError(selector, "too many literals in method", "");
	else
	{
		literalArray[++literalTop] = aLiteral;
		incr(aLiteral);
	}
	return (literalTop - 1);
}

/**
 * Generates bytecode to push an integer onto the stack
 * 
 * This function optimizes integer pushing by using special bytecodes
 * for common small integers (-1, 0, 1, 2). For other integers, it creates
 * a literal Integer object and generates code to push it.
 * 
 * @param val The integer value to push
 */
void genInteger(val) /* generate an integer push */
	int val;
{
	if (val == -1)
		genInstruction(PushConstant, minusOne);
	else if ((val >= 0) && (val <= 2))
		genInstruction(PushConstant, val);
	else
		genInstruction(PushLiteral, genLiteral(newInteger(val)));
}

char *glbsyms[] = {"currentInterpreter", "nil", "true", "false",
				   0};

/**
 * Processes a name reference in the code
 * 
 * This function handles references to variables by name. It determines the
 * variable's type (self/super, temporary, argument, instance variable, or global)
 * and generates the appropriate bytecode to push its value onto the stack.
 * 
 * The function searches for the name in various scopes, in order:
 * 1. Self/super (special variables)
 * 2. Temporary variables
 * 3. Method arguments
 * 4. Instance variables
 * 5. Global constants (nil, true, false, etc.)
 * 6. Global variables (looked up at runtime)
 * 
 * @param name The name to process
 * @return true if the name was 'super', false otherwise
 */
boolean nameTerm(char *name)
{
	int i;
	boolean done = false;
	boolean isSuper = false;

	/* it might be self or super */
	if (streq(name, "self") || streq(name, "super"))
	{
		genInstruction(PushArgument, 0);
		done = true;
		if (streq(name, "super"))
			isSuper = true;
	}

	/* or it might be a temporary (reverse this to get most recent first) */
	if (!done)
		for (i = temporaryTop; (!done) && (i >= 1); i--)
			if (streq(name, temporaryName[i]))
			{
				genInstruction(PushTemporary, i - 1);
				done = true;
			}

	/* or it might be an argument */
	if (!done)
		for (i = 1; (!done) && (i <= argumentTop); i++)
			if (streq(name, argumentName[i]))
			{
				genInstruction(PushArgument, i);
				done = true;
			}

	/* or it might be an instance variable */
	if (!done)
		for (i = 1; (!done) && (i <= instanceTop); i++)
		{
			if (streq(name, instanceName[i]))
			{
				genInstruction(PushInstance, i - 1);
				done = true;
			}
		}

	/* or it might be a global constant */
	if (!done)
		for (i = 0; (!done) && glbsyms[i]; i++)
			if (streq(name, glbsyms[i]))
			{
				genInstruction(PushConstant, i + 4);
				done = true;
			}

	/* not anything else, it must be a global */
	/* must look it up at run time */
	if (!done)
	{
		genInstruction(PushLiteral, genLiteral(newSymbol(name)));
		genMessage(false, 0, newSymbol("value"));
	}

	return (isSuper);
}

/**
 * Parses a literal array expression (#( ... ))
 * 
 * This function handles literal array expressions in Smalltalk code.
 * It parses the array contents (which can include nested arrays, numbers,
 * symbols, strings, etc.) and creates an Array object containing these
 * elements. The array is added to the literal table.
 * 
 * @return The index of the array in the literal table
 */
int parseArray()
{
	int i, size, base;
	object newLit, obj;

	base = literalTop;
	ignore nextToken();
	while (parseok && (token != closing))
	{
		switch (token)
		{
		case arraybegin:
			ignore parseArray();
			break;

		case intconst:
			ignore genLiteral(newInteger(tokenInteger));
			ignore nextToken();
			break;

		case floatconst:
			ignore genLiteral(newFloat(tokenFloat));
			ignore nextToken();
			break;

		case nameconst:
		case namecolon:
		case symconst:
			ignore genLiteral(newSymbol(tokenString));
			ignore nextToken();
			break;

		case binary:
			if (streq(tokenString, "("))
			{
				ignore parseArray();
				break;
			}
			if (streq(tokenString, "-") && isdigit(peek()))
			{
				ignore nextToken();
				if (token == intconst)
					ignore genLiteral(newInteger(-tokenInteger));
				else if (token == floatconst)
				{
					ignore genLiteral(newFloat(-tokenFloat));
				}
				else
					compilError(selector, "negation not followed",
								"by number");
				ignore nextToken();
				break;
			}
			ignore genLiteral(newSymbol(tokenString));
			ignore nextToken();
			break;

		case charconst:
			ignore genLiteral(newChar(tokenInteger));
			ignore nextToken();
			break;

		case strconst:
			ignore genLiteral(newStString(tokenString));
			ignore nextToken();
			break;

		default:
			compilError(selector, "illegal text in literal array",
						tokenString);
			ignore nextToken();
			break;
		}
	}

	if (parseok)
	{
		if (!streq(tokenString, ")"))
			compilError(selector,
						"array not terminated by right parenthesis",
						tokenString);
		else
			ignore nextToken();
	}
	size = literalTop - base;
	newLit = newArray(size);
	for (i = size; i >= 1; i--)
	{
		obj = literalArray[literalTop];
		basicAtPut(newLit, i, obj);
		decr(obj);
		literalArray[literalTop] = nilobj;
		literalTop = literalTop - 1;
	}
	return (genLiteral(newLit));
}

/**
 * Parses a term (basic expression unit)
 * 
 * This function handles the parsing of basic expression elements, including:
 * - Variable references
 * - Literals (numbers, characters, symbols, strings)
 * - Array literals
 * - Parenthesized expressions
 * - Blocks
 * - Primitives
 * 
 * @return true if the term was 'super', false otherwise
 */
boolean term()
{
	boolean superTerm = false; /* true if term is pseudo var super */

	if (token == nameconst)
	{
		superTerm = nameTerm(tokenString);
		ignore nextToken();
	}
	else if (token == intconst)
	{
		genInteger(tokenInteger);
		ignore nextToken();
	}
	else if (token == floatconst)
	{
		genInstruction(PushLiteral, genLiteral(newFloat(tokenFloat)));
		ignore nextToken();
	}
	else if ((token == binary) && streq(tokenString, "-"))
	{
		ignore nextToken();
		if (token == intconst)
			genInteger(-tokenInteger);
		else if (token == floatconst)
		{
			genInstruction(PushLiteral, genLiteral(newFloat(-tokenFloat)));
		}
		else
			compilError(selector, "negation not followed", "by number");
		ignore nextToken();
	}
	else if (token == charconst)
	{
		genInstruction(PushLiteral, genLiteral(newChar(tokenInteger)));
		ignore nextToken();
	}
	else if (token == symconst)
	{
		genInstruction(PushLiteral, genLiteral(newSymbol(tokenString)));
		ignore nextToken();
	}
	else if (token == strconst)
	{
		genInstruction(PushLiteral, genLiteral(newStString(tokenString)));
		ignore nextToken();
	}
	else if (token == arraybegin)
	{
		genInstruction(PushLiteral, parseArray());
	}
	else if ((token == binary) && streq(tokenString, "("))
	{
		ignore nextToken();
		expression();
		if (parseok)
		{
			if ((token != closing) || !streq(tokenString, ")"))
				compilError(selector, "Missing Right Parenthesis", "");
			else
				ignore nextToken();
		}
	}
	else if ((token == binary) && streq(tokenString, "<"))
		parsePrimitive();
	else if ((token == binary) && streq(tokenString, "["))
		block();
	else
		compilError(selector, "invalid expression start", tokenString);

	return (superTerm);
}

/**
 * Parses a primitive expression (< n ... >)
 * 
 * This function handles primitive expressions, which directly invoke
 * native code implemented in C. It parses the primitive number and arguments,
 * and generates the appropriate bytecode.
 * 
 * Primitives are a low-level escape mechanism that allows Smalltalk code
 * to access functionality that cannot be implemented in Smalltalk itself.
 */
void parsePrimitive()
{
	int primitiveNumber, argumentCount;

	if (nextToken() != intconst)
		compilError(selector, "primitive number missing", "");
	primitiveNumber = tokenInteger;
	ignore nextToken();
	argumentCount = 0;
	while (parseok && !((token == binary) && streq(tokenString, ">")))
	{
		ignore term();
		argumentCount++;
	}
	genInstruction(DoPrimitive, argumentCount);
	genCode(primitiveNumber);
	ignore nextToken();
}

/**
 * Generates bytecode for a message send
 * 
 * This function generates the bytecode for sending a message to an object.
 * It includes optimizations for common unary and binary messages, and
 * handles sending messages to 'super' differently from normal message sends.
 * 
 * @param toSuper Whether the message is being sent to 'super'
 * @param argumentCount The number of arguments to the message
 * @param messagesym The message selector (as a symbol)
 */
void genMessage(toSuper, argumentCount, messagesym)
	boolean toSuper;
int argumentCount;
object messagesym;
{
	boolean sent = false;
	int i;

	if ((!toSuper) && (argumentCount == 0))
		for (i = 0; (!sent) && unSyms[i]; i++)
			if (messagesym == unSyms[i])
			{
				genInstruction(SendUnary, i);
				sent = true;
			}

	if ((!toSuper) && (argumentCount == 1))
		for (i = 0; (!sent) && binSyms[i]; i++)
			if (messagesym == binSyms[i])
			{
				genInstruction(SendBinary, i);
				sent = true;
			}

	if (!sent)
	{
		genInstruction(MarkArguments, 1 + argumentCount);
		if (toSuper)
		{
			genInstruction(DoSpecial, SendToSuper);
			genCode(genLiteral(messagesym));
		}
		else
			genInstruction(SendMessage, genLiteral(messagesym));
	}
}

/**
 * Parses a sequence of unary messages
 * 
 * This function handles chains of unary messages (messages with no arguments)
 * sent to the same receiver. It also performs warning checks for unary message
 * names that conflict with variable names.
 * 
 * @param superReceiver Whether the initial receiver is 'super'
 * @return Whether the final receiver is 'super'
 */
boolean unaryContinuation(boolean superReceiver)
{
	int i;
	boolean sent;

	while (parseok && (token == nameconst))
	{
		/* first check to see if it could be a temp by mistake */
		for (i = 1; i < temporaryTop; i++)
			if (streq(tokenString, temporaryName[i]))
				compilWarn(selector, "message same as temporary:",
						   tokenString);
		for (i = 1; i < argumentTop; i++)
			if (streq(tokenString, argumentName[i]))
				compilWarn(selector, "message same as argument:",
						   tokenString);
		/* the next generates too many spurious messages */
		/* for (i=1; i < instanceTop; i++)
	   if (streq(tokenString, instanceName[i]))
	   compilWarn(selector,"message same as instance",
	   tokenString); */

		sent = false;

		if (!sent)
		{
			genMessage(superReceiver, 0, newSymbol(tokenString));
		}
		/* once a message is sent to super, reciever is not super */
		superReceiver = false;
		ignore nextToken();
	}
	return (superReceiver);
}

/**
 * Parses a sequence of binary messages
 * 
 * This function handles chains of binary messages (messages with one argument).
 * It ensures correct precedence by parsing unary messages within each argument.
 * 
 * @param superReceiver Whether the initial receiver is 'super'
 * @return Whether the final receiver is 'super'
 */
boolean binaryContinuation(boolean superReceiver)
{
	boolean superTerm;
	object messagesym;

	superReceiver = unaryContinuation(superReceiver);
	while (parseok && (token == binary))
	{
		messagesym = newSymbol(tokenString);
		ignore nextToken();
		superTerm = term();
		ignore unaryContinuation(superTerm);
		genMessage(superReceiver, 1, messagesym);
		superReceiver = false;
	}
	return (superReceiver);
}

/**
 * Optimizes control flow constructs with blocks
 * 
 * This function implements special bytecode optimizations for control structures
 * like ifTrue:/ifFalse:, whileTrue:, and:, or:. Instead of using general
 * message sending mechanism (which is slower), it generates specialized bytecode
 * sequences that implement these control structures more efficiently.
 * 
 * The optimization works by:
 * 1. Generating a conditional or unconditional branch instruction
 * 2. Leaving space for the branch target address (filled in later)
 * 3. Optionally generating code to pop the condition result
 * 4. Processing the block or expression that follows
 * 5. Patching the branch target address to point to the instruction after the block
 * 
 * This produces much more efficient code than the standard approach of
 * sending messages to blocks, especially for common control structures.
 * 
 * @param instruction The special branch instruction to generate (BranchIfTrue, BranchIfFalse, etc.)
 * @param dopop Whether to pop the result after the branch
 * @return The location in the code array where the branch target was stored (for later chaining)
 */
int optimizeBlock(int instruction, boolean dopop)
{
	int location;               /* Location to patch with branch target */
	enum blockstatus savebstat; /* Saved block status for restoration */

	/* Save current block status */
	savebstat = blockstat;
	
	/* Generate the branch instruction */
	genInstruction(DoSpecial, instruction);
	
	/* Remember location for branch target (to be filled in later) */
	location = codeTop;
	genCode(0);                 /* Placeholder for branch target */
	
	/* Optionally pop the condition value */
	if (dopop)
		genInstruction(DoSpecial, PopTop);
		
	ignore nextToken();
	
	/* Handle block syntax or normal expression */
	if (streq(tokenString, "["))
	{
		/* Process a block */
		ignore nextToken();
		
		/* Mark this as an optimized block if we weren't in a block already */
		if (blockstat == NotInBlock)
			blockstat = OptimizedBlock;
			
		body();  /* Parse the block body */
		
		/* Check for proper block termination */
		if (!streq(tokenString, "]"))
			compilError(selector, "missing close", "after block");
			
		ignore nextToken();
	}
	else
	{
		/* Process a normal expression and send 'value' to it */
		ignore binaryContinuation(term());
		genMessage(false, 0, newSymbol("value"));
	}
	
	/* Patch the branch target to point to the next instruction */
	codeArray[location] = codeTop + 1;
	
	/* Restore previous block status */
	blockstat = savebstat;
	
	return (location);
}

/**
 * Parses a sequence of keyword messages
 * 
 * This function handles keyword messages (messages with named arguments like key1:arg1 key2:arg2).
 * It includes special optimizations for common control structures, converting them directly to
 * efficient branch instructions rather than using the standard message sending mechanism.
 * 
 * The function specifically optimizes:
 * - ifTrue:/ifFalse: - Conditional branches
 * - whileTrue: - Loop structures
 * - and:/or: - Short-circuit boolean operations
 * 
 * For regular keyword messages, it builds the selector by concatenating all keywords
 * and processes each argument (which may itself contain binary and unary messages).
 * 
 * @param superReceiver Whether the initial receiver is 'super'
 * @return Whether the final receiver is 'super' (always false for keyword messages)
 */
boolean keyContinuation(boolean superReceiver)
{
	int i, j;                /* Used for branch patching locations */
	int argumentCount;       /* Number of arguments in keyword message */
	boolean sent, superTerm;
	object messagesym;       /* Message selector as a symbol */
	char pattern[80];        /* Buffer to build the complete selector */

	/* First process any binary messages (maintaining precedence) */
	superReceiver = binaryContinuation(superReceiver);
	
	if (token == namecolon)
	{
		/* Handle control flow optimizations */
		if (streq(tokenString, "ifTrue:"))
		{
			/* Generate code that branches if condition is false */
			i = optimizeBlock(BranchIfFalse, false);
			
			/* Look for an ifFalse: part to handle full if-then-else */
			if (streq(tokenString, "ifFalse:"))
			{
				/* Patch branch target to skip over the "then" part */
				codeArray[i] = codeTop + 3;
				
				/* Generate unconditional branch after "then" part to skip "else" part */
				ignore optimizeBlock(Branch, true);
			}
		}
		else if (streq(tokenString, "ifFalse:"))
		{
			/* Similar to ifTrue:, but branches if condition is true */
			i = optimizeBlock(BranchIfTrue, false);
			
			if (streq(tokenString, "ifTrue:"))
			{
				codeArray[i] = codeTop + 3;
				ignore optimizeBlock(Branch, true);
			}
		}
		else if (streq(tokenString, "whileTrue:"))
		{
			/* Remember start of loop for branch back */
			j = codeTop;
			
			/* Duplicate condition block so we can evaluate it again */
			genInstruction(DoSpecial, Duplicate);
			
			/* Evaluate the condition block */
			genMessage(false, 0, newSymbol("value"));
			
			/* Branch to end if condition is false */
			i = optimizeBlock(BranchIfFalse, false);
			
			/* Pop the condition block (no longer needed) */
			genInstruction(DoSpecial, PopTop);
			
			/* Unconditional branch back to start of loop */
			genInstruction(DoSpecial, Branch);
			genCode(j + 1);
			
			/* Patch exit branch to point after the loop */
			codeArray[i] = codeTop + 1;
			
			/* Pop the condition block from the original stack position */
			genInstruction(DoSpecial, PopTop);
		}
		else if (streq(tokenString, "and:"))
			/* Short-circuit AND - only evaluates second block if first is true */
			ignore optimizeBlock(AndBranch, false);
		else if (streq(tokenString, "or:"))
			/* Short-circuit OR - only evaluates second block if first is false */
			ignore optimizeBlock(OrBranch, false);
		else
		{
			/* Handle standard keyword messages */
			pattern[0] = '\0';
			argumentCount = 0;
			
			/* Process each keyword:argument pair */
			while (parseok && (token == namecolon))
			{
				/* Build the selector by concatenating all keywords */
				ignore strcat(pattern, tokenString);
				argumentCount++;
				ignore nextToken();
				
				/* Parse the argument (which may include unary and binary messages) */
				superTerm = term();
				ignore binaryContinuation(superTerm);
			}
			sent = false;

			/* Create symbol for the complete selector */
			messagesym = newSymbol(pattern);

			if (!sent)
			{
				/* Generate standard message send bytecode */
				genMessage(superReceiver, argumentCount, messagesym);
			}
		}
		
		/* After any keyword message, receiver is no longer super */
		superReceiver = false;
	}
	return (superReceiver);
}

/**
 * Parses message continuations (all types)
 * 
 * This function handles all types of message sends (unary, binary, keyword)
 * and cascaded message sends (using semicolons). It ensures the correct
 * precedence: unary > binary > keyword.
 * 
 * @param superReceiver Whether the initial receiver is 'super'
 */
void continuation(boolean superReceiver)
{
	superReceiver = keyContinuation(superReceiver);

	while (parseok && (token == closing) && streq(tokenString, ";"))
	{
		genInstruction(DoSpecial, Duplicate);
		ignore nextToken();
		ignore keyContinuation(superReceiver);
		genInstruction(DoSpecial, PopTop);
	}
}

/**
 * Parses an expression
 * 
 * This function parses a complete Smalltalk expression, which can be
 * either an assignment or a normal expression (term + continuations).
 * It's a key entry point for the recursive descent parser.
 */
void expression()
{
	boolean superTerm;
	char assignname[60];

	if (token == nameconst)
	{ /* possible assignment */
		ignore strcpy(assignname, tokenString);
		ignore nextToken();
		if ((token == binary) && streq(tokenString, "<-"))
		{
			ignore nextToken();
			assignment(assignname);
		}
		else
		{ /* not an assignment after all */
			superTerm = nameTerm(assignname);
			continuation(superTerm);
		}
	}
	else
	{
		superTerm = term();
		if (parseok)
			continuation(superTerm);
	}
}

/**
 * Processes a variable assignment
 * 
 * This function handles assignments to variables (name <- value).
 * It resolves the variable type (temporary, instance, or global)
 * and generates appropriate bytecode for the assignment.
 * 
 * @param name The name of the variable being assigned to
 */
void assignment(name) char *name;
{
	int i;
	boolean done;

	done = false;

	/* it might be a temporary */
	for (i = temporaryTop; (!done) && (i > 0); i--)
		if (streq(name, temporaryName[i]))
		{
			expression();
			genInstruction(AssignTemporary, i - 1);
			done = true;
		}

	/* or it might be an instance variable */
	for (i = 1; (!done) && (i <= instanceTop); i++)
		if (streq(name, instanceName[i]))
		{
			expression();
			genInstruction(AssignInstance, i - 1);
			done = true;
		}

	if (!done)
	{ /* not known, handle at run time */
		genInstruction(PushArgument, 0);
		genInstruction(PushLiteral, genLiteral(newSymbol(name)));
		expression();
		genMessage(false, 2, newSymbol("assign:value:"));
	}
}

/**
 * Parses a statement
 * 
 * This function handles individual statements in a method body.
 * A statement is either a return statement (^ expression) or
 * a normal expression.
 */
void statement()
{

	if ((token == binary) && streq(tokenString, "^"))
	{
		ignore nextToken();
		expression();
		if (blockstat == InBlock)
		{
			/* change return point before returning */
			genInstruction(PushConstant, contextConst);
			genMessage(false, 0, newSymbol("blockReturn"));
			genInstruction(DoSpecial, PopTop);
		}
		genInstruction(DoSpecial, StackReturn);
	}
	else
	{
		expression();
	}
}

/**
 * Parses a method or block body
 * 
 * This function parses a sequence of statements that form the body
 * of a method or block. It handles statement separators (periods)
 * and ensures the final result is left on the stack.
 */
void body()
{
	/* empty blocks are same as nil */
	if ((blockstat == InBlock) || (blockstat == OptimizedBlock))
		if ((token == closing) && streq(tokenString, "]"))
		{
			genInstruction(PushConstant, nilConst);
			return;
		}

	while (parseok)
	{
		statement();
		if (token == closing)
			if (streq(tokenString, "."))
			{
				ignore nextToken();
				if (token == inputend)
					break;
				else /* pop result, go to next statement */
					genInstruction(DoSpecial, PopTop);
			}
			else
				break; /* leaving result on stack */
		else if (token == inputend)
			break; /* leaving result on stack */
		else
		{
			compilError(selector, "invalid statement ending; token is ",
						tokenString);
		}
	}
}

/**
 * Parses a block expression (closure)
 * 
 * This function handles Smalltalk block expressions, which are anonymous functions
 * enclosed in square brackets. Blocks are first-class objects that capture their lexical
 * environment and can be passed around, stored in variables, and executed later.
 * 
 * Block syntax forms:
 * - Simple block: [ statements ]
 * - Block with arguments: [:arg1 :arg2 | statements ]
 * 
 * Implementation details:
 * 1. Block arguments are treated as temporaries in the current scope
 * 2. A Block object is created with metadata about arguments and their locations
 * 3. Primitive 29 is called to create a runtime closure (captures the context)
 * 4. The bytecode for the block body is compiled inline but skipped by normal execution
 * 5. The return instruction is generated to return from the block properly
 * 
 * Blocks are central to Smalltalk's design - control structures like if/while are
 * implemented as regular methods that take blocks as arguments rather than being
 * hardcoded into the language syntax.
 */
void block()
{
	int saveTemporary;       /* Save temporary variables to restore after block */
	int argumentCount;       /* Number of block arguments */
	int fixLocation;         /* Position to patch for jumping around block code */
	object tempsym, newBlk;  /* Symbol for temp names and the Block object */
	enum blockstatus savebstat; /* Block status to restore after parsing */

	/* Save current state */
	saveTemporary = temporaryTop;
	savebstat = blockstat;
	argumentCount = 0;
	
	/* Skip the opening '[' */
	ignore nextToken();
	/* Process block arguments if present */
	if ((token == binary) && streq(tokenString, ":"))
	{
		/* Each block argument begins with a colon (e.g., [:x :y | ...]) */
		while (parseok && (token == binary) && streq(tokenString, ":"))
		{
			/* After each colon must come an argument name */
			if (nextToken() != nameconst)
				compilError(selector, "name must follow colon",
							"in block argument list");
							
			/* Block arguments are implemented as temporaries */
			if (++temporaryTop > maxTemporary)
				maxTemporary = temporaryTop;
				
			argumentCount++;
			
			if (temporaryTop > temporaryLimit)
				compilError(selector, "too many temporaries in method", "");
			else
			{
				/* Store argument name in temporary variable table */
				tempsym = newSymbol(tokenString);
				temporaryName[temporaryTop] = charPtr(tempsym);
			}
			ignore nextToken();
		}
		
		/* Block arguments must be terminated by a vertical bar */
		if ((token != binary) || !streq(tokenString, "|"))
			compilError(selector, "block argument list must be terminated",
						"by |");
		ignore nextToken();
	}
	/* Create a new Block object and initialize its properties */
	newBlk = newBlock();
	
	/* Set argument count and starting position in the block */
	basicAtPut(newBlk, argumentCountInBlock, newInteger(argumentCount));
	basicAtPut(newBlk, argumentLocationInBlock, newInteger(saveTemporary + 1));
	
	/* Generate code to create a block closure at runtime:
	   1. Push the block template object as a literal
	   2. Push the current context
	   3. Call primitive 29 which creates a BlockClosure
	      (This captures the lexical environment) */
	genInstruction(PushLiteral, genLiteral(newBlk));
	genInstruction(PushConstant, contextConst);
	genInstruction(DoPrimitive, 2);
	genCode(29);
	
	/* Generate an unconditional branch to skip over the block's bytecode
	   (The block code itself is not executed when encountered, only when the block is later activated) */
	genInstruction(DoSpecial, Branch);
	fixLocation = codeTop;       /* Remember this location to patch later */
	genCode(0);                  /* Placeholder for jump target */
	
	/* Store the bytecode position where the block's code begins */
	basicAtPut(newBlk, bytecountPositionInBlock, newInteger(codeTop + 1));
	/* Mark that we're now parsing inside a block body (affects return statements) */
	blockstat = InBlock;
	
	/* Parse the block's body (sequence of statements) */
	body();
	
	/* Verify the block is properly terminated with closing bracket */
	if ((token == closing) && streq(tokenString, "]"))
		ignore nextToken();
	else 
		compilError(selector, "block not terminated by ]", "");
		
	/* Generate return instruction for the block */
	genInstruction(DoSpecial, StackReturn);
	
	/* Patch the branch target to skip over the block's bytecode */
	codeArray[fixLocation] = codeTop + 1;
	
	/* Restore the original temporary variable count and block status */
	temporaryTop = saveTemporary;
	blockstat = savebstat;
}

/**
 * Parses temporary variable declarations
 * 
 * This function processes temporary variable declarations in a method,
 * which appear between vertical bars (| temp1 temp2 |). It builds
 * a table of temporary variable names for use during parsing.
 */
void temporaries()
{
	object tempsym;

	temporaryTop = 0;
	if ((token == binary) && streq(tokenString, "|"))
	{
		ignore nextToken();
		while (token == nameconst)
		{
			if (++temporaryTop > maxTemporary)
				maxTemporary = temporaryTop;
			if (temporaryTop > temporaryLimit)
				compilError(selector, "too many temporaries in method",
							"");
			else
			{
				tempsym = newSymbol(tokenString);
				temporaryName[temporaryTop] = charPtr(tempsym);
			}
			ignore nextToken();
		}
		if ((token != binary) || !streq(tokenString, "|"))
			compilError(selector, "temporary list not terminated by bar",
						"");
		else
			ignore nextToken();
	}
}

/**
 * Parses a method's message pattern
 * 
 * This function processes the message pattern (selector) of a method,
 * which determines the method's name and arguments. It handles all
 * three types of message patterns:
 * - Unary: methodName
 * - Binary: + argument
 * - Keyword: key1: arg1 key2: arg2
 */
void messagePattern()
{
	object argsym;

	argumentTop = 0;
	ignore strcpy(selector, tokenString);
	if (token == nameconst) /* unary message pattern */
		ignore nextToken();
	else if (token == binary)
	{ /* binary message pattern */
		ignore nextToken();
		if (token != nameconst)
			compilError(selector,
						"binary message pattern not followed by name",
						selector);
		argsym = newSymbol(tokenString);
		argumentName[++argumentTop] = charPtr(argsym);
		ignore nextToken();
	}
	else if (token == namecolon)
	{ /* keyword message pattern */
		selector[0] = '\0';
		while (parseok && (token == namecolon))
		{
			ignore strcat(selector, tokenString);
			ignore nextToken();
			if (token != nameconst)
				compilError(selector, "keyword message pattern",
							"not followed by a name");
			if (++argumentTop > argumentLimit)
				compilError(selector, "too many arguments in method", "");
			argsym = newSymbol(tokenString);
			argumentName[argumentTop] = charPtr(argsym);
			ignore nextToken();
		}
	}
	else
		compilError(selector, "illegal message selector", tokenString);
}

/**
 * Main entry point for the Smalltalk method parser
 * 
 * This function coordinates the entire parsing process for a method, from the selector
 * pattern to the method body, and populates the provided Method object with:
 * - Message selector (method name)
 * - Bytecodes (executable instructions)
 * - Literals (constants used in the method)
 * - Stack and temporary variable size information
 * - Optionally the source text for debugging
 * 
 * The parsing follows Smalltalk's method syntax:
 * 1. Message pattern (selector with arguments)
 * 2. Temporary variable declarations
 * 3. Method body (sequence of statements)
 * 
 * If parsing succeeds, a complete Method object is created that can be executed
 * by the Smalltalk interpreter.
 * 
 * @param method The Method object to populate with parsed information
 * @param text The source code text of the method to parse
 * @param savetext Whether to store the source text in the Method object (for debugging)
 * @return true if parsing succeeded, false if there was an error
 */
boolean parse(object method, char *text, boolean savetext)
{
	int i;
	object bytecodes, theLiterals;
	byte *bp;

	/* Initialize the lexical analyzer with the source text */
	lexinit(text);
	
	/* Initialize parsing state */
	parseok = true;
	blockstat = NotInBlock;
	codeTop = 0;
	literalTop = temporaryTop = argumentTop = 0;
	maxTemporary = 0;

	/* Parse method in sequence: pattern, temporaries, body */
	messagePattern();            /* Method name and arguments */
	if (parseok)
		temporaries();           /* Temporary variable declarations */
	if (parseok)
		body();                  /* Method body (statements) */
	if (parseok)
	{
		/* All methods implicitly return self if no explicit return */
		genInstruction(DoSpecial, PopTop);       /* Discard result of last expression */
		genInstruction(DoSpecial, SelfReturn);   /* Return self */
	}

	/* Handle parsing failure by returning an invalid method object */
	if (!parseok)
	{
		basicAtPut(method, bytecodesInMethod, nilobj);
	}
	else
	{
		/* Parsing succeeded - build a complete Method object */
		
		/* 1. Create a ByteArray containing the compiled bytecodes */
		bytecodes = newByteArray(codeTop);
		bp = bytePtr(bytecodes);
		for (i = 0; i < codeTop; i++)
		{
			bp[i] = codeArray[i];
		}
		
		/* 2. Set the method's name (selector) */
		basicAtPut(method, messageInMethod, newSymbol(selector));
		
		/* 3. Attach the bytecodes to the method */
		basicAtPut(method, bytecodesInMethod, bytecodes);
		
		/* 4. Create and attach the literal table (if any literals exist) */
		if (literalTop > 0)
		{
			theLiterals = newArray(literalTop);
			for (i = 1; i <= literalTop; i++)
			{
				basicAtPut(theLiterals, i, literalArray[i]);
				decr(literalArray[i]);  /* Adjust reference count */
			}
			basicAtPut(method, literalsInMethod, theLiterals);
		}
		else
		{
			basicAtPut(method, literalsInMethod, nilobj);
		}
		
		/* 5. Set stack and temporary variable size information */
		basicAtPut(method, stackSizeInMethod, newInteger(6));  /* Default stack size */
		basicAtPut(method, temporarySizeInMethod, newInteger(1 + maxTemporary));
		
		/* 6. Optionally save the source text for debugging */
		if (savetext)
		{
			basicAtPut(method, textInMethod, newStString(text));
		}
		
		return (true);  /* Parsing succeeded */
	}
	return (false);     /* Parsing failed */
}
