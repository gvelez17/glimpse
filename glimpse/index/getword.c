/* Copyright (c) 1994 Sun Wu, Udi Manber, Burra Gopal.  All Rights Reserved. */
/* ./glimpse/index/getword.c */
#include "glimpse.h"

extern FILE *MESSAGEFILE;
extern int NextICurrentFileOffset, ICurrentFileOffset;
int StructuredIndex = 0;
extern int RecordLevelIndex;
extern int StoreByteOffset;
extern int rdelim_len;
extern char rdelim[MAX_LINE_LEN];
int WORD_TOO_LONG = 0;
int IndexNumber = 0;
int CountWords = 0;
int InterpretSpecial = 0;
int indexable_char[256];
int GMAX_WORD_SIZE = MAX_WORD_SIZE;
int PrintedLongWordWarning = 0;

#define ALL_LOWER 0	/* default, what you start with: all are possible */
#define FIRST_UPPER 1	/* only first one seen is upper: 0 is impossible */
#define ALL_UPPER 2	/* all seen so far are upper: 2 and 3 are possible */
#define MIXED 3		/* neither of the above 3 */

#define ALPHANUM 1
#define ALPHAONLY 2
#define NUMONLY 3

/* -------------------------------------------------------------------------
getword():
get a word from stream pointed to by buffer.
a word is a string of alpha-numeric characters.
After the word is gotten, return a new pointer that points to a alpha-numeric
character. For the first call to such function when the first character
is not a alpha-numeric character, getword() only adjust the pointer to
point to a alpha-numeric character.
--------------------------------------------------------------------------*/
unsigned char *getword(filename, word, buffer, buffer_end, pattr, next_record)
unsigned char *filename;
unsigned char *word;
unsigned char *buffer;
unsigned char *buffer_end;
int *pattr;
unsigned char **next_record;
{
    int  word_length=0;
    unsigned char c, *wp=word;
    unsigned char *oldword=word;
    unsigned char *old_buffer = buffer;
    int	previslsq = 0;
    int withinsq = 0;
    static int times = 0;

    if (!RecordLevelIndex) ICurrentFileOffset = NextICurrentFileOffset;
    if (pattr != NULL) *pattr = 0;
    if (CountWords) {	/* don't convert case, ignore special, don't bother about offsets. */
	unsigned char *temp_buffer;
	int flag = ALL_LOWER;

	for(temp_buffer = buffer; (temp_buffer - buffer < GMAX_WORD_SIZE) && (temp_buffer < buffer_end); temp_buffer ++) {
	    if (!INDEXABLE(*temp_buffer)) break;
	    if (isupper(*temp_buffer)) {
		if (flag == ALL_LOWER) {
		    if (temp_buffer == buffer) flag = FIRST_UPPER;
		    else { flag = MIXED; break; }
		}
		else if (flag == FIRST_UPPER) {
		    if (temp_buffer == buffer + 1) flag = ALL_UPPER;
		    else { flag = MIXED; break; }
		}
		else continue;	/* must be ALL_UPPER -> let it remain so */
	    }
	    else if (islower(*temp_buffer)) {
		if (flag == ALL_LOWER) continue;
		else if (flag == FIRST_UPPER) continue;
		else if (flag == ALL_UPPER) { flag = MIXED; break; }
	    }
	    /* else, not alphabet: ignore */
	}

	if (flag == MIXED) {	/* discard mixed words since they cannot be indexed */
	    word[0] = '\0';
	    if (IndexNumber) while(isalnum(*temp_buffer++));
	    else while(isalpha(*temp_buffer++));
	    return temp_buffer;
	}

	while(buffer < buffer_end) {
	    if(INDEXABLE(*buffer)) {
	 	*word++ = *buffer ++;
		word_length++;
	    }
	    else  {
		while((buffer< buffer_end) && !(INDEXABLE(*buffer))) buffer++;
		break;
	    }
	    if(word_length > GMAX_WORD_SIZE) {
		word = wp;
		WORD_TOO_LONG = ON;
		while((buffer < buffer_end) && INDEXABLE(*buffer)) buffer++; /* skip current long word */
		break;
	    }	
	}
    }
    else {	/* convert case, maybe interpret special */
	while(buffer < buffer_end) {
	    if (INDEXABLE(*buffer) || withinsq) {	/* if (!RecordLevelIndex) ICurrentFileOffset is in the right place */
		if (*buffer == '[') {
			previslsq = 1;
			withinsq = 1;
		}
		else {
			previslsq = 0;
			if (*buffer == ']') withinsq = 0;
		}
		if ((*buffer == '-') && !withinsq) {	/* terminate word here */
			buffer ++;
			if (!RecordLevelIndex) ICurrentFileOffset ++;
			else if ((next_record != NULL) && (buffer >= *next_record)) {
				*next_record = forward_delimiter(buffer, buffer_end, rdelim, rdelim_len, 0);
				if (StoreByteOffset) ICurrentFileOffset += *next_record - buffer;
				else ICurrentFileOffset ++;
			}
			break;
		}
		if (isupper(*buffer)) *word++ = tolower(*buffer++);
		else *word++ = *buffer++;
		if (RecordLevelIndex && (next_record != NULL) && (buffer >= *next_record)) {
			*next_record = forward_delimiter(buffer, buffer_end, rdelim, rdelim_len, 0);
			if (StoreByteOffset) ICurrentFileOffset += *next_record - buffer;
			else ICurrentFileOffset ++;
		}
		word_length++;
	    }
	    else if (INDEXABLE('[') && (*buffer == '^') && previslsq) {
		*word ++ = *buffer ++;
		word_length ++;
		if (RecordLevelIndex && (next_record != NULL) && (buffer >= *next_record)) {
			*next_record = forward_delimiter(buffer, buffer_end, rdelim, rdelim_len, 0);
			if (StoreByteOffset) ICurrentFileOffset += *next_record - buffer;
			else ICurrentFileOffset ++;
		}
		previslsq = 0;
	    }
	    else {	/* !INDEXABLE character */
		previslsq = 0;
		if (InterpretSpecial && (*buffer == '\\')) {
		    /* skip two things AND terminate word HERE */
		    if (buffer < buffer_end - 1) {
			buffer += 2;
			if (word_length <= 0) {
			    if (RecordLevelIndex) {
				buffer -= 2;
			        buffer ++;
			        if ((next_record != NULL) && (buffer >= *next_record)) {
				    *next_record = forward_delimiter(buffer, buffer_end, rdelim, rdelim_len, 0);
				    if (StoreByteOffset) ICurrentFileOffset += *next_record - buffer;
				    else ICurrentFileOffset ++;
			        }
			        buffer ++;
			        if ((next_record != NULL) && (buffer >= *next_record)) {
				    *next_record = forward_delimiter(buffer, buffer_end, rdelim, rdelim_len, 0);
				    if (StoreByteOffset) ICurrentFileOffset += *next_record - buffer;
				    else ICurrentFileOffset ++;
			        }
			    }
			    else {
				ICurrentFileOffset += 2;
			    }
			}
		    }
		    else if (buffer < buffer_end) {
			buffer ++;
			if (word_length <= 0) {
			    if (RecordLevelIndex && (next_record != NULL) && (buffer >= *next_record)) {
			        *next_record = forward_delimiter(buffer, buffer_end, rdelim, rdelim_len, 0);
			        if (StoreByteOffset) ICurrentFileOffset += *next_record - buffer;
				else ICurrentFileOffset ++;
			    }
			    else {
				ICurrentFileOffset ++;
			    }
			}
		    }
		}
		else {
		    if (word_length <= 0) while((buffer < buffer_end) && !(INDEXABLE(*buffer))) {
			buffer++;
			if (!RecordLevelIndex) ICurrentFileOffset ++;
			else if ((next_record != NULL) && (buffer >= *next_record)) {
				*next_record = forward_delimiter(buffer, buffer_end, rdelim, rdelim_len, 0);
				if (StoreByteOffset) ICurrentFileOffset += *next_record - buffer;
				else ICurrentFileOffset ++;
			}
		    }
		    /* else, it is a !INDEXABLE character AFTER seeing a legit word: terminate right here and let next loop worry about it */
		    /* --> better offset computation; should if() be for ByteLevelIndex???? ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ */
		    else if (RecordLevelIndex) break;
		    else while((buffer < buffer_end) && !(INDEXABLE(*buffer))) {
			buffer++;
			if (RecordLevelIndex && (next_record != NULL) && (buffer >= *next_record)) {
				*next_record = forward_delimiter(buffer, buffer_end, rdelim, rdelim_len, 0);
				if (StoreByteOffset) ICurrentFileOffset += *next_record - buffer;
				else ICurrentFileOffset ++;
			}
		    }
		}
		break;
	    }

	    if(word_length > GMAX_WORD_SIZE) {
		word = wp;
		WORD_TOO_LONG = ON;
		while((buffer < buffer_end) && INDEXABLE(*buffer)) {
			buffer++; /* skip current long word */
			if (RecordLevelIndex && (next_record != NULL) && (buffer >= *next_record)) {
				*next_record = forward_delimiter(buffer, buffer_end, rdelim, rdelim_len, 0);
				if (StoreByteOffset) ICurrentFileOffset += *next_record - buffer;
				else ICurrentFileOffset ++;
			}
		}
		break;
	    }	
	}
	if (RecordLevelIndex && (next_record != NULL) && (buffer >= *next_record)) {
		*next_record = forward_delimiter(buffer, buffer_end, rdelim, rdelim_len, 0);
		if (StoreByteOffset) ICurrentFileOffset += *next_record - buffer;
		else ICurrentFileOffset ++;
	}
    }

    if(WORD_TOO_LONG) {
	c = wp[GMAX_WORD_SIZE];
	wp[GMAX_WORD_SIZE] = '\0';
	if (!PrintedLongWordWarning) {
		fprintf(MESSAGEFILE, "Warning: ignoring very long word '%s' (with > %d chars) in %s\n", oldword, GMAX_WORD_SIZE, filename);
		PrintedLongWordWarning = 1;
	}
	wp[GMAX_WORD_SIZE] = c;
	*wp = '\0';
    }
    *word = '\0';
    WORD_TOO_LONG = 0;
    if ((pattr != NULL) && (word_length > 0) && (StructuredIndex))
	*pattr = region_identify(ICurrentFileOffset, 0);
    if (!RecordLevelIndex) NextICurrentFileOffset += (buffer <= old_buffer) ? 1 : (buffer - old_buffer);	/* beginning of next word, atleast 1 */
#if	0
	if (!strcmp(wp, "to")) {
		printf("%d ", ICurrentFileOffset);
		if (++times > 20) {
			printf("\n");
			times = 0;
		}
	}
#endif
    return(buffer);
}

set_indexable_char(indexable_char)
	int	indexable_char[256];
{
	int	i;

	/* Saves a lot of calls during run-time! */
	for (i=0; i<256; i++) {
		if(!ISASCII((unsigned char)i) && !isalpha((unsigned char)i)) indexable_char[i] = 0;
		else if(IndexNumber) indexable_char[i] = isalnum(i);
		else indexable_char[i] = isalpha((unsigned char)i);
	}
	indexable_char['_'] = 1;
}

set_special_char(special_char)
	int	special_char[256];
{
	/*
	 * Set all special characters interpreted by agrep to 1.
	 * Assume set_indexable_char has been done on it.
	 */
	special_char['-'] = 1;
	/* special_char[','] = 1; */
	/* special_char[';'] = 1; */
	/* special_char['.'] = 1; */
	/* special_char['#'] = 1; */
	/* special_char['|'] = 1; */
	special_char['['] = 1;
	special_char[']'] = 1;
	/* special_char['('] = 1; */
	/* special_char[')'] = 1; */
	/* special_char['>'] = 1; */
	/* special_char['<'] = 1; */
	/* special_char['^'] = 1; */
	/* special_char['$'] = 1; */
	/* special_char['+'] = 1; */
}

