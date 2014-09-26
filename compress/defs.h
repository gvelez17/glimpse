/* Copyright (c) 1994 Burra Gopal, Udi Manber.  All Rights Reserved. */

/**************************************************************************
 * defs.h:	contains definitions for our static/dictionary based	  *
 *		compression scheme that is tailored for very fast search. *
 **************************************************************************/
#ifndef	_DEFS_H_
#define _DEFS_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include "glimpse.h"

#undef COMP_SUFFIX
#undef DEF_STRING_FILE
#undef DEF_HASH_FILE
#undef DEF_FREQ_FILE
#undef SIGNATURE_LEN

#define MIN_WORD_LEN		1		/* smaller words are not indexed: heuristics like special_texts etc. must be used: verbatim is good enough */
#define HASH_TABLE_SIZE		MAX_64K_HASH
#define SMALL_HASH_TABLE_SIZE	MAX_4K_HASH
#define HASH_ENTRY_SIZE		32 		/* hash-file stores: name of len=24, a 5 digit int, a ' ' + a '\n' = 31 bytes + some padding once in a while */
#define DEF_BLOCKSIZE		4096		/* I/O unit size = OS page size */
#define MIN_BLOCKSIZE		512		/* granularity for above and below */
#define HASH_FILE_BLOCKS	(HASH_TABLE_SIZE * HASH_ENTRY_SIZE / MIN_BLOCKSIZE)
#define STRING_FILE_BLOCKS	(HASH_TABLE_SIZE * MAX_WORD_LEN / MIN_BLOCKSIZE)
#define MAX_SPECIAL_CHARS	32		/* Maximum # of special characters used during compress */
#define DEF_SPECIAL_WORDS	32		/* Special words for which 1B codes are reserved */

#define COMP_ATLEAST		10		/* At least 10% compression is needed */
#define COMP_SUFFIX		".CZ"		/* Common suffix used for all compressed files: IT INCLUDES THE '.' !!! */
#define DEF_INDEX_FILE		INDEX_FILE	/* same as glimpse's */
#define DEF_STRING_FILE		".glimpse_uncompress"
#define DEF_HASH_FILE		".glimpse_compress"
#define DEF_FREQ_FILE		".glimpse_quick"
#define DEF_THRESHOLD		16		/* 256? default for min bytes to be coverd before storing in hash table */
#define MAX_THRESHOLD		65535		/* MAX_WORDS*MAX_THRESHOLD must be < 2**32 - 1 = maxoffset = maxdiskspace = integer */
#define MAX_LSB			254		/* 256 - |{'\0', '\n'}| */
#define DEF_MAX_WORDS		(MAX_LSB*MAX_LSB)

#define SAMPLE_SIZE		8192		/* amount of data read to determine file-type: NOT CALLED FOR STDIN! */
#define SIGNATURE_LEN		16		/* to avoid calling strlen: including \0! */

typedef struct _hash_entry {
	struct _hash_entry *next;
	char *word;				/* string itself */
	union {
		int	offset;			/* offset into the dictionary file: used only while building compress's dict from glimpse's dict */
		struct {
			short	freq;		/* number of times the word occurs -- provided it is in the dictionary */
			short	index;		/* index into the string table */
		} attribute;			/* once freq > THRESHOLD, its just an index into the string table: used only while compressing a file */
	} val;
} hash_entry;

/*
 * The total number of special characters (1..4) CANNOT exceed MAX_SPECIAL_CHARS.
 * The arrangement is as follows:
 * 1. SPECIAL_TEXTS
 * 2. SPECIAL_SEPARATORS
 * 3. SPECIAL_DELIMITERS
 * 4. VERBATIM
 * 5. SPECIAL_WORDS
 * Any rearrangement of these can be done provided the BEGIN/END values
 * are defined properly: the NUMs remain the same.
 */

#define BEGIN_SPECIAL_CHARS	1		/* character 0 is never a part of any code */
#define END_SPECIAL_CHARS	30		/* Not including begin/end verbatim */

/* Special delimiters are text-sequences which can come after a word instead of a blank: this is a subset of the above with '\n' and '\t' */
#define EASY_NUM_SPECIAL_DELIMITERS	8	/* numbered from 1 .. 8 */
#define HARD_NUM_SPECIAL_DELIMITERS	9	/* extra: a special kind of newline */
#define SPECIAL_DELIMITERS		{ '.', ',', ':', '-', ';', '!', '"', '\'', '\n'}
#define BEGIN_SPECIAL_DELIMITERS	BEGIN_SPECIAL_CHARS
#define EASY_END_SPECIAL_DELIMITERS	9
#define HARD_END_SPECIAL_DELIMITERS	10

