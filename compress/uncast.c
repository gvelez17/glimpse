/* Copyright (c) 1994 Burra Gopal, Udi Manber.  All Rights Reserved. */

/*
 * uncast.c:	main text uncompression routines. Exports tuncompress() called
 *		from main() in main_uncast.c, and one other simple routine
 *		tuncompressible_file().
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
#define MYEOF	0xffffffff

extern int RESERVED_CHARS;
extern int MAX_WORDS;
extern int SPECIAL_WORDS;
extern int BEGIN_SPECIAL_WORDS;
extern int END_SPECIAL_WORDS;
extern int NUM_SPECIAL_DELIMITERS;
extern int END_SPECIAL_DELIMITERS;
extern int ONE_VERBATIM;
extern int TC_FOUND_BLANK, TC_FOUND_NOTBLANK;
extern char comp_signature[SIGNATURE_LEN];
extern hash_entry freq_words_table[MAX_WORD_LEN+2][256];	/* 256 is the maximum possible number of special words */
extern char freq_words_strings[256][MAX_WORD_LEN+2];
extern int freq_words_lens[256];
extern char *compress_string_table[DEF_MAX_WORDS]; /*[MAX_WORD_LEN+2]; */

extern int usemalloc, next_free_strtable;

initialize_tuncompress(string_file, freq_file, flags)
	char	*string_file, *freq_file;
	int	flags;
{
	FILE	*stringfp;

	if (!initialize_common(freq_file, flags)) return 0;
	next_free_strtable = 0;
	memset(compress_string_table, '\0', sizeof(char *) * DEF_MAX_WORDS);
	if (MAX_WORDS == 0) return 1;

	/* Load uncompress dictionary */
	if ((stringfp = fopen(string_file, "r")) == NULL) {
		if (flags & TC_ERRORMSGS) {
			fprintf(stderr, "cannot open cast-dictionary file: %s\n", string_file);
			fprintf(stderr, "(use -H to give a dictionary-dir or run 'buildcast' to make a dictionary)\n");
		}
		return 0;
	}
	if (!build_string(compress_string_table, stringfp, -1, 0)) {	/* read all bytes until end */
		fclose(stringfp);
		return 0;
	}
	fclose(stringfp);
	return 1;
}

uninitialize_tuncompress()
{
	int	i;

	uninitialize_common();
	if (usemalloc) {
		for (i=0; i<MAX_WORDS; i++) {
			if (compress_string_table[i] != NULL) free(compress_string_table[i]);
		}
	}
	memset(compress_string_table, '\0', sizeof(char *) * DEF_MAX_WORDS);
	next_free_strtable = 0;
}

extern int initialize_common_done;

/* TRUE if file has the signature in its first 15 bytes, false otherwise */
int
tuncompressible(buffer, num_read)
	char	*buffer;
	int	num_read;
{
	char	*sig = comp_signature;
	int	i;

	if (!initialize_common_done) return 0;
	if (num_read < SIGNATURE_LEN - 1) return 0;
	for (i=0; i<SIGNATURE_LEN - 1; i++)
		if (buffer[i] != sig[i]) return 0;
	return 1;
	/* a rewind is not done. hence this is useful even for stdin */
}

int
tuncompressible_filename(name, len)
	char	*name;
	int	len;
{
	char	tempname[MAX_LINE_LEN];
	if (!initialize_common_done) return 0;
	special_get_name(name, len, tempname);
	len = strlen(tempname);
	if ((len < strlen(COMP_SUFFIX) + 1) || (strcmp(&tempname[len-strlen(COMP_SUFFIX)], COMP_SUFFIX))) return 0;
	return 1;
}

int
tuncompressible_file(name)
	char	*name;
{
	char	buf[SIGNATURE_LEN + 2];
	int	num;
	FILE	*fp;

	if (!initialize_common_done) return 0;
	if (!tuncompressible_filename(name, strlen(name))) return 0;
	if ((fp = my_fopen(name, "r")) == NULL) return 0;
	num = fread(buf, 1, SIGNATURE_LEN - 1, fp);
	fclose(fp);
	return(tuncompressible(buf, num));
}

tuncompressible_fp(fp)
	FILE	*fp;
{
	char	buf[SIGNATURE_LEN + 2];
	int	num;

	if (!initialize_common_done) return 0;
	num = fread(buf, 1, SIGNATURE_LEN - 1, fp);
	return(tuncompressible(buf, num));
}

/* defined in misc.c */
extern char special_texts[];
extern char special_delimiters[];

