/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021
	Based on Little Smalltalk, version 2
	Written by Tim Budd, Oregon State University, July 1987

	Name Table Module
	
	This module implements the symbol table and name lookup functionality
	for the Smalltalk environment. Key responsibilities include:
	
	1. Dictionary/Symbol Management:
	   - A name table is a Dictionary indexed by symbols
	   - Two primary name tables are used by the interpreter:
	     a) globalNames: Contains globally accessible identifiers
	     b) Method tables: Associated with each class
	
	2. Symbol Uniqueness:
	   - All Symbol instances must be unique for two reasons:
	     a) To ensure that equality testing with == works as expected
	     b) To prevent memory bloat with duplicate symbols
	   - Symbols are stored in a hash table to enforce uniqueness
	
	3. Common Symbol Management:
	   - Defines and maintains frequently used symbols (nil, true, false, etc.)
	   - These common symbols are cached for efficiency
	
	4. Name Lookup:
	   - Provides functions to look up names in the symbol tables
	   - Both global lookups and class method lookups are supported
	
	Note: This module primarily handles reading FROM tables; writing to 
	tables is handled elsewhere in the system.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "env.h"
#include "memory.h"
#include "names.h"

/**
 * Inserts a key-value pair into a name table (Dictionary)
 * 
 * This function adds or updates an entry in a Dictionary. The function:
 * 1. First tries to directly insert at the hash location if empty or matching key
 * 2. If collision occurs, creates a linked list of associations (implemented as Links)
 * 3. Either adds a new link at the end of the chain or updates an existing one
 * 
 * The hash value is multiplied by 3 because each hash bucket in the table consists of 
 * three consecutive slots: key, value, and link to collision chain.
 * 
 * @param dict - The dictionary object to insert into
 * @param hash - Pre-computed hash value for the key
 * @param key - The key (usually a Symbol) to insert
 * @param value - The value to associate with the key
 */
noreturn nameTableInsert(object dict, int hash, object key, object value)
{
	object table, link, nwLink, nextLink, tablentry;

	/* First get the hash table */
	table = basicAt(dict, 1);

	if (sizeField(table) < 3)
		sysError("attempt to insert into", "too small name table");
	else
	{
		hash = 3 * (hash % (sizeField(table) / 3));
		tablentry = basicAt(table, hash + 1);
		if ((tablentry == nilobj) || (tablentry == key))
		{
			basicAtPut(table, hash + 1, key);
			basicAtPut(table, hash + 2, value);
		}
		else
		{
			nwLink = newLink(key, value);
			incr(nwLink);
			link = basicAt(table, hash + 3);
			if (link == nilobj)
			{
				basicAtPut(table, hash + 3, nwLink);
			}
			else
				while (1)
					if (basicAt(link, 1) == key)
					{
						basicAtPut(link, 2, value);
						break;
					}
					else if ((nextLink = basicAt(link, 3)) == nilobj)
					{
						basicAtPut(link, 3, nwLink);
						break;
					}
					else
						link = nextLink;
			decr(nwLink);
		}
	}
}

/**
 * Searches a dictionary for an element satisfying a predicate function
 * 
 * This function iterates through a dictionary (hash table) to find an element
 * that matches a given predicate function. It:
 * 1. Calculates the hash bucket location
 * 2. Checks the direct bucket entry first
 * 3. If not found, traverses the linked list of associations at that bucket
 * 4. Returns the value associated with the first matching key
 * 
 * The dictionary structure in Smalltalkje consists of a table where each
 * hash bucket contains three slots:
 * - Key (at hash)
 * - Value (at hash+1)
 * - Link to collision chain (at hash+2) - contains more Link objects for collisions
 * 
 * @param dict - The dictionary object to search
 * @param hash - Pre-computed hash value for key lookup
 * @param fun - Function pointer to predicate that tests if a key matches
 * @return The value associated with the matching key, or nilobj if not found
 */
object hashEachElement(object dict, register int hash, int (*fun)(object))
{
	object table, key, value, link;
	register object *hp;
	int tablesize;

	table = basicAt(dict, 1);

	/* Now see if table is valid */
	if ((tablesize = sizeField(table)) < 3)
		sysError("system error", "lookup on null table");
	else
	{
		hash = 1 + (3 * (hash % (tablesize / 3)));
		hp = sysMemPtr(table) + (hash - 1);
		key = *hp++;   /* table at: hash */
		value = *hp++; /* table at: hash + 1 */
		if ((key != nilobj) && (*fun)(key))
			return value;
		for (link = *hp; link != nilobj; link = *hp)
		{
			hp = sysMemPtr(link);
			key = *hp++;   /* link at: 1 */
			value = *hp++; /* link at: 2 */
			if ((key != nilobj) && (*fun)(key))
				return value;
		}
	}
	return nilobj;
}

