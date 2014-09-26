/* Copyright (c) 1994 Sun Wu, Udi Manber, Burra Gopal.  All Rights Reserved. */
/* ./glimpse/index/filetype.c */
/* --------------------------------------------------------------------------
   this function detect whether a given file is of special type
   which we do not want to index.
   if so, then return(1) else return (0).
   a file is said to be binary if more than 10% of character > 128
   in the sampled input.
   a file is a uuencoded file if (maybe after mail header), there is
   a "begin" followed by 3 digits, and no lower case character.

   statistics we are concerned of:
   1) average word length: should not be greater than 10.
   2) index density: (the number of different words v.s. number of words).

-----------------------------------------------------------------------------*/
#include "glimpse.h"
#define SAMPLE_SIZE  8192
#define EXTRACT_SAMPLE_SIZE (MAX_LINE_LEN*2)	/* must be lesser than above: used to get info to be stored ALONG with filename */	/* suggested fix: ldrolez@usa.net */
#define WORD_THRESHOLD  18  /* the ratio between number of characters and
		delimiters (blanks or \n) above which the file is determined to be
		hqx or other non-natural language text */

#if	BG_DEBUG
extern	FILE	*LOGFILE;
#endif	/*BG_DEBUG*/
char *member[MAX_4K_HASH];
int member_tag[MAX_4K_HASH];
int  file_id;
extern  char *getword();
extern char INDEX_DIR[MAX_LINE_LEN];
extern int ExtractInfo;
extern int InfoAfterFilename;

char *extract_info_suffix[] = EXTRACT_INFO_SUFFIX;

/*
 * dosuffix > 0 => processes suffixes (build_in.c after filtering);
 * dosuffix > 0 but != 1 => processes suffixes only (IndexEverything, dir.c where we don't want to read files);
 * dosuffix == 0 => processes other ad-hoc file checks (Default, dir.c where we want to discard un-indexable files).
 */
