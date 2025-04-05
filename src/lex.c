/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021
	Based on Little Smalltalk, version 2
	Written by Tim Budd, Oregon State University, July 1987

	Lexical Analyzer Implementation
	
	This module implements the lexical analyzer (scanner) for the Smalltalk
	parser. It's responsible for breaking source code text into tokens that
	can be processed by the parser. The lexical analyzer handles:
	
	1. Token Recognition:
	   - Identifiers (variable and message names)
	   - Numbers (integers and floating point)
	   - Characters (e.g., $a)
	   - Symbols (e.g., #symbol)
	   - Strings (e.g., 'Hello')
	   - Special characters and operators
	
	2. Tokenization Features:
	   - Skipping whitespace and comments
	   - Handling of special token types like keyword selectors (name:)
	   - Proper token classification based on Smalltalk syntax rules
	   - Character pushback for lookahead parsing
	
	This module is designed to be called only by the parser and not used
	directly by other parts of the system.
	
	The lexer implements a state machine pattern where characters are consumed
	one at a time and processed according to Smalltalk syntax rules. The main
	entry point is nextToken(), which handles all token types and updates global
	state variables with information about the recognized token.
*/

#include <stdio.h>
#include <ctype.h>
#include "env.h"
#include "memory.h"
#include "lex.h"

extern double atof();

/* Global variables returned by lexical analyzer */
tokentype token;      /* Current token type (nameconst, intconst, etc.) */
char tokenString[80]; /* String representation of the current token */
int tokenInteger;     /* Integer value (for integer and character tokens) */
double tokenFloat;    /* Floating point value (for float tokens) */

/* Local variables used only by lexical analyzer */
static char *cp;            /* Character pointer to current position in input string */
static char pushBuffer[10]; /* Buffer for pushed-back characters (for lookahead parsing) */
static int pushindex;       /* Index of last pushed-back character in pushBuffer */
static char cc;             /* Current character being processed by the lexer */
static long longresult;     /* Accumulator for building integer values during number parsing */

/**
 * Initialize the lexical analyzer
 * 
 * This function sets up the lexical analyzer to scan a new input string.
 * It resets the internal state, sets the input pointer to the beginning
 * of the provided string, and fetches the first token.
 * 
 * The analyzer maintains its state between calls, so lexinit() must be called
 * each time a new string needs to be processed.
 * 
 * @param str The input string to scan (null-terminated)
 */
noreturn lexinit(char *str)
{
	pushindex = 0;
	cp = str;
	/* get first token */
	ignore nextToken();
}

/**
 * Push a character back into the input stream
 * 
 * This function pushes a character back into the input, allowing for
 * lookahead in the parsing process. Characters are pushed onto a small
 * buffer and will be the next ones retrieved by nextChar().
 * 
 * This is a critical function for the lexer as it supports the one-character
 * lookahead needed for proper token recognition. For example, when processing
 * numbers, the lexer needs to look ahead to see if a period follows a digit
 * (indicating a floating point number).
 * 
 * @param c The character to push back
 */
static void pushBack(c) char c;
{
	pushBuffer[pushindex++] = c;
}

/**
 * Get the next character from the input
 * 
 * This function retrieves the next character either from the pushback
 * buffer (if characters have been pushed back) or from the input string.
 * It updates the current character (cc) with the retrieved character
 * and returns it.
 * 
 * The function prioritizes characters from the pushback buffer over
 * characters from the input string, implementing the lookahead mechanism.
 * 
 * @return The next character from the input, or '\0' if end of input is reached
 */
static char nextChar()
{
	if (pushindex > 0)
		cc = pushBuffer[--pushindex];
	else if (*cp)
		cc = *cp++;
	else
		cc = '\0';
	return (cc);
}

/**
 * Get the remaining text on the current line
 * 
 * This function returns a pointer to the rest of the current line
 * from the current position. It's useful for processing comments
 * or for error reporting.
 * 
 * Note: This does not advance the lexer's position - it merely provides
 * access to the current position in the input string.
 * 
 * @return Pointer to the remainder of the current line (the current position in cp)
 */
char* toEndOfLine()
{
	return cp;
}

/**
 * Look at the next character without consuming it
 * 
 * This function performs a lookahead operation, retrieving the next
 * character but then pushing it back so it can be read again by
 * the next call to nextChar().
 * 
 * This is a convenience function built on top of nextChar() and pushBack()
 * that allows the lexer to peek at the next character without advancing
 * the input position.
 * 
 * @return The next character in the input stream, without consuming it
 */
char peek()
{
	pushBack(nextChar());
	return (cc);
}

/**
 * Check if a character is a closing delimiter
 * 
 * This function determines if a character is one that can close an
 * expression, such as a period, right bracket, etc. These characters
 * have special meaning in the Smalltalk syntax.
 * 
 * Closing delimiters in Smalltalk include:
 * - '.' (period): Ends a statement
 * - ']' (right bracket): Closes a block expression
 * - ')' (right parenthesis): Closes a grouped expression
 * - ';' (semicolon): Separates statements in a cascade
 * - '"' (double quote): Closes a comment
 * - '\'' (single quote): Closes a string literal
 * 
 * @param c The character to check
 * @return true if the character is a closing delimiter, false otherwise
 */
static boolean isClosing(c) char c;
{
	switch (c)
	{
	case '.':
	case ']':
	case ')':
	case ';':
	case '\"':
	case '\'':
		return (true);
	}
	return (false);
}

/**
 * Check if a character can be part of a symbol
 * 
 * This function determines if a character is valid within a symbol name.
 * Symbols can contain alphanumeric characters and certain special characters,
 * but not whitespace or closing delimiters.
 * 
 * In Smalltalk, symbols (prefixed with #) can include:
 * - Alphanumeric characters (a-z, A-Z, 0-9)
 * - Special characters that are not whitespace or closing delimiters
 * 
 * For example, #mySymbol, #+, #at:put: are all valid symbols.
 * 
 * @param c The character to check
 * @return true if the character can be part of a symbol, false otherwise
 */
static boolean isSymbolChar(c) char c;
{
	if (isdigit(c) || isalpha(c))
		return (true);
	if (isspace(c) || isClosing(c))
		return (false);
	return (true);
}

/**
 * Check if a character is a single-character binary operator
 * 
 * This function identifies characters that are always treated as
 * single-character binary operators and cannot be combined with
 * other characters to form multi-character operators.
 * 
 * Single binary characters in Smalltalk include:
 * - '[' (left bracket): Begins a block expression
 * - '(' (left parenthesis): Begins a grouped expression
 * - ')' (right parenthesis): Ends a grouped expression
 * - ']' (right bracket): Ends a block expression
 * 
 * These characters always stand alone and are never part of a multi-character
 * binary selector.
 * 
 * @param c The character to check
 * @return true if the character is a single-character binary operator
 */
static boolean singleBinary(c) char c;
{
	switch (c)
	{
	case '[':
	case '(':
	case ')':
	case ']':
		return (true);
	}
	return (false);
}

/**
 * Check if a character can be the second character in a binary operator
 * 
 * This function determines if a character can appear as the second
 * character in a multi-character binary operator. Certain characters
 * like alphanumerics, spaces, or single binary operators cannot be
 * the second character in a binary sequence.
 * 
 * In Smalltalk, binary selectors can be composed of one or more
 * non-alphanumeric characters (except for special cases like brackets).
 * Examples of valid binary selectors include +, -, *, /, //, >>, <<, etc.
 * 
 * This function helps distinguish between single-character binary operators
 * and the start of multi-character binary operators.
 * 
 * @param c The character to check
 * @return true if the character can be the second in a binary operator
 */
static boolean binarySecond(c) char c;
{
	if (isalpha(c) || isdigit(c) || isspace(c) || isClosing(c) ||
		singleBinary(c))
		return (false);
	return (true);
}

/**
 * Retrieve the next token from the input
 * 
 * This is the main function of the lexical analyzer. It scans the input
 * and identifies the next token according to Smalltalk syntax rules.
 * The function updates the global token-related variables (token,
 * tokenString, tokenInteger, tokenFloat) with information about the
 * token found.
 * 
 * The function implements a complex state machine that handles:
 * - Skipping whitespace and comments (enclosed in double quotes)
 * - Recognizing identifiers and keywords (messages with colons)
 * - Parsing integer and floating point numbers (including scientific notation)
 * - Processing character constants (prefixed with $)
 * - Handling symbols (prefixed with #) and literal arrays (#(...))
 * - Processing string literals (enclosed in single quotes)
 * - Identifying special characters and operators
 * 
 * The algorithm works as follows:
 * 1. Skip all whitespace and comments
 * 2. Read the next character and begin building a token
 * 3. Based on the character type, branch into specialized parsing:
 *    - For letters: Parse identifiers or keywords
 *    - For digits: Parse numbers (checking for decimals and exponents)
 *    - For $: Parse character constants
 *    - For #: Parse symbols or arrays
 *    - For ': Parse string literals
 *    - For closing delimiters: Return as closing tokens
 *    - For other characters: Parse as binary operators
 * 
 * @return The type of token found (as a tokentype enumeration value)
 */
/**
 * Skip over whitespace and comments in the input
 * 
 * This helper function advances the character pointer past any
 * whitespace and comments in the input stream.
 */
static void skipWhitespaceAndComments()
{
	while (nextChar() && (isspace(cc) || (cc == '"'))) {
		if (cc == '"') {
			/* Process comment: read until closing double quote */
			while (nextChar() && (cc != '"'))
				; /* Skip all characters inside comment */
			
			if (!cc)
				break; /* Break the loop if we hit end of input during comment */
		}
	}
}

/**
 * Process identifier tokens (variable names, message names)
 * 
 * This helper function handles the parsing of identifiers which
 * can be either variable names or message names (with or without colons).
 * 
 * @param tp Pointer to the current position in tokenString
 * @return Updated pointer to the current position in tokenString
 */
static char* processIdentifier(char *tp)
{
	/* Continue reading alphanumeric characters */
	while (nextChar() && isalnum(cc))
		*tp++ = cc;
	
	if (cc == ':') {
		/* This is a keyword message part (e.g., "at:" in "at:put:") */
		*tp++ = cc;
		token = namecolon;
	}
	else {
		/* This is a regular identifier (variable or message without colon) */
		pushBack(cc);
		token = nameconst;
	}
	
	return tp;
}

/**
 * Process scientific notation in number tokens
 * 
 * This helper function handles the parsing of scientific notation
 * in floating point numbers (e.g., 1.23e-4).
 * 
 * @param tp Pointer to the current position in tokenString
 * @param sign Pointer to a boolean indicating the sign of the exponent
 * @return Updated pointer to the current position in tokenString
 */
static char* processScientificNotation(char *tp, boolean *sign);

/**
 * Process number tokens (integer or floating point)
 * 
 * This helper function handles the parsing of numeric literals,
 * which can be either integers or floating point numbers.
 * 
 * @param tp Pointer to the current position in tokenString
 * @return Updated pointer to the current position in tokenString
 */
static char* processNumber(char *tp)
{
	boolean sign;
	
	/* Build the integer part of the number */
	longresult = cc - '0';
	while (nextChar() && isdigit(cc)) {
		*tp++ = cc;
		longresult = (longresult * 10) + (cc - '0');
	}
	
	/* Check if the number fits in an integer */
	if (longCanBeInt(longresult)) {
		tokenInteger = (int) longresult;
		token = intconst;
	}
	else {
		/* Number is too large for int, treat as float */
		token = floatconst;
		tokenFloat = (double)longresult;
	}
	
	/* Check for decimal point to identify floating point number */
	if (cc == '.') { 
		/* Look ahead to see if this is a decimal point in a float */
		if (nextChar() && isdigit(cc)) {
			/* This is a floating point number with decimal part */
			*tp++ = '.';
			
			/* Read all digits after decimal point */
			do {
				*tp++ = cc;
			} while (nextChar() && isdigit(cc));
			
			if (cc)
				pushBack(cc);
				
			/* Convert the entire string to a floating point number */
			token = floatconst;
			*tp = '\0';
			tokenFloat = atof(tokenString);
		}
		else {
			/* Not a decimal point - it's just a period following a number */
			/* Push back the character after the period */
			if (cc)
				pushBack(cc);
			
			/* Push back the period itself */
			pushBack('.');
		}
	}
	else {
		/* No decimal point, push back the non-digit character */
		pushBack(cc);
	}

	/* Check for scientific notation (e.g., 1.23e-4) */
	if (nextChar() && cc == 'e') { 
		tp = processScientificNotation(tp, &sign);
	}
	else if (cc) {
		pushBack(cc);
	}
	
	return tp;
}

/**
 * Process scientific notation in number tokens
 * 
 * This helper function handles the parsing of scientific notation
 * in floating point numbers (e.g., 1.23e-4).
 * 
 * @param tp Pointer to the current position in tokenString
 * @param sign Pointer to a boolean indicating the sign of the exponent
 * @return Updated pointer to the current position in tokenString
 */
static char* processScientificNotation(char *tp, boolean *sign)
{
	/* Look ahead for sign or digit after 'e' */
	if (nextChar() && cc == '-') {
		/* Handle negative exponent */
		*sign = true;
		ignore nextChar();
	}
	else {
		/* Handle positive exponent */
		*sign = false;
	}
	
	if (cc && isdigit(cc)) { 
		/* Valid scientific notation found */
		*tp++ = 'e';
		if (*sign)
			*tp++ = '-';
			
		/* Read all digits in the exponent */
		while (cc && isdigit(cc)) {
			*tp++ = cc;
			ignore nextChar();
		}
		
		if (cc)
			pushBack(cc);
			
		/* Convert the entire string with exponent to a float */
		*tp = '\0';
		token = floatconst;
		tokenFloat = atof(tokenString);
	}
	else { 
		/* Not scientific notation - pushback all characters */
		if (cc)
			pushBack(cc);
		if (*sign)
			pushBack('-');
		pushBack('e');
	}
	
	return tp;
}

/**
 * Process symbol tokens (#symbol) or literal arrays (#(...))
 * 
 * This helper function handles the parsing of symbols and literal arrays,
 * which are both prefixed with the # character.
 * 
 * @param tp Pointer to the current position in tokenString
 * @return Updated pointer to the current position in tokenString
 */
static char* processSymbolOrArray(char *tp)
{
	tp--; /* Erase pound sign from token string - we just want the symbol content */
	
	if (nextChar() == '(') {
		/* This is a literal array (#(...)) */
		token = arraybegin;
	}
	else {
		/* This is a symbol (#symbol) */
		pushBack(cc);
		
		/* Read all valid symbol characters */
		while (nextChar() && isSymbolChar(cc))
			*tp++ = cc;
			
		pushBack(cc);
		token = symconst;
	}
	
	return tp;
}

/**
 * Process string tokens ('hello')
 * 
 * This helper function handles the parsing of string literals,
 * which are enclosed in single quotes.
 * 
 * @param tp Pointer to the current position in tokenString
 * @return Updated pointer to the current position in tokenString
 */
static char* processString(char *tp)
{
	tp--; /* Erase opening quote from token string */
	
processStringLoop:
	/* Read characters until closing quote */
	while (nextChar() && (cc != '\''))
		*tp++ = cc;
		
	/* Handle escaped quotes within the string ('don''t' has one escaped quote) */
	if (cc && nextChar() && (cc == '\'')) {
		/* This is an escaped quote (two consecutive quotes) */
		*tp++ = cc;
		goto processStringLoop; /* Continue reading the string */
	}
	
	pushBack(cc);
	token = strconst;
	
	return tp;
}

/**
 * Retrieve the next token from the input
 * 
 * This is the main function of the lexical analyzer. It scans the input
 * and identifies the next token according to Smalltalk syntax rules.
 * The function updates the global token-related variables (token,
 * tokenString, tokenInteger, tokenFloat) with information about the
 * token found.
 * 
 * The function implements a complex state machine that handles:
 * - Skipping whitespace and comments (enclosed in double quotes)
 * - Recognizing identifiers and keywords (messages with colons)
 * - Parsing integer and floating point numbers (including scientific notation)
 * - Processing character constants (prefixed with $)
 * - Handling symbols (prefixed with #) and literal arrays (#(...))
 * - Processing string literals (enclosed in single quotes)
 * - Identifying special characters and operators
 * 
 * The algorithm works as follows:
 * 1. Skip all whitespace and comments
 * 2. Read the next character and begin building a token
 * 3. Based on the character type, branch into specialized parsing:
 *    - For letters: Parse identifiers or keywords
 *    - For digits: Parse numbers (checking for decimals and exponents)
 *    - For $: Parse character constants
 *    - For #: Parse symbols or arrays
 *    - For ': Parse string literals
 *    - For closing delimiters: Return as closing tokens
 *    - For other characters: Parse as binary operators
 * 
 * @return The type of token found (as a tokentype enumeration value)
 */
tokentype nextToken()
{
	char *tp;
	boolean sign;

	/* Skip over whitespace and comments */
	skipWhitespaceAndComments();

	/* Start building the token string */
	tp = tokenString;
	*tp++ = cc;

	if (!cc) { 
		/* We've reached the end of input */
		token = inputend;
	}
	else if (isalpha(cc)) { 
		/* Process identifier: variable name or message name */
		tp = processIdentifier(tp);
	}
	else if (isdigit(cc)) { 
		/* Process number: integer or floating point */
		tp = processNumber(tp);
	}
	else if (cc == '$') { 
		/* Process character constant ($a, $1, $+, etc.) */
		/* In Smalltalk, $ followed by any character represents that character's ASCII value */
		tokenInteger = (int)nextChar(); /* Store the character's ASCII value */
		token = charconst;
	}
	else if (cc == '#') { 
		/* Process symbol (#symbol) or literal array (#(...)) */
		tp = processSymbolOrArray(tp);
	}
	else if (cc == '\'') { 
		/* Process string constant ('hello') */
		tp = processString(tp);
	}
	else if (isClosing(cc)) {
		/* Process closing delimiters (., ], ), ;, etc.) */
		token = closing;
	}
	else if (singleBinary(cc)) {
		/* Process single character binary operators ([, (, etc.) */
		token = binary;
	}
	else {
		/* Process multi-character binary operators (+, -, *, /, etc.) */
		/* Try to read a second character if it can be part of a binary operator */
		if (nextChar() && binarySecond(cc))
			*tp++ = cc;
		else
			pushBack(cc);
			
		token = binary;
	}

	/* Null-terminate the token string and return the token type */
	*tp = '\0';
	return (token);
}
