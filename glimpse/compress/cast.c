/* Copyright (c) 1994 Burra Gopal, Udi Manber.  All Rights Reserved. */

/*
 * cast.c:	main text compression routines. Exports tcompress() called
 *		from main() in read_out.c, and one other simple routine
 *		tcompressible_file(). This module can also be used from csearch.c.
 */

#include "defs.h"
#include <sys/time.h>
#if defined(__NeXT__)
                                      /* NeXT has no <utime.h> */
struct utimbuf {
        time_t actime;          /* access time */
        time_t modtime;         /* modification time */
};
#else
#include <utime.h>
#endif
#define ALNUMWORDS 1

#define MYEOF	0xffffffff

extern int RESERVED_CHARS;
extern int MAX_WORDS;
extern int SPECIAL_WORDS;
extern int BEGIN_SPECIAL_WORDS;
extern int END_SPECIAL_WORDS;
extern int NUM_SPECIAL_DELIMITERS;
extern int END_SPECIAL_DELIMITERS;
extern int ONE_VERBATIM;
extern int next_free_hash, next_free_str;
extern hash_entry freq_words_table[MAX_WORD_LEN+2][256];		/* 256 is the maximum possible number of special words */
extern char freq_words_strings[256][MAX_WORD_LEN+2];
extern int freq_words_lens[256];
extern char comp_signature[SIGNATURE_LEN];
extern hash_entry *compress_hash_table[HASH_TABLE_SIZE];

extern int usemalloc;

/* initialize and load dictionaries */
initialize_tcompress(hash_file, freq_file, flags)
	char	*hash_file, *freq_file;
	int	flags;
{
	FILE	*hashfp;

	if (!initialize_common(freq_file, flags)) return 0;
	next_free_hash = 0;
	memset(compress_hash_table, '\0', sizeof(hash_entry *) * HASH_TABLE_SIZE);
        if (MAX_WORDS == 0) return 1;

	/* Load compress dictionary */
	if ((hashfp = fopen(hash_file, "r")) == NULL) {
		if (flags & TC_ERRORMSGS) {
			fprintf(stderr, "cannot open cast-dictionary file: %s\n", hash_file);
			fprintf(stderr, "(use -H to give a dictionary-dir or run 'buildcast' to make a dictionary)\n");
		}
		return 0;
	}
	if (!tbuild_hash(compress_hash_table, hashfp, -1)) {	/* read all bytes until end */
		fclose(hashfp);
		return 0;
	}
	fclose(hashfp);
	return 1;
}

uninitialize_tcompress()
{
	int	i;
	hash_entry *e, *t;

	uninitialize_common();
	if (usemalloc) {
		for (i=0; i<HASH_TABLE_SIZE; i++) {
			e = compress_hash_table[i];
			while (e != NULL) {
				t = e;
				e = e->next;
				free(t->word);
				free(t);
			}
		}
	}
	memset(compress_hash_table, '\0', sizeof(hash_entry *) * HASH_TABLE_SIZE);
	next_free_hash = next_free_str = 0;
}

/* TRUE if input file has been compressed already, FALSE otherwise */
int
already_tcompressed(buffer, length, flags)
	char	*buffer;
	int	length;
	int	flags;
{
	char	*sig = comp_signature;

	if (!strncmp(buffer, sig, SIGNATURE_LEN - 1)) {
		if (flags & TC_ERRORMSGS)
			fprintf(stderr, "Already compressed,");
		return 1;
	}
	return 0;
}

extern int initialize_common_done;

/* TRUE if input file is an ascii file, FALSE otherwise */
int
tcompressible(buffer, num_read, flags)
	char	*buffer;
	int	num_read;
	int	flags;
{
	if (!initialize_common_done) {
		if (flags & TC_ERRORMSGS)
			fprintf(stderr, "No cast-dictionary,");
		return 0;
	}
        if(ttest_binary(buffer, num_read)) {
		if (flags & TC_ERRORMSGS)
			fprintf(stderr, "Binary data,");
		return(0);
	}
        if(ttest_uuencode(buffer, num_read)) {
		if (flags & TC_ERRORMSGS)
			fprintf(stderr, "UUEncoded data,");
		return(0);
	}
        if(ttest_postscript(buffer, num_read)) {
		if (flags & TC_ERRORMSGS)
			fprintf(stderr, "Postscript data,");
		return(0);
	}
	if (already_tcompressed(buffer, num_read, flags)) return 0;
	return(1);
}