/* Special separators are things that can separate two words: they are 2blanks, 2tabs or 2newlines */
#define NUM_SEPARATORS		7		/* numbered from 10 .. 16 */
#define NEWLINE			'\n' 		/* = HARD_END_SPECIAL_DELIMITERS --> carefully chosen so that this is TRUE !!!! Speeds up searches */
#define NOTBLANK		(NEWLINE + 1)	/* acts like unputc(' ') if char after a word != blk OR sp-delims */
#define BLANK			(NOTBLANK + 1)
#define TAB			(NOTBLANK + 2)
#define TWOBLANKS		(NOTBLANK + 3)	/* Beginning of a sentence */
#define TWOTABS			(NOTBLANK + 4)	/* Indentation */
#define TWONEWLINES		(NOTBLANK + 5)	/* Beginning of a paragraph */
#define BEGIN_SEPARATORS	10
#define END_SEPARATORS		17

/*
 * An alternate way would be to have a code for BLANK and NBLANKS, TAB and NTABS, and, NEWLINE and NNEWLINES:
 * in each of these cases, the byte occuring immediately next would determine the number of BLANKS/TABS/NEWLINES.
 * Though this works for a general number of cases, it needs two bytes of encoding: which makes us
 * wonder whether those cases occur commonly enough to waste two bytes to encode two blanks (common).
 * The present encoding guarantees 50% compression for any sequence of separators anyway, and is much simpler.
 */

/* Special texts are text-sequences which have a 1 byte codes associated with them: these appear first among the special things */
#define NUM_SPECIAL_TEXTS	13		/* numbered from 17 .. 29 */
#define SPECIAL_TEXTS		{ '.', ',', ':', '-', ';', '!', '"', '\'', '#', '$', '%', '(', ')'}	/* Could have used ?, @ and & too */
#define BEGIN_SPECIAL_TEXTS	17
#define END_SPECIAL_TEXTS	30

/* Characters for literal text */
#define BEGIN_VERBATIM		30
#define END_VERBATIM		31
#define EASY_ONE_VERBATIM	EASY_END_SPECIAL_DELIMITERS
#define HARD_ONE_VERBATIM	BEGIN_VERBATIM	/* Is not an ascii char since ascii is 32.. */

/* BEGIN and END SPECIAL_WORDS are variables */

#if	0
/* THIS WON'T REALLY HELP SINCE SOURCE CODE RARELY HAS COMMON WORDS: KEYWORDS ARE VERY SMALL SO THEY HARDLY GIVE ANY COMPRESSION */
char special_program_chars[] = { '.', ',', ':', '-', '!', ';', '?', '+', '/', '\'', '"', '~', '`', '&', '@', '#', '$', '%', '^', '*', '=', '(', ')', '{', '}', '[', ']', '_', '|', '\\', '<', '>' };
#endif	/*0*/

/*
 * Common exported functions.
 */

unsigned short encode_index();
unsigned short decode_index();
unsigned int mygetc();
int is_little_endian();
int build_string();
int build_hash();
int dump_hash();
int dump_string();
int get_word_from_offset();
int dump_and_free_string_hash();
hash_entry *insert_hash();
hash_entry *get_hash();
int hash_it();
char * tescapesinglequote();

/*
 * The beauty of this allocation scheme is that "free" does not need to be implemented!
 * The total memory occupied by both the string and hash tables is appx 1.5 MB
 */

#define hashfree(h)	if (usemalloc) free(e);

#define hashalloc(e) \
{\
	if (usemalloc) (e) = (hash_entry *)malloc(sizeof(hash_entry));\
	else {\
		if (free_hash == NULL) free_hash = (hash_entry *)malloc(sizeof(hash_entry) * DEF_MAX_WORDS);\
		if (free_hash == NULL) (e) = NULL;\
		else (e) = ((next_free_hash >= DEF_MAX_WORDS) ? (NULL) : (&(free_hash[next_free_hash ++])));\
	}\
	if ((e) == NULL) {fprintf(stderr, "Out of memory in cast-hash-table!\n"); exit(2); }\
}

#define strfree(s)	if (usemalloc) free(s);

/* called ONLY in the build procedure in which we can afford to be slow and do an strcpy since sizes of words are not determined: hardcoded in build_hash() */
#define stralloc(s, len) \
{\
	if (usemalloc) (s) = (char *)malloc(len);\
	else {\
		if (free_str == NULL) free_str = (char *)malloc(AVG_WORD_LEN * DEF_MAX_WORDS);\
		if (free_str == NULL) (s) = NULL;\
		else (s) = ((next_free_str >= AVG_WORD_LEN * DEF_MAX_WORDS) ? (NULL) : (&(free_str[next_free_str]))); next_free_str += (len);\
	}\
	if ((s) == NULL) {fprintf(stderr, "Out of memory in cast-string-table!\n"); exit(2); }\
}

/* There is no equivalent strtablealloc since it is hardcoded into build_string and is not used anywhere else */

/* Some flags corr. to user options: avoid global variables for options, pass flags as parameters */
#define TC_EASYSEARCH	0x1
#define TC_UNTILNEWLINE	0x2
#define TC_REMOVE	0x4
#define TC_OVERWRITE	0x8
#define TC_RECURSIVE	0x10
#define TC_ERRORMSGS	0x20
#define TC_SILENT	0x40
#define TC_NOPROMPT	0x80
#define TC_FILENAMESONSTDIN 0x100

#define CAST_VERSION	"1.0"
#define CAST_DATE	"1994"

#endif	/*_DEFS_H_*/
