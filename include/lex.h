/*
	Smalltalkje, version 1 based on:

	Little Smalltalk, version 2
	Written by Tim Budd, Oregon State University, July 1987

	Lexical Analyzer Header
	
	This header defines the interface for the lexical analyzer component of
	the Smalltalk parser. The lexical analyzer (or lexer) is responsible for
	breaking input text into tokens - the basic syntactic units of the Smalltalk
	language like identifiers, numbers, symbols, etc.
	
	The lexer reads characters from the input stream and groups them into
	tokens, providing these tokens to the parser for syntactic analysis.
	This process is the first phase in compiling Smalltalk code.
*/

/**
 * Token Types
 * 
 * This enumeration defines all the possible token types that can be
 * returned by the lexical analyzer. Each type represents a distinct
 * syntactic element in the Smalltalk language.
 * 
 * The lexer classifies each sequence of characters it reads into one
 * of these token types, which the parser then uses to understand the
 * structure of the program.
 */
typedef enum tokensyms
{
	nothing,     /* No token recognized or initialization state */
	nameconst,   /* Identifier (variable/message name without colon) */
	namecolon,   /* Identifier followed by a colon (keyword message part) */
	intconst,    /* Integer literal (e.g., 42) */
	floatconst,  /* Floating-point literal (e.g., 3.14) */
	charconst,   /* Character literal (e.g., $a) */
	symconst,    /* Symbol literal (e.g., #symbol) */
	arraybegin,  /* Beginning of a literal array (e.g., #( ) */
	strconst,    /* String literal (e.g., 'hello') */
	binary,      /* Binary message selector (e.g., +, -, *, /) */
	closing,     /* Closing delimiter (e.g., ), }, ]) */
	inputend     /* End of input marker */
} tokentype;

/**
 * Get the next token from the input stream
 * 
 * This function reads characters from the input stream and returns
 * the next token found. It updates the global token-related variables
 * (token, tokenString, etc.) with information about the token.
 * 
 * @return The type of the token that was found
 */
extern tokentype nextToken(NOARGS);

/**
 * Global Token Information
 * 
 * These variables hold information about the most recently recognized token.
 * They are updated by the nextToken() function each time it's called and
 * provide access to details about the current token being processed.
 */
extern tokentype token;     /* The type of the current token */
extern char tokenString[];  /* The string representation of the current token */
extern int tokenInteger;    /* The integer value if the token is an integer or character constant */
extern double tokenFloat;   /* The floating-point value if the token is a float constant */

/**
 * Initialize the lexical analyzer
 * 
 * This function prepares the lexical analyzer for use by setting up
 * internal state, initializing buffers, and preparing to read from
 * the input stream. It must be called before the first call to nextToken().
 */
extern noreturn lexinit();

/**
 * Read all characters to the end of the current line
 * 
 * This function consumes and returns all remaining characters on the
 * current line of input. It's useful for skipping over comments or
 * handling special line-oriented syntax.
 * 
 * @return A pointer to a string containing the rest of the line
 */
extern char* toEndOfLine();