tcompressible_file(name, flags)
	char	*name;
	int	flags;
{
	char	buf[SAMPLE_SIZE + 2];
	int	num;
	FILE	*fp = my_fopen(name, "r");

	if (!initialize_common_done) {
		if (flags & TC_ERRORMSGS)
			fprintf(stderr, "No cast-dictionary,");
		if (fp != NULL) fclose(fp);
		return 0;
	}
	if (fp == NULL) return 0;
	num = fread(buf, 1, SAMPLE_SIZE, fp);
	fclose(fp);
	return(tcompressible(buf, num, flags));
}

tcompressible_fp(fp, flags)
	FILE	*fp;
	int	flags;
{
	char	buf[SAMPLE_SIZE + 2];
	int	num;

	if (!initialize_common_done) {
		if (flags & TC_ERRORMSGS)
			fprintf(stderr, "No cast-dictionary,");
		return 0;
	}
	if (fp == stdin) return 1;
	num = fread(buf, 1, SAMPLE_SIZE, fp);
	return(tcompressible(buf, num, flags));
}

/* -------------------------------------------------------------------------
tgetword():
get a word from stream pointed to by fp: a "word" is an ID = a stream of
alphanumeric characters beginning with an alphanumeric char and ending with
a non-alphanumeric character. The character following the word is returned,
and the file pointer points to THIS character in the input stream. If there
is no word beginning at the current position of the file pointer, tgetword
simply behaves like getc(), i.e., just returns the character read. If the
word is too long, then it fills up all the bytes it can and returns the
character it could not fill up.

To read a series of words without doing an ungetc() for the extra character
read by tgetword, the caller can set *length to 1 and word[0] to the character
returned by tgetword. This can make compress work even if infile = stdin.
--------------------------------------------------------------------------*/
unsigned int
tgetword(fp, buf, maxinlen, lenp, word, length)
	FILE	*fp;
	char	*buf;
	int	maxinlen;
	int	*lenp;
	char	*word;
	int	*length;
{
	unsigned int	c;

#if	!ALNUMWORDS
	if (*length > 0){
		c = (unsigned char)word[*length - 1];
		if (!isalpha(c)) goto not_alpha;
		else goto alpha;
	}

	if ((c = mygetc(fp, buf, maxinlen, lenp)) == MYEOF) return MYEOF;
	if (!isalpha(c)) {	/* this might be a number */
		if (!isdigit(c)) return c;
		word[*length] = c;
		(*length) ++;
		word[*length] = '\0';
	not_alpha:
		while(isdigit(c = mygetc(fp, buf, maxinlen, lenp))) {
			if (*length >= MAX_NAME_LEN) return c;
			word[*length] = c;
			(*length) ++;
			word[*length] = '\0';
		}
		return c;
	}
	else {	/* this might be a dictionary word */
		word[*length] = c;
		(*length) ++;
		word[*length] = '\0';
	alpha:
		while(isalnum(c = mygetc(fp, buf, maxinlen, lenp))) {
			if (*length >= MAX_NAME_LEN) return c;
			word[*length] = c;
			(*length) ++;
			word[*length] = '\0';
		}
		return c;
	}
#else	/*!ALNUMWORDS*/
	if (*length > 0){
		c = word[*length - 1];
	}
	else {
		if ((c = mygetc(fp, buf, maxinlen, lenp)) == MYEOF) return MYEOF;
		if (!isalnum(c)) return c;
		word[(*length)++] = c;
		word[*length] = '\0';
	}
	while(((c = mygetc(fp, buf, maxinlen, lenp)) != MYEOF) && (isalnum(c))) {
		if (*length >= MAX_NAME_LEN) return c;
		word[*length] = c;
		(*length) ++;
		word[*length] = '\0';
	}
	return c;
#endif	/*!ALNUMWORDS*/
}