int
filetype(name, dosuffix, xinfo_len, xinfo)
char *name;
int dosuffix;
int *xinfo_len;	/* length of information extracted */
char xinfo[MAX_LINE_LEN];	/* atmost 1K info can be extracted */
{
	unsigned char buffer[SAMPLE_SIZE+1];
	int num_read;
        int BINARY=0;
        int UUENCODED=0;
	int fd;
	int i, name_len = strlen(name);
	int extract_only = 0;
	char name_buffer[MAX_LINE_LEN];
	char *tempname;

	if (InfoAfterFilename || ExtractInfo) {
		special_get_name(name, name_len, name_buffer);
		tempname = name_buffer;
	}
	else tempname = name;
	name_len = strlen(tempname);

/* printf("\tname=%s dosuffix=%d xinfo_len=%x *=%d\n", tempname, dosuffix, xinfo_len, (xinfo_len == NULL) ? -1 : *xinfo_len); */
	if (xinfo_len != NULL) *xinfo_len = 0;
	if (!dosuffix) goto nosuffix;
	if (!strcmp(COMP_SUFFIX, &tempname[name_len-strlen(COMP_SUFFIX)]))
		return 0;
	if (test_special_suffix(tempname)) {
/* printf("\t\tspecial suffix \n"); */
#if	BG_DEBUG
		fprintf(LOGFILE, "special suffix: %s -- not indexing\n", name);
#endif	/*BG_DEBUG*/
		return 1;
	}
	if (dosuffix != 1) {
		if (!ExtractInfo || (xinfo_len == NULL) || (xinfo == NULL)) return 0;
		extract_only = 1;
	}

nosuffix:

	if((fd = my_open(tempname, 0)) < 0) {
		/* This is the only thing the user might want to know: suppress other warnings */
		fprintf(stderr, "permission denied or non-existent file: %s\n", name);
		return(1);
	}
        if ((num_read = read(fd, buffer, extract_only?EXTRACT_SAMPLE_SIZE:SAMPLE_SIZE)) <= 0) {
#if	BG_DEBUG
		fprintf(LOGFILE, "no data: %s -- not indexing\n", name);
#endif	/*BG_DEBUG*/
		close(fd);
		return 1;
	}

	if (extract_only) goto extract;

	if (test_postscript(buffer, num_read)) {
/* printf("\t\tpostscript\n"); */
#if	BG_DEBUG
		fprintf(LOGFILE, "postscript file: %s -- not indexing\n", name);
#endif	/*BG_DEBUG*/
		close(fd);
		return 1;
	}

        BINARY = test_binary(buffer, num_read);
        if(BINARY == ON) {
/* printf("\t\tbinary\n"); */
#if	BG_DEBUG
		fprintf(LOGFILE, "binary file: %s -- not indexing\n", name);
#endif	/*BG_DEBUG*/
		close(fd);
		return(1);
	}

	/* now check for uuencoded file */
        UUENCODED = test_uuencode(buffer, num_read);
        if(UUENCODED == ON) {
/* printf("\t\tuuencoded\n"); */
#if	BG_DEBUG
		fprintf(LOGFILE, "uuencoded file: %s -- not indexing\n", name);
#endif	/*BG_DEBUG*/
		close(fd);
		return(1);
	}
	if(heavy_index(tempname, buffer, num_read)) { 
/* printf("\t\theavy_index\n"); */
#if	BG_DEBUG
		fprintf(LOGFILE, "heavy index file: %s -- not indexing\n ", name);
#endif	/*BG_DEBUG*/
		close(fd);
		return(1);
	}
	if(hqx(tempname, buffer, num_read)) { 
/* printf("\t\thqx\n"); */
#if	BG_DEBUG
		fprintf(LOGFILE, "too few real words: %s -- not indexing\n", name);
#endif	/*BG_DEBUG*/
		close(fd);
		return(1);
	}

extract:
	if (ExtractInfo && (xinfo_len != NULL) && (xinfo != NULL)) {
		/* This can be replaced by checks for <HTML> in the file somewhere, but suffixes are faster and easier and enough in most cases */
		for (i=0; i<NUM_EXTRACT_INFO_SUFFIX; i++) {
			if (!strcasecmp(&tempname[name_len - strlen(extract_info_suffix[i])], extract_info_suffix[i])) break;
		}
		*xinfo_len = 0;
		if (i < NUM_EXTRACT_INFO_SUFFIX) {
			*xinfo_len = extract_info(tempname, buffer, num_read, i, xinfo, MAX_LINE_LEN);
		} else {
			xinfo[0] = FILE_END_MARK;
			xinfo[1] = '\0';
			*xinfo_len = 2;
		}
/* printf("\t\ti=%d extracted %d\n", i, *xinfo_len); */
	}

	close(fd);
	return(0);
}