#define process_special_char(c)\
{\
	if (outfp != NULL) {\
		switch(c)\
		{\
		case TWOBLANKS:\
			if ((maxoutlen >= 0) && (outlen + 2 >= maxoutlen)) return outlen;\
			putc(' ', outfp);\
			outlen ++;\
			putc(' ', outfp);\
			outlen ++;\
			break;\
		case BLANK:\
			if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;\
			putc(' ', outfp);\
			outlen ++;\
			break;\
\
		case TWOTABS:\
			if ((maxoutlen >= 0) && (outlen + 2 >= maxoutlen)) return outlen;\
			putc('\t', outfp);\
			outlen ++;\
			putc('\t', outfp);\
			outlen ++;\
			break;\
		case TAB:\
			if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;\
			putc('\t', outfp);\
			outlen ++;\
			break;\
\
		case TWONEWLINES:\
			if ((maxoutlen >= 0) && (outlen + 2 >= maxoutlen)) return outlen;\
			putc('\n', outfp);\
			outlen ++;\
			putc('\n', outfp);\
			outlen ++;\
			break;\
		case NEWLINE:\
			if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;\
			putc('\n', outfp);\
			outlen ++;\
			break;\
\
		default:\
			if ((c < END_SPECIAL_TEXTS) && (c >= BEGIN_SPECIAL_TEXTS)) {\
				if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;\
				putc(special_texts[c - BEGIN_SPECIAL_TEXTS], outfp); outlen ++;\
			}\
			else if ((c < END_SPECIAL_DELIMITERS) && (c >= BEGIN_SPECIAL_DELIMITERS)) {\
				if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;\
				putc(special_delimiters[c - BEGIN_SPECIAL_DELIMITERS], outfp); outlen ++;\
			}\
			else if ((c < END_SPECIAL_WORDS) && (c >= BEGIN_SPECIAL_WORDS)) {\
				if ((maxoutlen >= 0) && (outlen + freq_words_lens[c - BEGIN_SPECIAL_WORDS] >= maxoutlen)) return outlen;\
				fprintf(outfp, "%s", freq_words_strings[c - BEGIN_SPECIAL_WORDS]); outlen += freq_words_lens[c - BEGIN_SPECIAL_WORDS];\
			}\
			/* else should not have called this function */\
		}\
	}\
	if (outbuf != NULL) {\
		switch(c)\
		{\
		case TWOBLANKS:\
			if ((maxoutlen >= 0) && (outlen + 2 >= maxoutlen)) return outlen;\
			outbuf[outlen ++] = ' ';\
			outbuf[outlen ++] = ' ';\
			break;\
		case BLANK:\
			if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;\
			outbuf[outlen ++] = ' ';\
			break;\
\
		case TWOTABS:\
			if ((maxoutlen >= 0) && (outlen + 2 >= maxoutlen)) return outlen;\
			outbuf[outlen ++] = '\t';\
			outbuf[outlen ++] = '\t';\
			break;\
		case TAB:\
			if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;\
			outbuf[outlen ++] = '\t';\
			break;\
\
		case TWONEWLINES:\
			if ((maxoutlen >= 0) && (outlen + 2 >= maxoutlen)) return outlen;\
			outbuf[outlen ++] = '\n';\
			outbuf[outlen ++] = '\n';\
			break;\
		case NEWLINE:\
			if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;\
			outbuf[outlen ++] = '\n';\
			break;\
\
		default:\
			if ((c < END_SPECIAL_TEXTS) && (c >= BEGIN_SPECIAL_TEXTS)) {\
				if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;\
				outbuf[outlen ++] = special_texts[c - BEGIN_SPECIAL_TEXTS];\
			}\
			else if ((c < END_SPECIAL_DELIMITERS) && (c >= BEGIN_SPECIAL_DELIMITERS)) {\
				if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;\
				outbuf[outlen ++] = special_delimiters[c - BEGIN_SPECIAL_DELIMITERS];\
			}\
			else if ((c < END_SPECIAL_WORDS) && (c >= BEGIN_SPECIAL_WORDS)) {\
				/* printf("-->%s\n", freq_words_strings[c-BEGIN_SPECIAL_WORDS]); */\
				if ((maxoutlen >= 0) && (outlen + freq_words_lens[c - BEGIN_SPECIAL_WORDS] >= maxoutlen)) return outlen;\
				memcpy(outbuf+outlen, freq_words_strings[c - BEGIN_SPECIAL_WORDS], freq_words_lens[c - BEGIN_SPECIAL_WORDS]);\
				outlen += freq_words_lens[c - BEGIN_SPECIAL_WORDS]; \
			}\
			/* else should not have called this function */\
		}\
	}\
}