/*--------------------------------------------------------------------
Skips a series of characters of the type skipc and sets the number of
characters skipped. Used to compress multiple blanks, tabs & newlines.
It returns the first character not equal to skipc. If there are no
characters beginning at the current location of the file pointer
which are equal to skipc, this function simply behaves as getc().
---------------------------------------------------------------------*/
int
skip(fp, buf, maxinlen, lenp, skipc, skiplen)
	FILE	*fp;
	char	*buf;
	int	maxinlen;
	int	*lenp;
	int	skipc;
	int	*skiplen;
{
	unsigned int	c;

	*skiplen = 1;	/* c has already been read! */
	while((c = mygetc(fp, buf, maxinlen, lenp)) == skipc) (*skiplen) ++;
	return c;
}

/* defined in misc.c */
extern char special_texts[];
extern char special_delimiters[];

int
get_special_text_index(c)
	unsigned int	c;
{
	int	i;

	for(i=0; i<NUM_SPECIAL_TEXTS; i++)
		if (special_texts[i] == c) return i;
	return -1;
}

int
get_special_delimiter_index(c)
	unsigned int	c;
{
	int	i;

	for(i=0; i<NUM_SPECIAL_DELIMITERS; i++)
		if (special_delimiters[i] == c) return i;
	return -1;
}

#define process_spaces(skipc, skiplen)\
{\
	int	count2 = 0, count1 = 0, i;\
\
	count2 = (skiplen/2);\
	count1 = (skiplen%2);\
\
	if (easysearch) {\
		switch(skipc)\
		{\
		case ' ':\
			if ((maxoutlen >= 0) && (outlen + count1 + count2 >= maxoutlen)) return outlen;\
			if (outfp != NULL) {\
				for (i=0; i<count2; i++) putc(TWOBLANKS, outfp);\
				for (i=0; i<count1; i++) putc(BLANK, outfp);\
				outlen += count1 + count2;\
			}\
			if (outbuf != NULL) {\
				for (i=0; i<count2; i++) outbuf[outlen ++] = TWOBLANKS;\
				for (i=0; i<count1; i++) outbuf[outlen ++] = BLANK;\
			}\
			break;\
\
		case '\t':\
			if ((maxoutlen >= 0) && (outlen + count1 + count2 >= maxoutlen)) return outlen;\
			if (outfp != NULL) {\
				for (i=0; i<count2; i++) putc(TWOTABS, outfp);\
				for (i=0; i<count1; i++) putc(TAB, outfp);\
				outlen += count1 + count2;\
			}\
			if (outbuf != NULL) {\
				for (i=0; i<count2; i++) outbuf[outlen ++] = TWOTABS;\
				for (i=0; i<count1; i++) outbuf[outlen ++] = TAB;\
			}\
			break;\
\
		case '\n':\
			if ((maxoutlen >= 0) && (outlen + count1 + count2*2 >= maxoutlen)) return outlen;\
			if (outfp != NULL) {\
				for (i=0; i<count2*2; i++) putc(NEWLINE, outfp);\
				for (i=0; i<count1; i++) putc(NEWLINE, outfp);\
				outlen += count1 + count2*2;\
			}\
			if (outbuf != NULL) {\
				for (i=0; i<count2*2; i++) outbuf[outlen ++] = NEWLINE;\
				for (i=0; i<count1; i++) outbuf[outlen ++] = NEWLINE;\
			}\
			break;\
\
		default: break;	/* cannot reach here */\
		}\
	}\
	else {\
		if ((maxoutlen >= 0) && (outlen + count1 + count2 >= maxoutlen)) return outlen;\
		switch(skipc)\
		{\
		case ' ':\
			if (outfp != NULL) {\
				for (i=0; i<count2; i++) putc(TWOBLANKS, outfp);\
				for (i=0; i<count1; i++) putc(BLANK, outfp);\
				outlen += count1 + count2;\
			}\
			if (outbuf != NULL) {\
				for (i=0; i<count2; i++) outbuf[outlen ++] = TWOBLANKS;\
				for (i=0; i<count1; i++) outbuf[outlen ++] = BLANK;\
			}\
			break;\
\
		case '\t':\
			if (outfp != NULL) {\
				for (i=0; i<count2; i++) putc(TWOTABS, outfp);\
				for (i=0; i<count1; i++) putc(TAB, outfp);\
				outlen += count1 + count2;\
			}\
			if (outbuf != NULL) {\
				for (i=0; i<count2; i++) outbuf[outlen ++] = TWOTABS;\
				for (i=0; i<count1; i++) outbuf[outlen ++] = TAB;\
			}\
			break;\
\
		case '\n':\
			if (outfp != NULL) {\
				for (i=0; i<count2; i++) putc(TWONEWLINES, outfp);\
				for (i=0; i<count1; i++) putc(NEWLINE, outfp);\
				outlen += count1 + count2;\
			}\
			if (outbuf != NULL) {\
				for (i=0; i<count2; i++) outbuf[outlen ++] = TWONEWLINES;\
				for (i=0; i<count1; i++) outbuf[outlen ++] = NEWLINE;\
			}\
			break;\
\
		default: break;	/* cannot reach here */\
		}\
	}\
}

