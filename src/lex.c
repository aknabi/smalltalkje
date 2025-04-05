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
*/

#include <stdio.h>
#include <ctype.h>
#include "env.h"
#include "memory.h"
#include "lex.h"

extern double atof();

/* Global variables returned by lexical analyzer */
tokentype token;      /* Current token type */
char tokenString[80]; /* String representation of the current token */
int tokenInteger;     /* Integer value (for integer and character tokens) */
double tokenFloat;    /* Floating point value (for float tokens) */

/* Local variables used only by lexical analyzer */
static char *cp;            /* Character pointer to current position in input */
static char pushBuffer[10]; /* Buffer for pushed-back characters (lookahead) */
static int pushindex;       /* Index of last pushed-back character */
static char cc;             /* Current character being processed */
static long longresult;     /* Accumulator for building integer values */

/**
 * Initialize the lexical analyzer
 * 
 * This function sets up the lexical analyzer to scan a new input string.
 * It resets the internal state, sets the input pointer to the beginning
 * of the provided string, and fetches the first token.
 * 
 * @param str The input string to scan
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
 * It updates the current character (cc) and returns it.
 * 
 * @return The next character from the input
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
 * @return Pointer to the remainder of the current line
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
 * @return The next character in the input stream
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
 * The function handles:
 * - Skipping whitespace and comments
 * - Recognizing identifiers and keywords
 * - Parsing integer and floating point numbers
 * - Processing character constants
 * - Handling symbols and arrays
 * - Processing string literals
 * - Identifying special characters and operators
 * 
 * @return The type of token found (as a tokentype enumeration value)
 */
tokentype nextToken()
{
	char *tp;
	boolean sign;

	/* skip over blanks and comments */
	while (nextChar() && (isspace(cc) || (cc == '"')))
		if (cc == '"')
		{
			/* read comment */
			while (nextChar() && (cc != '"'))
				;
			if (!cc)
				break; /* break if we run into eof */
		}

	tp = tokenString;
	*tp++ = cc;

	if (!cc) /* end of input */
		token = inputend;

	else if (isalpha(cc))
	{ /* identifier */
		while (nextChar() && isalnum(cc))
			*tp++ = cc;
		if (cc == ':')
		{
			*tp++ = cc;
			token = namecolon;
		}
		else
		{
			pushBack(cc);
			token = nameconst;
		}
	}

	else if (isdigit(cc))
	{ /* number */
		longresult = cc - '0';
		while (nextChar() && isdigit(cc))
		{
			*tp++ = cc;
			longresult = (longresult * 10) + (cc - '0');
		}
		if (longCanBeInt(longresult))
		{
			tokenInteger = (int) longresult;
			token = intconst;
		}
		else
		{
			token = floatconst;
			tokenFloat = (double)longresult;
		}
		if (cc == '.')
		{ /* possible float */
			if (nextChar() && isdigit(cc))
			{
				*tp++ = '.';
				do
					*tp++ = cc;
				while (nextChar() && isdigit(cc));
				if (cc)
					pushBack(cc);
				token = floatconst;
				*tp = '\0';
				tokenFloat = atof(tokenString);
			}
			else
			{
				/* nope, just an ordinary period */
				if (cc)
					pushBack(cc);
				pushBack('.');
			}
		}
		else
			pushBack(cc);

		if (nextChar() && cc == 'e')
		{ /* possible float */
			if (nextChar() && cc == '-')
			{
				sign = true;
				ignore nextChar();
			}
			else
				sign = false;
			if (cc && isdigit(cc))
			{ /* yep, its a float */
				*tp++ = 'e';
				if (sign)
					*tp++ = '-';
				while (cc && isdigit(cc))
				{
					*tp++ = cc;
					ignore nextChar();
				}
				if (cc)
					pushBack(cc);
				*tp = '\0';
				token = floatconst;
				tokenFloat = atof(tokenString);
			}
			else
			{ /* nope, wrong again */
				if (cc)
					pushBack(cc);
				if (sign)
					pushBack('-');
				pushBack('e');
			}
		}
		else if (cc)
			pushBack(cc);
	}

	else if (cc == '$')
	{ /* character constant */
		tokenInteger = (int)nextChar();
		token = charconst;
	}

	else if (cc == '#')
	{		  /* symbol */
		tp--; /* erase pound sign */
		if (nextChar() == '(')
			token = arraybegin;
		else
		{
			pushBack(cc);
			while (nextChar() && isSymbolChar(cc))
				*tp++ = cc;
			pushBack(cc);
			token = symconst;
		}
	}

	else if (cc == '\'')
	{		  /* string constant */
		tp--; /* erase pound sign */
	strloop:
		while (nextChar() && (cc != '\''))
			*tp++ = cc;
		/* check for nested quote marks */
		if (cc && nextChar() && (cc == '\''))
		{
			*tp++ = cc;
			goto strloop;
		}
		pushBack(cc);
		token = strconst;
	}

	else if (isClosing(cc)) /* closing expressions */
		token = closing;

	else if (singleBinary(cc))
	{ /* single binary expressions */
		token = binary;
	}

	else
	{ /* anything else is binary */
		if (nextChar() && binarySecond(cc))
			*tp++ = cc;
		else
			pushBack(cc);
		token = binary;
	}

	*tp = '\0';
	return (token);
}