int UNCAST_ERRORS = 0;

/* Uncompresses input from indata and outputs it into outdata: returns number of chars in output */
int
tuncompress(indata, maxinlen, outdata, maxoutlen, flags)
	void	*indata, *outdata;
	int	maxinlen, maxoutlen;
	int	flags;
{
	unsigned short	index, dindex;
	unsigned int	c;
	int	verbatim_state = 0;
	int	inlen, outlen = 0;
	FILE	*infp = NULL, *outfp = NULL;
	unsigned char	*inbuf = NULL, *outbuf = NULL;
	int	easysearch = flags&TC_EASYSEARCH;
	int	untilnewline = flags&TC_UNTILNEWLINE;

	if (flags & TC_SILENT) return 0;
	if (maxinlen < 0) {
		infp = (FILE *)indata;
		if ((easysearch = mygetc(infp, inbuf, maxinlen, &inlen)) == MYEOF) return outlen;	/* ignore parameter: take from file */
		inlen = SIGNATURE_LEN;
	}
	else {	/* don't care about signature: user's responsibility */
		inbuf = (unsigned char *)indata;
		inlen = 0;
	}

	if (maxoutlen < 0) {
		outfp = (FILE *)outdata;
	}
	else {
		outbuf = (unsigned char *)outdata;
	}

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

	if (TC_FOUND_BLANK) {
		if (outfp != NULL) putc(' ', outfp);
		if (outbuf != NULL) outbuf[outlen] = ' ';
		outlen ++;
	}
	TC_FOUND_BLANK = 0;	/* default: use result of previous backward_tcompressed_word only */

	/*
	 * The algorithm, as expected, is a complete inverse of the compression
	 * algorithm: see tcompress.c in this directory to understand this function.
	 * I've used gotos since the termination condition is too complex.
	 * The two sub-parts are exactly the same except for verbatim processing.
	 * Actually, loop-unrolling was done here: you can combine them together but...
	 */
	if (easysearch)
	{	/* compress was done in a context-free way to speed up searches */
		while(1) {
			if((c = mygetc(infp, inbuf, maxinlen, &inlen)) == MYEOF) return outlen;

		bypass_getc1:
			if (c == ONE_VERBATIM) {
				if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;
				if ((c = mygetc(infp, inbuf, maxinlen, &inlen)) == MYEOF) return outlen;
				if (outfp != NULL) putc(c, outfp);	/* no processing whatsoever */
				if (outbuf != NULL) outbuf[outlen] = c;
				outlen ++;
			}
			else if (verbatim_state) {
				if (c == END_VERBATIM) verbatim_state = 0;
				else {
					if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;
					if (outfp != NULL) putc(c, outfp);
					if (outbuf != NULL) outbuf[outlen] = c;
					outlen ++;
				}
			}
			else if (c < END_SPECIAL_CHARS) {
				process_special_char(c)
				if ( ((c == NEWLINE) || (c == TWONEWLINES)) && untilnewline)
					return outlen;
			}
			else if (c == BEGIN_VERBATIM) {
				if((c = mygetc(infp, inbuf, maxinlen, &inlen)) == MYEOF) return outlen;
				if ((maxoutlen >= 0) && (outlen + 1>= maxoutlen)) return outlen;
				if (outfp != NULL) putc(c, outfp);
				if (outbuf != NULL) outbuf[outlen] = c;
				outlen ++;
				verbatim_state = 1;
			}
			else if (c == END_VERBATIM) {	/* not in verbatim state, still end_verbatim! */
				verbatim_state = 0;
				fprintf(stderr, "error in decompression after %d chars [verbatim processing]. skipping...\n", inlen);
				UNCAST_ERRORS = 1;
			}
			else
			{
			above1:
				if (c < RESERVED_CHARS) {	/* this is a special-word but not a special char */
					process_special_char(c)
				}
				else {	/* it is an index of a word in the dictionary since 1st byte >= RESERVED_CHARS */
					index = c;
					index <<= 8;
					if ((c = mygetc(infp, inbuf, maxinlen, &inlen)) == MYEOF) return outlen;
					index |= c;
					dindex = decode_index(index);
					if(dindex < MAX_WORDS) {
						if ((maxoutlen >= 0) && (outlen + AVG_WORD_LEN >= maxoutlen)) return outlen;
						if (outfp != NULL) outlen += myfpcopy(outfp, compress_string_table[dindex]);
						if (outbuf != NULL) {
							outlen += mystrcpy(outbuf+outlen, compress_string_table[dindex]);
						}
						if ((maxoutlen >= 0) && (outlen >= maxoutlen)) return outlen;
					}
					else {
						fprintf(stderr, "error in decomperssion after %d chars [bad index %x]. skipping...\n", inlen, index);
						UNCAST_ERRORS = 1;
					}
				}

			/* process_char_after_word1: */
				/* now to see what follows the word: a blank or a special delimiter or not-blank */
				if((c = mygetc(infp, inbuf, maxinlen, &inlen)) == MYEOF) {
					if (!TC_FOUND_NOTBLANK) {
						if (outfp != NULL) putc(' ', outfp);
						if (outbuf != NULL) outbuf[outlen] = ' ';
						outlen ++;
					}
					TC_FOUND_NOTBLANK = 0;	/* default: use result of previous forward_tcompressed_word only */
					return outlen;
				}
				else if (c < MAX_SPECIAL_CHARS) {
					if ((c < END_SPECIAL_DELIMITERS) && (c >= BEGIN_SPECIAL_DELIMITERS)) {
						if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;
						if (outfp != NULL) putc(special_delimiters[c - BEGIN_SPECIAL_DELIMITERS], outfp);
						if (outbuf != NULL) outbuf[outlen] = special_delimiters[c - BEGIN_SPECIAL_DELIMITERS];
						outlen ++;
					}
					else if (c != NOTBLANK) {
						if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;
						if (outfp != NULL) putc(' ', outfp);
						if (outbuf != NULL) outbuf[outlen] = ' ';
						outlen ++;
						goto bypass_getc1;
					}
					/* else go normal getc */
				}
				else {	/* can be one of the special_words or a dictionary index */
					if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;
					if (outfp != NULL) putc(' ', outfp);
					if (outbuf != NULL) outbuf[outlen] = ' ';
					outlen ++;
					goto above1;
				}
			}
		}
	}
	else
	{	/* compression was done in a context sensitive fashion w/o regards to search */
		while(1) {
			if((c = mygetc(infp, inbuf, maxinlen, &inlen)) == MYEOF) return outlen;

		bypass_getc2:
			if (verbatim_state) {
				if (c == END_VERBATIM) verbatim_state = 0;
				else if (c == BEGIN_VERBATIM) goto verbatim_processing;
				else {
					if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;
					if (outfp != NULL) putc(c, outfp);
					if (outbuf != NULL) outbuf[outlen] = c;
					outlen ++;
				}
			}
			else if (c < END_SPECIAL_CHARS) {
				process_special_char(c)
				if ( ((c == NEWLINE) || (c == TWONEWLINES)) && untilnewline)
				return outlen;
			}
			else if (c == BEGIN_VERBATIM) {
			verbatim_processing:
				if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;
				if((c = mygetc(infp, inbuf, maxinlen, &inlen)) == MYEOF) return outlen;
				if (outfp != NULL) putc(c, outfp);
				if (outbuf != NULL) outbuf[outlen] = c;
				outlen ++;
				if ((c!=BEGIN_VERBATIM) && (c!=END_VERBATIM)) verbatim_state = 1;	/* only _these_ are escape characters */
			}
			else if (c == END_VERBATIM) {	/* not in verbatim state, still end_verbatim! */
				verbatim_state = 0;
				fprintf(stderr, "error in decompression after %d chars [verbatim processing]. skipping...\n", inlen);
				UNCAST_ERRORS = 1;
			}
			else
			{
			above2:
				if (c < RESERVED_CHARS) {	/* this is a special-word but not a special char */
					process_special_char(c)
				}
				else {	/* it is an index of a word in the dictionary since 1st byte >= RESERVED_CHARS */
					index = c;
					index <<= 8;
					if ((c = mygetc(infp, inbuf, maxinlen, &inlen)) == MYEOF) return outlen;
					index |= c;
					dindex = decode_index(index);
					if(dindex < MAX_WORDS) {
						if ((maxoutlen >= 0) && (outlen + AVG_WORD_LEN >= maxoutlen)) return outlen;
						if (outfp != NULL) outlen += myfpcopy(outfp, compress_string_table[dindex]);
						if (outbuf != NULL) {
							outlen += mystrcpy(outbuf+outlen, compress_string_table[dindex]);
						}
						if ((maxoutlen >= 0) && (outlen >= maxoutlen)) return outlen;
					}
					else {
						fprintf(stderr, "error in decomperssion after %d chars [bad index %x]. skipping...\n", inlen, index);
						UNCAST_ERRORS = 1;
					}
				}

			/* process_char_after_word2: */
				/* now to see what follows the word: a blank or a special delimiter or not-blank */
				if((c = mygetc(infp, inbuf, maxinlen, &inlen)) == MYEOF) {
					if (!TC_FOUND_NOTBLANK) {
						if (outfp != NULL) putc(' ', outfp);
						if (outbuf != NULL) outbuf[outlen] = ' ';
						outlen ++;
					}
					TC_FOUND_NOTBLANK = 0;	/* default: use result of previous forward_tcompressed_word only */
					return outlen;
				}
				else if (c < MAX_SPECIAL_CHARS) {
					if ((c < END_SPECIAL_DELIMITERS) && (c >= BEGIN_SPECIAL_DELIMITERS)) {
						if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;
						if (outfp != NULL) putc(special_delimiters[c - BEGIN_SPECIAL_DELIMITERS], outfp);
						if (outbuf != NULL) outbuf[outlen] = special_delimiters[c - BEGIN_SPECIAL_DELIMITERS];
						outlen ++;
					}
					else if (c != NOTBLANK) {
						if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;
						if (outfp != NULL) putc(' ', outfp);
						if (outbuf != NULL) outbuf[outlen] = ' ';
						outlen ++;
						goto bypass_getc2;
					}
					/* else go normal getc */
				}
				else {	/* can be one of the special_words or a dictionary index */
					if ((maxoutlen >= 0) && (outlen + 1 >= maxoutlen)) return outlen;
					if (outfp != NULL) putc(' ', outfp);
					if (outbuf != NULL) outbuf[outlen] = ' ';
					outlen ++;
					goto above2;
				}
			}
		}
	}
}