/* This does not look at "suffix_index": it is possible to extract different things for different files: they are displayed after name of file in glimpse */
int
extract_info(name, buffer, num_read, suffix_index, xinfo, max_len)
	char	*name, *buffer, *xinfo;
	int	num_read, suffix_index, max_len;
{
	int	i=0, j=0, k=0, found_begin = 0;
	static char notitle[16];
	static char *begin = "<title>", *end = "</title>";
	static int begin_len, end_len;
	static char tr[256];
	static int first_time = 1;

	if (first_time) {
		begin_len = strlen(begin);
		end_len = strlen(end);
		for (i=0; i<256; i++)
			tr[i] = i;
		for (i=0; i<256; i++)
			if (isupper(i)) tr[i] = tr[tolower(i)];

   		/* We need xinfo to start with a dividing character, usually space or tab */
		notitle[0] =  FILE_END_MARK;
		notitle[1] = '\0';
		strcat(notitle,"No Title");

		first_time = 0;
	}

	i = 0;
	buffer[num_read] = '\0';
	while (i<=num_read-begin_len) {
		if (buffer[i] != '<') {
			i++;
			continue;
		}
		for (j=0; j<begin_len; j++)
			if (tr[buffer[j+i]] != tr[begin[j]]) break;
		if (j < begin_len) {
			i ++;
			continue;
		}
		i += j;
		while ((buffer[i] == '\0') || (buffer[i] == '\n')) i++;
		found_begin = 1;
		break;
	}
	if (!found_begin) {
		k = strlen(notitle);
		strncpy(xinfo, notitle, max_len);
		xinfo[max_len-1] = '\0';
/* printf("-X on %s --> %s\n", name, xinfo); */
		return k;
	}
	k = 0;
	xinfo[k++] = FILE_END_MARK;   /* We need xinfo to start with a dividing character, usually space or tab */
	/* There was a hard to find off-by-one error here that caused random
	   crashes. We need an extra byte for the terminating 0, so loop
	   only up to max_len - 1.
	   - CV 01/10/00 */
	while ((i<num_read) && (k<max_len  - strlen(name) - 3)) {
		if (buffer[i] != '<') {
			if ((buffer[i] == '\0') || (buffer[i] == '\n')) {
				xinfo[k++] = ' ';	/* must convert whole title to one line */
				i++;
			}
			else if (buffer[i] == ':') {	/* maybe change : to HTMML ascii character rep of : ?? ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ */
				xinfo[k++] = '\\';
				xinfo[k++] = buffer[i++];
			}
			else xinfo[k++] = buffer[i++];
			continue;
		}
		for (j=0; j<end_len; j++)
			if (tr[buffer[j+i]] != tr[end[j]]) break;
		if (j < end_len) {
			if ((buffer[i] == '\0') || (buffer[i] == '\n')) {
				xinfo[k++] = ' ';
				i++;
			}
			else if (buffer[i] == ':') {	/* maybe change : to HTMML ascii character rep of : ?? ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ */
				xinfo[k++] = '\\';
				xinfo[k++] = buffer[i++];
			}
			else xinfo[k++] = buffer[i++];
			continue;
		}
		/* found_end ; forget about i */
		break;
	}
	if (k <= 1) {
		k = strlen(notitle);
		strncpy(xinfo, notitle, max_len);
		xinfo[max_len-1] = '\0';
/* printf("-X on %s --> %s\n", name, xinfo); */
		return k;
	}
	xinfo[k] = '\0';
/* printf("-X on %s --> %s\n", name, xinfo); */
	return k;
}

/* ----------------------------------------------------------------------
   check for heavy index file.
   the function first test block 1 (of SAMPLE_SIZE bytes).
   the file is determined to be heavy index file if
   index_ratio > 0.9 and num_words > 500
   ???
---------------------------------------------------------------------- */
heavy_index(name, buffer, num_read)
char *name;
char *buffer;
int num_read;
{
	char *buffer_end;
	int hash_value;
	int new_word_num=0;
	int word_num=0;
	char word[256];

	buffer_end = &buffer[num_read];
	while((buffer = getword(name, word, buffer, buffer_end, NULL, NULL)) < buffer_end) {
		if(word[0] == '\0') continue;
		word_num++;
		hash_value = hash4k(word, strlen(word));
		if(member_tag[hash_value] != file_id) {
			new_word_num++;
			member_tag[hash_value] = file_id;
		}
	}
	if(new_word_num * 100 >= word_num * 83 && word_num >= 500) return(1);
#ifdef debug
	printf("%s: new_word_num=%d, word_num=%d\n", name, new_word_num, word_num);
#endif
	return(0);
}

/* ----------------------------------------------------------------------
   check for hqx encoded files or other files with long lines,
   for example, postscript files, core files, and others.
   the function first test block 1 (of SAMPLE_SIZE bytes).
   the file is determined to be bad if the ratio of blanks or newlines
   is too small.
---------------------------------------------------------------------- */

hqx(name, buffer, num_read)
char *name;
char *buffer;
int num_read;
{
int i;
char c;
int sep=0;
	if (num_read < 2048) return(0) ;
	for (i=0; i < num_read ; i++) {
		c=buffer[i];
		if (c == '\n' || c == ' ' || c == '/') sep++;
	/* the '/' is for list of file names. */
	/* the \n is for lists of words, but should be excluded really so
		that dictionaries are excluded */
	}
	if (!sep) return(1);
	if (num_read/sep > WORD_THRESHOLD) return(1);
		else return(0);
} 
