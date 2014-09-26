/* Copyright (c) 1994 Burra Gopal, Udi Manber.  All Rights Reserved. */

/*
 * misc.c:	Miscellaneous routines used everywhere.
 */

#include "defs.h"

#define MYEOF	0xffffffff

/*
 * These are the global variables common to both compress/uncompress/csearch.c
 */

int RESERVED_CHARS = 0;
int MAX_WORDS = 0;
int SPECIAL_WORDS = 0;
int BEGIN_SPECIAL_WORDS = 0;
int END_SPECIAL_WORDS = 0;
int NUM_SPECIAL_DELIMITERS = 0;
int END_SPECIAL_DELIMITERS = 0;
int ONE_VERBATIM = 0;
int TC_FOUND_NOTBLANK = 0;
int TC_FOUND_BLANK = 0;
int usemalloc = 0;

char special_texts[] = SPECIAL_TEXTS;
char special_delimiters[] = SPECIAL_DELIMITERS;

hash_entry freq_words_table[MAX_WORD_LEN+2][256];		/* 256 is the maximum possible number of special words */
char freq_words_strings[256][MAX_WORD_LEN+2];
int freq_words_lens[256];

set_usemalloc()
{
	usemalloc = 1;
}

unset_usemalloc()
{
	usemalloc = 0;
}

unsigned int
mygetc(fp, buf, maxlen, lenp)
	FILE	*fp;
	unsigned char	*buf;
	int	*lenp;
{
	unsigned int	c;

	if (fp != NULL) c = getc(fp);
	if (buf != NULL) {
		if (*lenp >= maxlen) return MYEOF;
		else c = (unsigned int)buf[*lenp];
	}
	(*lenp) ++;
	return c;
}

myfpcopy(fp, src)
	FILE	*fp;
	char	*src;
{
	int i=0;

	while(*src) {
		putc(*src, fp);
		src ++;
		i ++;
	}
	return i;
}

mystrcpy(dest, src)
	char	*src, *dest;
{
	int i=0;

	while(*dest = *src) {
		dest ++;
		src ++;
		i ++;
	}
	return i;
}

/* Returns 1 if little endian, 0 if big endian */
int
get_endian()
{
	union{
		int	x;
		struct	{
			short	y1;
			short	y2;
		} y;
	} var;

	var.x = 0xffff0000;
	if (var.y.y1 == 0) return 1;
	else return 0;
}

/*
 * These procedures take care of the fact that the msb of the encoded
 * short cannot be < RESERVED_CHARS, and the lsb cannot be equal to '\n' or '\0'.
 */

unsigned char
encode_msb(i)
	unsigned char	i;
{
	return i + RESERVED_CHARS;
}

unsigned char
decode_msb(i)
	unsigned char	i;
{
	return i - RESERVED_CHARS;
}

unsigned char
encode_lsb(i)
	unsigned char	i;
{
	if (i == '\0') return MAX_LSB;
	if (i == '\n') return MAX_LSB + 1;
	return i;
}

unsigned char
decode_lsb(i)
	unsigned char	i;
{
	if (i == MAX_LSB) return '\0';
	if (i == MAX_LSB + 1) return '\n';
	return i;
}

unsigned short
encode_index(i)
	unsigned short	i;
{
	unsigned char msb, lsb;

	msb = (i / MAX_LSB);
	lsb = (i % MAX_LSB);
	msb = encode_msb(msb);
	lsb = encode_lsb(lsb);
	return (msb << 8) | lsb;
}

unsigned short
decode_index(i)
	unsigned short	i;
{
	unsigned char msb, lsb;

	msb = ((i & 0x0ff00) >> 8);
	lsb = (i & 0x00ff);
	msb = decode_msb(msb);
	lsb = decode_lsb(lsb);
	return (msb * MAX_LSB + lsb);
}

#if	0
/* This is bullshit */

unsigned short
encode_index(i)
	unsigned short	i;
{
	unsigned int msb, lsb;

top:
	msb = (i & 0xff00) >> 8;
	if ((i & 0x00ff) == '\n') { i = MAX_WORDS + msb; goto top; /* eliminate tail recursion */}
	lsb = (i & 0x00ff);
	msb += RESERVED_CHARS;
	return (0x0000ffff & ((msb << 8) | lsb));
}

unsigned short
decode_index(i)
	unsigned short	i;
{
	unsigned int msb, lsb, ret;

	msb = (i & 0xff00) >> 8;
	lsb = (i & 0x00ff);
	msb -= RESERVED_CHARS;
	ret = (0x0000ffff & ((msb << 8) | lsb));
	if (ret >= MAX_WORDS) ret = (((ret - MAX_WORDS) << 8) | '\n');
	return ret;
}
#endif	/*0*/

char	comp_signature[SIGNATURE_LEN];	/* SIGNATURE_LEN - 1 hex-chars terminated by '\0' */