/**
 * Computes a hash value for a string
 * 
 * This function generates a hash by summing character values and then
 * ensuring the result fits within Smalltalk integer constraints.
 * The hash is:
 * 1. Made positive (absolute value)
 * 2. Reduced if larger than 16384 to ensure it can be represented
 *    as a valid Smalltalk integer (which has size limitations)
 * 
 * While this is a simple hash function, it's efficient for the typical
 * short string identifiers used in Smalltalk names.
 * 
 * @param str - The C string to hash
 * @return An integer hash value suitable for Smalltalk use
 */
int strHash(char *str)
{
	register int hash;
	register char *p;

	hash = 0;
	for (p = str; *p; p++)
		hash += *p;
	if (hash < 0)
		hash = -hash;
	/* make sure it can be a smalltalk integer */
	if (hash > 16384)
		hash >>= 2;
	return hash;
}

static object objBuffer;   /* Buffer to store matched object for caller */
static char *charBuffer;   /* Buffer to store string to compare against */

/**
 * Tests if an object's character data matches a target string
 * 
 * This predicate function is used with hashEachElement to find objects
 * (typically symbols) that match a specific string. When a match is found,
 * it saves the matching object in objBuffer for later retrieval.
 * 
 * The function works by:
 * 1. Checking if the object has character data (using charPtr)
 * 2. Comparing the character data to the target string in charBuffer
 * 3. If a match is found, saving the object in objBuffer and returning 1
 * 
 * @param key - The object to test (typically a Symbol)
 * @return 1 if the object's character data matches charBuffer, 0 otherwise
 */
static int strTest(object key)
{
	if (charPtr(key) && streq(charPtr(key), charBuffer))
	{
		objBuffer = key;
		return 1;
	}
	return 0;
}

/**
 * Finds a global symbol key by its string representation
 * 
 * This function searches the global symbols table for a symbol
 * matching the provided string. Used primarily to locate existing symbols
 * without creating new ones.
 * 
 * Symbol uniqueness is maintained by only creating new symbols when
 * a matching one doesn't already exist. This function helps implement
 * that by finding existing symbols.
 * 
 * @param str - The C string to search for
 * @return The symbol object with matching string, or nilobj if not found
 */
object globalKey(char *str)
{
	objBuffer = nilobj;
	charBuffer = str;
	ignore hashEachElement(symbols, strHash(str), strTest);
	return objBuffer;
}

/**
 * Looks up a value in a dictionary by string key
 * 
 * This function searches a dictionary for an entry whose key (typically a Symbol)
 * matches the provided string. Used for name lookups in symbol tables.
 * 
 * The function is commonly used to:
 * 1. Look up method names in a class's method dictionary
 * 2. Look up global variables in the global names dictionary
 * 3. Look up instance variables in various contexts
 * 
 * @param dict - Dictionary to search in
 * @param str - The string key to look up
 * @return The value associated with the matching key, or nilobj if not found
 */
object nameTableLookup(object dict, char *str)
{
	charBuffer = str;
	return hashEachElement(dict, strHash(str), strTest);
}

/* Arrays of cached symbol objects for performance optimization */
object unSyms[12];  /* Unary message symbols (e.g., isNil, notNil) */
object binSyms[30]; /* Binary message symbols (e.g., +, -, <, >) */

/* String representations of unary messages that will be cached as symbols */
char *unStrs[] = {"isNil", "notNil", "value", "new", "class", "size",
				  "basicSize", "print", "printString", 0};

/* String representations of binary messages that will be cached as symbols */
char *binStrs[] = {"+", "-", "<", ">", "<=", ">=", "=", "~=", "*",
				   "quo:", "rem:", "bitAnd:", "bitXor:", "==",
				   ",", "at:", "basicAt:", "do:", "coerce:", "error:", "includesKey:",
				   "isMemberOf:", "new:", "to:", "value:", "whileTrue:", "addFirst:",
				   "addLast:",
				   0};

/**
 * Initializes common symbols used by the bytecode interpreter
 * 
 * This function caches frequently used symbols to improve performance
 * during bytecode interpretation. It:
 * 1. Sets up 'true' and 'false' global objects
 * 2. Initializes arrays of unary message symbols (e.g., isNil, notNil)
 * 3. Initializes arrays of binary message symbols (e.g., +, -, <, >)
 * 4. Ensures the "Block" symbol exists
 * 
 * These cached symbols avoid redundant symbol lookups during execution,
 * significantly improving the performance of common operations by providing
 * direct access to these frequently used symbols.
 */
noreturn initCommonSymbols()
{
	int i;

	trueobj = globalSymbol("true");
	falseobj = globalSymbol("false");
	for (i = 0; unStrs[i]; i++)
		unSyms[i] = newSymbol(unStrs[i]);
	for (i = 0; binStrs[i]; i++)
		binSyms[i] = newSymbol(binStrs[i]);
	ignore newSymbol("Block");
}