#define FUNCTION	tuncompress_file
#define DIRECTORY	tuncompress_directory
#include "trecursive.c"

/* returns #bytes (>=0) in the uncompressed file, -1 if major error (not able to uncompress) */
int
tuncompress_file(name, outname, flags)
	char	*name;
	char	*outname;
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

	if (-1 == stat(tempname, &statbuf)) {
		if (flags & TC_ERRORMSGS)
			fprintf(stderr, "permission denied or non-existent: %s\n", tempname);
		return -1;
	}
	if (S_ISDIR(statbuf.st_mode)) {
		if (flags & TC_RECURSIVE) return tuncompress_directory(tempname, outname, flags);
		if (flags & TC_ERRORMSGS)
			fprintf(stderr, "skipping directory: %s\n", tempname);
		return -1;
	}
        if (!S_ISREG(statbuf.st_mode)) {
                if (flags & TC_ERRORMSGS)
			fprintf(stderr, "not a regular file, skipping: %s\n", tempname);
                return -1;
	}

	inlen = strlen(tempname);
	if (!tuncompressible_filename(tempname, inlen)) {
		if (!(flags & TC_RECURSIVE) && (flags & TC_ERRORMSGS))
			fprintf(stderr, "no %s extension, skipping: %s\n", COMP_SUFFIX, tempname);
		return -1;
	}
	if ((fp = fopen(tempname, "r")) == NULL) {
		if (flags & TC_ERRORMSGS)
			fprintf(stderr, "permission denied or non-existent: %s\n", tempname);
		return -1;
	}
	if (!tuncompressible_fp(fp)) {
		if (flags & TC_ERRORMSGS)
			fprintf(stderr, "signature does not match, skipping: %s\n", tempname);
		fclose(fp);
		return -1;
	}
	if (flags & TC_SILENT) {
		printf("%s\n", tempname);
		fclose(fp);
		return 0;
	}

	/* Create and open output file */
	strncpy(outname, tempname, MAX_LINE_LEN);
	outname[inlen - strlen(COMP_SUFFIX)] = '\0';
	if (!access(outname, R_OK)) {
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
		if (flags & TC_ERRORMSGS)
			fprintf(stderr, "cannot open for writing: %s\n", outname);
		fclose(fp);
		return -1;
	}

	UNCAST_ERRORS = 0;
	if ( ((ret = tuncompress(fp, -1, outfp, -1, flags)) > 0) && !UNCAST_ERRORS && (flags & TC_REMOVE)) {
		unlink(tempname);
	}

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