/* returns the number of words read */
build_freq(freq_words_table, freq_words_strings, freq_words_lens, freq_file, flags)
	hash_entry	freq_words_table[MAX_WORD_LEN+2][256];
	char		freq_words_strings[256][MAX_WORD_LEN+2];
	int		freq_words_lens[256];
	char		*freq_file;
{
	FILE	*fp = fopen(freq_file, "r");
	int	len, num, i, j;
	hash_entry	*e;
	int	numsofar = 0;
	int	freq_words;

	memset(comp_signature, '\0', SIGNATURE_LEN);
	if (fp == NULL) {
		if (flags & TC_ERRORMSGS) {
			fprintf(stderr, "cannot open cast-dictionary file: %s\n", freq_file);
			fprintf(stderr, "(use -H to give a dictionary-dir or run 'buildcast' to make a dictionary)\n");
		}
		return -1;
	}

	/* initialize the tables by accessing only those entries which will be used */
	if (SIGNATURE_LEN != fread(comp_signature, 1, SIGNATURE_LEN, fp)) {
		if (flags & TC_ERRORMSGS) fprintf(stderr, "illegal cast signature in: %s\n", freq_file);
		fclose(fp);
		return -1;
	}
	comp_signature[SIGNATURE_LEN - 1] = '\0';	/* overwrite '\0' */
	fscanf(fp, "%d\n", &freq_words);
	if ((freq_words < 0) || (freq_words > 256 - MAX_SPECIAL_CHARS)) {
		if (flags & TC_ERRORMSGS) fprintf(stderr, "illegal number of frequent words %d outside [0, %d] in: %s\n", freq_words, 256-MAX_SPECIAL_CHARS, freq_file);
		fclose(fp);
		return -1;
	}
	if (freq_words == 0) {
		fclose(fp);
		return 0;
	}
	for (i=0; i<=MAX_WORD_LEN; i++) {
		for (j=0; j<freq_words; j++)
			freq_words_table[i][j].val.offset = -1;
	}
	memset(freq_words_lens, '\0', sizeof(int)*freq_words);
	for (i=0; i<freq_words; i++) {
		freq_words_strings[i][0] = '\0';
	}

	/* Refer to read_in.c for the format in which these words are dumped */
	while (2 == fscanf(fp, "%d %d\n", &len, &num)) {
		for(i=0; i<num; i++) {
			e = &(freq_words_table[len][i]);
			e->word = &(freq_words_strings[numsofar + i][0]);
			if (1 != fscanf(fp, "%s\n", e->word)) {
				fclose(fp);
				return numsofar;
			}
			freq_words_lens[numsofar + i] = len;
			e->val.offset = numsofar + i;	/* which-th special word is it? */
			if (i + 1 == num)
				e->next = NULL;
			else e->next = &(freq_words_table[len][i+1]);
		}
		numsofar += num;
	}
	fclose(fp);
	return numsofar;
}

int	initialize_common_done = 0;

/* Used in tcomp.c, tuncomp.c and csearch.c */
initialize_common(freq_file, flags)
char	*freq_file;
int	flags;
{
	if (initialize_common_done == 1) return 1;
	if (SPECIAL_WORDS == -1) return 0;
	if ((freq_file == NULL) || (freq_file[0] == '\0')) return 0;	/* courtesy: crd@hplb.hpl.hp.com */
        if ((SPECIAL_WORDS = build_freq(freq_words_table, freq_words_strings, freq_words_lens, freq_file, flags)) == -1) return 0;
        BEGIN_SPECIAL_WORDS = MAX_SPECIAL_CHARS;
        RESERVED_CHARS = END_SPECIAL_WORDS = BEGIN_SPECIAL_WORDS + SPECIAL_WORDS;
        MAX_WORDS = MAX_LSB*(256-RESERVED_CHARS);	/* upper byte must be > RESERVED_CHARS, lower byte must not be '\n' */
	TC_FOUND_NOTBLANK = 0;
	TC_FOUND_BLANK = 0;
	initialize_common_done = 1;
	return 1;
}

uninitialize_common()
{
	initialize_common_done = 0;
	return;
}

/*
 * Simple O(worlen*linelen) search since the average linelen is
 * guaranteed to be ~ 80/2, and the average wordlen, 2.
 * SHOULD WORK FOR ANY LEGITIMATE COMPRESSED STRING WITH EASY SEARCH
 */