#define PRE_VERBATIM(v)\
{\
	if (!v) {\
		v = 1;\
		if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;\
		if (outfp != NULL) putc(BEGIN_VERBATIM, outfp);\
		if (outbuf != NULL) outbuf[outlen] = BEGIN_VERBATIM;\
		outlen ++;\
	}\
}

#define POST_VERBATIM(v) \
{\
	if (v) {\
		v = 0;\
		if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;\
		if (outfp != NULL) putc(END_VERBATIM, outfp);\
		if (outbuf != NULL) outbuf[outlen] = END_VERBATIM;\
		outlen ++;\
	}\
}

#define EASY_PRE_VERBATIM(v) \
{\
	if (easysearch) {\
		if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;\
		if (outfp != NULL) putc(ONE_VERBATIM, outfp);\
		if (outbuf != NULL) outbuf[outlen] = ONE_VERBATIM;\
		outlen ++;\
	}\
	else {\
		PRE_VERBATIM(v)\
	}\
}

#define EASY_POST_VERBATIM(v) \
{\
	if (easysearch) {\
		POST_VERBATIM(v)\
	}\
	/* else ignore */\
}

int
get_special_word_index(word, len)
	char	word[MAX_NAME_LEN];
	int	len;
{
	register int	comp;
	hash_entry	*e;

	if ((len > MAX_WORD_LEN) || (SPECIAL_WORDS <= 0)) return -1;
	e = freq_words_table[len];
	while((e != NULL) && (e->val.offset != -1)) {
		comp = strcmp(word, e->word);
		if (comp == 0) return e->val.offset;
		if (comp < 0) return -1;	/* can't find it anyway */
		e = e->next;
	}
	return -1;
}

