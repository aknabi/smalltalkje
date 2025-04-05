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
 * This function implements special optimizations for control structures
 * like ifTrue:/ifFalse:, whileTrue:, and:, or:. Instead of using general
 * message sending, it generates specialized bytecode sequences that are
 * more efficient.
 * 
 * @param instruction The special instruction to generate (BranchIfTrue, etc.)
 * @param dopop Whether to pop the result after the branch
 * @return The location in the code array where the branch target was stored
 */
int optimizeBlock(int instruction, boolean dopop)
{
	int location;
	enum blockstatus savebstat;

	savebstat = blockstat;
	genInstruction(DoSpecial, instruction);
	location = codeTop;
	genCode(0);
	if (dopop)
		genInstruction(DoSpecial, PopTop);
	ignore nextToken();
	if (streq(tokenString, "["))
	{
		ignore nextToken();
		if (blockstat == NotInBlock)
			blockstat = OptimizedBlock;
		body();
		if (!streq(tokenString, "]"))
			compilError(selector, "missing close", "after block");
		ignore nextToken();
	}
	else
	{
		ignore binaryContinuation(term());
		genMessage(false, 0, newSymbol("value"));
	}
	codeArray[location] = codeTop + 1;
	blockstat = savebstat;
	return (location);
}

/**
 * Parses a sequence of keyword messages
 * 
 * This function handles keyword messages (messages with named arguments).
 * It includes special optimizations for control structures and ensures
 * correct message precedence.
 * 
 * @param superReceiver Whether the initial receiver is 'super'
 * @return Whether the final receiver is 'super'
 */
boolean keyContinuation(boolean superReceiver)
{
	int i, j, argumentCount;
	boolean sent, superTerm;
	object messagesym;
	char pattern[80];

	superReceiver = binaryContinuation(superReceiver);
	if (token == namecolon)
	{
		if (streq(tokenString, "ifTrue:"))
		{
			i = optimizeBlock(BranchIfFalse, false);
			if (streq(tokenString, "ifFalse:"))
			{
				codeArray[i] = codeTop + 3;
				ignore optimizeBlock(Branch, true);
			}
		}
		else if (streq(tokenString, "ifFalse:"))
		{
			i = optimizeBlock(BranchIfTrue, false);
			if (streq(tokenString, "ifTrue:"))
			{
				codeArray[i] = codeTop + 3;
				ignore optimizeBlock(Branch, true);
			}
		}
		else if (streq(tokenString, "whileTrue:"))
		{
			j = codeTop;
			genInstruction(DoSpecial, Duplicate);
			genMessage(false, 0, newSymbol("value"));
			i = optimizeBlock(BranchIfFalse, false);
			genInstruction(DoSpecial, PopTop);
			genInstruction(DoSpecial, Branch);
			genCode(j + 1);
			codeArray[i] = codeTop + 1;
			genInstruction(DoSpecial, PopTop);
		}
		else if (streq(tokenString, "and:"))
			ignore optimizeBlock(AndBranch, false);
		else if (streq(tokenString, "or:"))
			ignore optimizeBlock(OrBranch, false);
		else
		{
			pattern[0] = '\0';
			argumentCount = 0;
			while (parseok && (token == namecolon))
			{
				ignore strcat(pattern, tokenString);
				argumentCount++;
				ignore nextToken();
				superTerm = term();
				ignore binaryContinuation(superTerm);
			}
			sent = false;

			/* check for predefined messages */
			messagesym = newSymbol(pattern);

			if (!sent)
			{
				genMessage(superReceiver, argumentCount, messagesym);
			}
		}
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
 * Parses a block expression
 * 
 * This function handles block expressions in Smalltalk ([ ... ]).
 * It supports blocks with arguments ([:arg1 :arg2 | ...]) and
 * creates a proper Block object with bytecode for the block body.
 */
void block()
{
	int saveTemporary, argumentCount, fixLocation;
	object tempsym, newBlk;
	enum blockstatus savebstat;

	saveTemporary = temporaryTop;
	savebstat = blockstat;
	argumentCount = 0;
	ignore nextToken();
	if ((token == binary) && streq(tokenString, ":"))
	{
		while (parseok && (token == binary) && streq(tokenString, ":"))
		{
			if (nextToken() != nameconst)
				compilError(selector, "name must follow colon",
							"in block argument list");
			if (++temporaryTop > maxTemporary)
				maxTemporary = temporaryTop;
			argumentCount++;
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
			compilError(selector, "block argument list must be terminated",
						"by |");
		ignore nextToken();
	}
	newBlk = newBlock();
	basicAtPut(newBlk, argumentCountInBlock, newInteger(argumentCount));
	basicAtPut(newBlk, argumentLocationInBlock,
			   newInteger(saveTemporary + 1));
	genInstruction(PushLiteral, genLiteral(newBlk));
	genInstruction(PushConstant, contextConst);
	genInstruction(DoPrimitive, 2);
	genCode(29);
	genInstruction(DoSpecial, Branch);
	fixLocation = codeTop;
	genCode(0);
	/*genInstruction(DoSpecial, PopTop); */
	basicAtPut(newBlk, bytecountPositionInBlock, newInteger(codeTop + 1));
	blockstat = InBlock;
	body();
	// if ((token == closing) && streq(tokenString, "]"))
	if ((token == closing) && streq(tokenString, "]"))
		ignore nextToken();
	else 
		compilError(selector, "block not terminated by ]", "");
	genInstruction(DoSpecial, StackReturn);
	codeArray[fixLocation] = codeTop + 1;
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
 * Main entry point for the method parser
 * 
 * This function parses a complete Smalltalk method and fills in the
 * method object with the resulting bytecodes, literals, and metadata.
 * It coordinates the entire parsing process from selector to body.
 * 
 * @param method The Method object to populate
 * @param text The source code text of the method
 * @param savetext Whether to save the source text in the method object
 * @return true if parsing succeeded, false if there was an error
 */
boolean parse(object method, char *text, boolean savetext)
{
	int i;
	object bytecodes, theLiterals;
	byte *bp;

	lexinit(text);
	parseok = true;
	blockstat = NotInBlock;
	codeTop = 0;
	literalTop = temporaryTop = argumentTop = 0;
	maxTemporary = 0;

	messagePattern();
	if (parseok)
		temporaries();
	if (parseok)
		body();
	if (parseok)
	{
		genInstruction(DoSpecial, PopTop);
		genInstruction(DoSpecial, SelfReturn);
	}

	if (!parseok)
	{
		basicAtPut(method, bytecodesInMethod, nilobj);
	}
	else
	{
		bytecodes = newByteArray(codeTop);
		bp = bytePtr(bytecodes);
		for (i = 0; i < codeTop; i++)
		{
			bp[i] = codeArray[i];
		}
		basicAtPut(method, messageInMethod, newSymbol(selector));
		basicAtPut(method, bytecodesInMethod, bytecodes);
		if (literalTop > 0)
		{
			theLiterals = newArray(literalTop);
			for (i = 1; i <= literalTop; i++)
			{
				basicAtPut(theLiterals, i, literalArray[i]);
				decr(literalArray[i]);
			}
			basicAtPut(method, literalsInMethod, theLiterals);
		}
		else
		{
			basicAtPut(method, literalsInMethod, nilobj);
		}
		basicAtPut(method, stackSizeInMethod, newInteger(6));
		basicAtPut(method, temporarySizeInMethod,
				   newInteger(1 + maxTemporary));
		if (savetext)
		{
			basicAtPut(method, textInMethod, newStString(text));
		}
		return (true);
	}
	return (false);
}