int
exists_tcompressed_word(word, wordlen, line, linelen, flags)
	CHAR	*word, *line;
	int	wordlen, linelen;
{
	int	i, j;

#if	0
	for (i=0; i<linelen; i++) printf("%d ", line[i]);
	printf("\n");
	for (i=0; i<wordlen; i++) printf("%d ", word[i]);
	printf("\n");
#endif	/*0*/

	if (wordlen > linelen) return -1;
	if (flags & TC_EASYSEARCH) {
		for (i=0; i<=linelen-wordlen; i++) {
			if (word[0] == BEGIN_VERBATIM)
				while ((i <= linelen - wordlen) && (line[i] != BEGIN_VERBATIM)) i++;
			j = 0;
			while ((j < wordlen) && (i <= linelen - wordlen) && (word[j] == line[i+j])) j++;
			if (j >= wordlen) return i;
			if (i > linelen - wordlen) return -1;

			/* Goto next-pos for i. Remember: the for loop ALSO skips over one i */
			if (line[i] >= RESERVED_CHARS) i++;
			else if (line[i] == BEGIN_VERBATIM) {
				i++;
				while ((i <= linelen - wordlen) && (line[i] != END_VERBATIM)) i++;
				if (i > linelen - wordlen) return -1;
			}
			else if (line[i] == EASY_ONE_VERBATIM) i++;
		}
	}
	else {
		for (i=0; i<=linelen-wordlen; i++) {
			if (word[0] == BEGIN_VERBATIM)
				while ((i <= linelen - wordlen) && (line[i] != BEGIN_VERBATIM)) i++;
			j = 0;
			while ((j < wordlen) && (i <= linelen - wordlen) && (word[j] == line[i+j])) j++;
			if (j >= wordlen) return i;
			if (i > linelen - wordlen) return -1;

			/* Goto next-pos for i. Remember: the for loop ALSO skips over one i */
			if (line[i] >= RESERVED_CHARS) i++;
			else if (line[i] == BEGIN_VERBATIM) {
				i++;
				while ((i <= linelen - wordlen) && (line[i] != BEGIN_VERBATIM) && (line[i] != END_VERBATIM)) i++;
				if (i > linelen - wordlen) return -1;
				if (line[i] == BEGIN_VERBATIM) i--;	/* counter-act the i++ */
			}
		}
	}

	return -1;
}

/*
 * There is a problem here if we use these two routines to search for delimiters:
 * With outtail set, the implicit blank AFTER the word just before the beginning
 * of the record and a possible NOTBLANK after the end of the record might be missed.
 * No way to rectify it now unless we have flags to indicate if these things occured.
 * That is why, I have introduced TC_FOUND_NOTBLANK and TC_FOUND_BLANK.
 */

/* return where the word begins or ends (=outtail): range = [begin, end) */
unsigned char *
forward_tcompressed_word(begin, end, delim, len, outtail, flags)
	unsigned char *begin, *end, *delim;
	int len, outtail, flags;
{
	register unsigned char *curend;
	register int pos;

	TC_FOUND_NOTBLANK = 0;
	if (begin + len > end) return end + 1;
	curend = begin;
top:
	while ((curend <= end) && (*curend != '\n')) curend ++;
	if ((pos = exists_tcompressed_word(delim, len, begin, curend-begin, flags)) == -1) {
		curend ++;	/* for next '\n' */
		if (curend > end) return end + 1;
		begin = curend;
		goto top;
	}
	begin += pos;	/* place where delimiter begins */
	if (outtail) {
		TC_FOUND_NOTBLANK = 1;
		return begin + len;
	}
	else return begin;
}

/* return where the word begins or ends (=outtail): range = [begin, end) */
unsigned char *
backward_tcompressed_word(end, begin, delim, len, outtail, flags)
	unsigned char *begin, *end, *delim;
	int len, outtail, flags;
{
	register unsigned char *curbegin;
	register int pos;

	TC_FOUND_BLANK = 0;
	if (begin + len > end) return begin;
	curbegin = end;
top:
	while ((curbegin > begin) && (*curbegin != '\n')) curbegin --;
	if ((pos = exists_tcompressed_word(delim, len, curbegin, end-curbegin, flags)) == -1) {
		curbegin --;	/* for next '\n' */
		if (curbegin < begin) return begin;
		end = curbegin;
		goto top;
	}
	curbegin += pos;	/* place where delimiter begins */
	if (outtail) {
		if ((curbegin + len < end) && (*(curbegin + len) != NOTBLANK)) TC_FOUND_BLANK = 1;
		return curbegin + len;
	}
	else return curbegin;
}

/* Escapes single quotes in "original" string with backquote (\) s.t. it can be passed on to the shell as a file name: returns its second argument for printf */
/* Called before passing any argument to the system() routine in glimpse or glimspeindex source code */
/* Works only if the new name is going to be passed as argument to the shell within two ''s */
char *
tescapesinglequote(original, new)
	char	*original, *new;
{
	char	*oldnew = new;
	while (*original != '\0') {
		if (*original == '\'') {
			*new ++ = '\'';	/* close existing ' : this guy will be a part of a file name starting from a ' */
			*new ++ = '\\';	/* add escape character */
			*new ++ = '\'';	/* add single quote from original here */
		}
		*new ++ = *original ++; /* start the real single quote to continute existing file name if *original was ' */
	}
	*new = *original;
	return oldnew;
}