/* Compresses input from indata and outputs it into outdata: returns number of chars in output */
int
tcompress(indata, maxinlen, outdata, maxoutlen, flags)
	void	*indata, *outdata;
	int	maxinlen, maxoutlen;
	int	flags;
{
	unsigned char	curword[MAX_NAME_LEN];
	int	curlen;
	int	hashindex;
	hash_entry *e;
	unsigned int	c;
	unsigned short	encodedindex;
	int	skiplen;
	int	ret;
	int	verbatim_state = 0;
	char	*sig = comp_signature;
	FILE	*infp = NULL, *outfp = NULL;
	unsigned char	*inbuf = NULL, *outbuf = NULL;
	int	outlen = 0, inlen = 0;
	int	easysearch = flags&TC_EASYSEARCH;
	int	untilnewline = flags&TC_UNTILNEWLINE;

	if (flags & TC_SILENT) return 0;
	if (easysearch) {
		ONE_VERBATIM = EASY_ONE_VERBATIM;
		NUM_SPECIAL_DELIMITERS = EASY_NUM_SPECIAL_DELIMITERS;
		END_SPECIAL_DELIMITERS = EASY_END_SPECIAL_DELIMITERS;
	}
	else {
		ONE_VERBATIM = HARD_ONE_VERBATIM;
		NUM_SPECIAL_DELIMITERS = HARD_NUM_SPECIAL_DELIMITERS;
		END_SPECIAL_DELIMITERS = HARD_END_SPECIAL_DELIMITERS;
	}

	if (maxinlen < 0) {
		infp = (FILE *)indata;
	}
	else {
		inbuf = (unsigned char *)indata;
	}
	if (maxoutlen < 0) {
		outfp = (FILE *)outdata;
	}
	else {
		outbuf = (unsigned char *)outdata;
	}

	/* Write signature and information about whether compression was context-free or not: first 16 bytes */
	if (outfp != NULL) {
		if ((maxoutlen >= 0) && (outlen + SIGNATURE_LEN >= maxoutlen)) return outlen;
		if (0 == fwrite(sig, 1, SIGNATURE_LEN - 1, outfp)) return 0;
		if (easysearch) putc(1, outfp);
		else putc(0, outfp);
		outlen += SIGNATURE_LEN;
	}
	/* No need to put a signature OR easysearch when doing it in memory: caller must manipulate */

	/*
	 * The algorithm for compression is as follows:
	 *
	 * For each input word, we search and see if it is in the dictionary.
	 * If it IS there, we just look at its word-index and output it.
	 * Then, if the character immediately after the word is NOT a blank,
	 * we output a second character indicating what it was.
	 *
	 * If it is not in the dictionary then we output it verbatim: for
	 * verbatim o/p, we take care to merge consecutive verbatim outputs
	 * by NOT putting delimiters between them (one start and one end
	 * delimiter).
	 *
	 * If the input is not a word but a single character, then it can be:
	 * 1. A special character, in which case we output its code.
	 * 2. A blank character in which case we keep getting more characters
	 *    to see howmany blanks we get. At the first non blank character,
	 *    we output a sequence of special characters which encode multiple
	 *    blanks (note: blanks can be spaces, tabs or newlines).
	 *
	 * Please refer to the state diagram for explanations.
	 * I've used gotos since the termination condition is too complex.
	 */

real_tgetword:
	curlen = 0;
	curword[0] = '\0';

concocted_tgetword:
	c = tgetword(infp, inbuf, maxinlen, &inlen, curword, &curlen);

bypass_tgetword:
	if (curlen == 0) { /* only one character read and that is in c. */
		switch(c)
		{
		case ' ':
		case '\t':
		case '\n':
			POST_VERBATIM(verbatim_state);	/* need post-verbatim since there might be a LOT of blanks, etc. */
			ret = skip(infp, inbuf, maxinlen, &inlen, c, &skiplen);
			process_spaces(c, skiplen);
			if ((c == '\n') && untilnewline) return outlen;
			if (isalnum((unsigned char)ret)) {
				curword[0] = (unsigned char)ret;
				curword[1] = '\0';
				curlen = 1;
				goto concocted_tgetword;
			}
			else if (ret != MYEOF) {
				c = (unsigned int)ret;
				goto bypass_tgetword;
			}
			/* else fall thru */

		case MYEOF: return outlen;

		default:
			if ((ret = get_special_text_index(c)) != -1) {
				if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;
				if (verbatim_state) {	/* no need to do post-verbatim since only one character: optimization */
					if (outfp != NULL) putc(c, outfp);
					if (outbuf != NULL) outbuf[outlen] = c;
					outlen ++;
				}
				else {
					if (outfp != NULL) putc(ret + BEGIN_SPECIAL_TEXTS, outfp);
					if (outbuf != NULL) outbuf[outlen] = ret + BEGIN_SPECIAL_TEXTS;
					outlen ++;
				}
			}
			else {
				/*
				 * Has to be verbatim character: they have a ONE_VERBATIM before each
				 * irrespective of verbatim_state. Otherwise there is no way to differentiate
				 * one of our special characters from the same characters appearing in the
				 * source. Hence binary files blow-up to twice their original size.
				 * 
				 * Also, if it is a verbatim character that cannot be confused with one of OUR
				 * special characters, then just put it in w/o changing verbatim state. Else
				 * put a begin-verbatim before it and THEN output that character=saves 1 char.
				 */
				if ((c != BEGIN_VERBATIM) && (c != END_VERBATIM)) {	/* reduces to below if easysearch */
					EASY_PRE_VERBATIM(verbatim_state)
					if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;
					if (outfp != NULL) putc(c, outfp);
					if (outbuf != NULL) outbuf[outlen] = c;
					outlen ++;
				}
				else {	/* like \ escape in C: \ is \\ */
					if ((maxoutlen >= 0) && (outlen + 2 >= maxoutlen)) return outlen;
					if (outfp != NULL) putc(ONE_VERBATIM, outfp);
					if (outbuf != NULL) outbuf[outlen] = ONE_VERBATIM;
					outlen ++;
					if (outfp != NULL) putc(c, outfp);
					if (outbuf != NULL) outbuf[outlen] = c;
					outlen ++;
				}
			}
			goto real_tgetword;
		}
	}
	else	/* curlen >= 1 */
	{
		if (!easysearch && verbatim_state && (curlen <= 2)) {
			fprintf(outfp, "%s", curword);	/* don't bother to close the verbatim state and put a 2byte index=saves 1 char */
			curword[0] = '\0';
			curlen = 0;
			goto bypass_tgetword;
		}
		else
		{
			if ((ret = get_special_word_index(curword, curlen)) != -1) {
				POST_VERBATIM(verbatim_state);
				/* printf("ret=%d word=%s\n", ret, curword); */
				if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;
				if (outfp != NULL) putc(ret + BEGIN_SPECIAL_WORDS, outfp);
				if (outbuf != NULL) outbuf[outlen] = ret + BEGIN_SPECIAL_WORDS;
				outlen ++;
			}
			else if ((e = get_hash(compress_hash_table, curword, curlen, &hashindex)) != NULL) {
#if	0
				fprintf(stderr, "%x ", e->val.attribute.index);
#endif	/*0*/
				encodedindex = encode_index(e->val.attribute.index);
				POST_VERBATIM(verbatim_state);
				if ((maxoutlen >= 0) && (outlen + sizeof(short) >= maxoutlen)) return outlen;
				if (outfp != NULL) {
					putc(((encodedindex & 0xff00)>>8), outfp);
					putc((encodedindex & 0x00ff), outfp);
				}
				if (outbuf != NULL) {
					outbuf[outlen] = ((encodedindex & 0xff00)>>8);
					outbuf[outlen + 1] = encodedindex & 0x00ff;
				}
				outlen += sizeof(short);
			}
			else goto NOT_IN_DICTIONARY;

		/* process_char_after_word: */
			switch(c)
			{
			case ' ':
				goto real_tgetword;	/* blank is a part of the word */

			case MYEOF:
				if (easysearch) return outlen;
				if (outfp != NULL) putc(NOTBLANK, outfp);
				if (outbuf != NULL) outbuf[outlen] = NOTBLANK;
				outlen ++;
				return outlen;

			default:
				if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;
				if ((ret = get_special_delimiter_index(c)) != -1) {
					if (outfp != NULL) putc((ret+BEGIN_SPECIAL_DELIMITERS), outfp);
					if (outbuf != NULL) outbuf[outlen] = ret + BEGIN_SPECIAL_DELIMITERS;
					outlen ++;
					goto real_tgetword;
				}
				else {
					if (outfp != NULL) putc(NOTBLANK, outfp);
					if (outbuf != NULL) outbuf[outlen] = NOTBLANK;
					outlen ++;
					if (!isalnum(c)) {
						curword[0] = '\0';
						curlen = 0;
						goto bypass_tgetword;
					}
					else {	/* might be a number which ended with an alphabet: ".. born in 1992AD" */
						curword[0] = c;
						curword[1] = '\0';
						curlen = 1;
						goto concocted_tgetword;
					}
				}
			}
		}

	NOT_IN_DICTIONARY: /* word not in dictionary */
		PRE_VERBATIM(verbatim_state);
		if ((maxoutlen >= 0) && (outlen + curlen >= maxoutlen)) return outlen;
		if ((outfp != NULL) && (0 == fwrite(curword, sizeof(char), curlen, outfp))) return 0;
		if (outbuf != NULL) memcpy(outbuf+outlen, curword, curlen);
		outlen += curlen;
		EASY_POST_VERBATIM(verbatim_state);

		switch(c)
		{
		case MYEOF: /* Prefix searches still work since our scheme is context free */ return outlen;

		default:
			if (!isalnum(c)) {
				curword[0] = '\0';
				curlen = 0;
				goto bypass_tgetword;
			}
			else {	/* might be a number which ended with an alphabet: ".. born in 1992AD" */
				curword[0] = c;
				curword[1] = '\0';
				curlen = 1;
				goto concocted_tgetword;
			}
		}
	}
}

#define FUNCTION	tcompress_file
#define DIRECTORY	tcompress_directory
#include "trecursive.c"

/* returns #bytes (>=0) in the compressed file, -1 if major error (not able to compress) */
tcompress_file(name, outname, flags)
	char	*name, *outname;
	int	flags;
{
	FILE	*fp;
	FILE	*outfp;
	int	inlen, ret;
	struct stat statbuf;
	/* struct timeval tvp[2]; */
	struct utimbuf tvp;
	char	tempname[MAX_LINE_LEN];

	if (name == NULL) return -1;
	special_get_name(name, -1, tempname);
	inlen = strlen(tempname);
	if (-1 == stat(tempname, &statbuf)) {
		if (flags & TC_ERRORMSGS)
			fprintf(stderr, "permission denied or non-existent: %s\n", tempname);
		return -1;
	}
	if (S_ISDIR(statbuf.st_mode)) {
		if (flags & TC_RECURSIVE) return tcompress_directory(tempname, outname, flags);
		if (flags & TC_ERRORMSGS)
			fprintf(stderr, "skipping directory: %s\n", tempname);
		return -1;
	}
	if (!S_ISREG(statbuf.st_mode)) {
		if (flags & TC_ERRORMSGS)
			fprintf(stderr, "not a regular file, skipping: %s\n", tempname);
		return -1;
	}
	if ((fp = fopen(tempname, "r")) == NULL) {
		if (flags & TC_ERRORMSGS)
			fprintf(stderr, "permission denied or non-existent: %s\n", tempname);
		return -1;
	}
	if (!tcompressible_fp(fp, flags)) {
		if (flags & TC_ERRORMSGS)
			fprintf(stderr, " skipping: %s\n", tempname);
		fclose(fp);
		return -1;
	}
	rewind(fp);
	if (flags & TC_SILENT) {
		printf("%s\n", tempname);
		fclose(fp);
		return 0;
	}

	/* Create and open output file */
	strncpy(outname, tempname, MAX_LINE_LEN);
	if (inlen + strlen(COMP_SUFFIX) + 1 >= MAX_LINE_LEN) {
		outname[MAX_LINE_LEN - strlen(COMP_SUFFIX)] = '\0';
		fprintf(stderr, "very long file name %s: truncating to: %s", tempname, outname);
	}
	strcat(outname, COMP_SUFFIX);
	if (!access(outname, R_OK)) {	/* output file exists */
		if (!(flags & TC_OVERWRITE)) {
			fclose(fp);
			return 0;
		}
		else if (!(flags & TC_NOPROMPT)) {
			char	s[8];
			printf("overwrite %s? (y/n): ", outname);
			scanf("%c", s);
			if (s[0] != 'y') {
				fclose(fp);
				return 0;
			}
		}
	}
	if ((outfp = fopen(outname, "w")) == NULL) {
		fprintf(stderr, "cannot open for writing: %s\n", outname);
		fclose(fp);
		return -1;
	}

	ret = tcompress(fp, -1, outfp, -1, flags);
	if ((statbuf.st_size * (100 - COMP_ATLEAST))/100 < ret) {
		fprintf(stderr, "less than %d%% compression, skipping: %s\n", COMP_ATLEAST, tempname);
		fclose(fp);
		rewind(outfp);
		fclose(outfp);
		unlink(outname);
		return ret;
	}

	if ((ret > 0) && (flags & TC_REMOVE)) unlink(tempname);
	fclose(fp);
	fflush(outfp);
	fclose(outfp);
	/*
	tvp[0].tv_sec = statbuf.st_atime;
	tvp[0].tv_usec = 0;
	tvp[1].tv_sec = statbuf.st_mtime;
	tvp[1].tv_usec = 0;
	utimes(outname, tvp);
	*/
	tvp.actime = statbuf.st_atime;
	tvp.modtime = statbuf.st_mtime;
	utime(outname, &tvp);
	return ret;
}
