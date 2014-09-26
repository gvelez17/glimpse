/* Copyright (c) 1994 Burra Gopal, Udi Manber.  All Rights Reserved. */

/*
 * quick.c:	Used to search for a pattern in a compressed file.
 *
 * Algorithm: if the file (or stdin) is a compressed file, then:
 * +  a. Read in the hash-table-index file.
 * +  b. For each page in which the words of the pattern can be found:
 *	 build the hash-table using the words in exactly those pages.
 * +  c. Now, call compress with the given pattern.
 *
 * +  d. Call the normal search routines with the compressed pattern on
 *	 the input file.
 * +  e. If the option is to count number of matches, just exit.
 *	 Otherwise we have to modify the r_output/output routines:
 *
 * +  f. Read in the string-table-index file.
 * +  g. For each page in which the word numbers of the input file can
 *	 be found: build the string-table using the words in exactly
 *	 those pages.
 * +  h. Call uncompress with the input file line to be output and
 *	 output THIS line instead of the original matched line.
 *
 * Part of this will be in agrep and part of this here.
 */

#include "defs.h"
#include <sys/types.h>
#include <sys/stat.h>

/*
 * The quick-functions can be called multiple number of times --
 * they however open the hash, string and freq files only once.
 */

hash_entry *compress_hash_table[HASH_TABLE_SIZE];	/* used for compress: assume it is zeroed by C */
char	loaded_hash_table[HASH_FILE_BLOCKS];		/* bit mask of loaded pages in hash-table: store chars since just 4K: speed is most imp. */
char	*hashindexbuf;
int	hashindexsize;

/* returns length of compressed pattern after filling up the compressed pattern in the user-supplied newpattern buffer */
int
quick_tcompress(freq_file, hash_file, pattern, len, newpattern, maxnewlen, flags)
	char	*freq_file;
	char	*hash_file;
	CHAR	*pattern;
	int	len;
	void	*newpattern;	/* can be FILE* or CHAR* */
	int	*maxnewlen;
	int	flags;
{
	static FILE	*hashfp = NULL, *hashindexfp = NULL;
	static char	old_freq_file[MAX_LINE_LEN] = "", old_hash_file[MAX_LINE_LEN] = "";
	static int	blocksize;
	int		newlen;

	if ((hashfp == NULL) || (strcmp(freq_file, old_freq_file)) || (strcmp(hash_file, old_hash_file)))
	{	/* Have to do some initializations */
		char	s[256];
		struct stat statbuf;

		if (hashfp != NULL) {
			uninitialize_tcompress();
			fclose(hashfp);
			hashfp = NULL;
		}
		else memset(loaded_hash_table, '\0', HASH_FILE_BLOCKS);
		if (!initialize_common(freq_file, flags)) return 0;	/* don't call initialize_tcompress since that will load the FULL hash table */

		if ((hashfp = fopen(hash_file, "r")) == NULL) {
			if (flags & TC_ERRORMSGS) {
				fprintf(stderr, "cannot open cast-dictionary file: %s\n", hash_file);
				fprintf(stderr, "(use -H to give a dictionary-dir or run 'buildcast' to make a dictionary)\n");
			}
			return 0;
		}

		sprintf(s, "%s.index", hash_file);
		if ((hashindexfp = fopen(s, "r")) == NULL) {
			if (flags & TC_ERRORMSGS)
				fprintf(stderr, "cannot open for reading: %s\n", s);
			fclose(hashfp);
			hashfp = NULL;
			return 0;
		}
		blocksize = 0;
		fscanf(hashindexfp, "%d\n", &blocksize);
		if (blocksize == 0) blocksize = DEF_BLOCKSIZE;

		if (fstat(fileno(hashindexfp), &statbuf) == -1) {
			fprintf(stderr, "error in quick_tcompress/fstat on '%s.index'\n", hash_file);
			fclose(hashfp);
			hashfp = NULL;
			fclose(hashindexfp);
			hashindexfp = NULL;
			return 0;
		}

		if ((hashindexbuf = (char *)malloc(statbuf.st_size + 1)) == NULL) {
			if (flags & TC_ERRORMSGS)
				fprintf(stderr, "quick_tcompress: malloc failure!\n");
			fclose(hashfp);
			hashfp = NULL;
			fclose(hashindexfp);
			hashindexfp = NULL;
			return 0;
		}

		if ((hashindexsize = fread(hashindexbuf, 1, statbuf.st_size, hashindexfp)) == -1) {
			fprintf(stderr, "error in quick_tcompress/fread on '%s.index'\n", hash_file);
			fclose(hashfp);
			hashfp = NULL;
			fclose(hashindexfp);
			hashindexfp = NULL;
			return 0;
		}
		hashindexsize ++;	/* st_size - bytes used up for blocksize in file + 1 <= st_size */
		hashindexbuf[hashindexsize] = '\0';
		fclose(hashindexfp);

		strcpy(old_freq_file, freq_file);
		strcpy(old_hash_file, hash_file);
	}
	else rewind(hashfp);	/* Don't do it first time */

	if (pattern[len-1] == '\0') len--;
	build_partial_hash(compress_hash_table, hashfp, hashindexbuf, hashindexsize, pattern, len, blocksize, loaded_hash_table);
	newlen = tcompress(pattern, len, newpattern, maxnewlen, flags);
#if	0
	printf("quick_tcompress: pat=%s len=%d newlen=%d newpat=", pattern, len, newlen);
	for (i=0; i<newlen; i++) printf("%d ", newpattern[i]);
	printf("\n");
#endif	/*0*/
	return newlen;
}

char	*compress_string_table[DEF_MAX_WORDS]; /*[MAX_WORD_LEN+2]; */
char	loaded_string_table[STRING_FILE_BLOCKS];		/* bit mask of loaded pages in string-table: store chars since just 4K: speed is most imp. */
char	*stringindexbuf;
int	stringindexsize;

/* returns length of uncompressed pattern after filling up the uncompressed pattern in the user-supplied newpattern buffer */
int
quick_tuncompress(freq_file, string_file, pattern, len, newpattern, maxnewlen, flags)
	char	*string_file;
	char	*freq_file;
	CHAR	*pattern;
	int	len;
	void	*newpattern;	/* can be FILE* or CHAR* */
	int	*maxnewlen;
	int	flags;
{
	static FILE	*stringfp = NULL, *stringindexfp = NULL;
	static char	old_freq_file[MAX_LINE_LEN] = "", old_string_file[MAX_LINE_LEN] = "";
	static int	blocksize;
	int		newlen;
	int		dummy;

	if ((stringfp == NULL) || (strcmp(freq_file, old_freq_file)) || (strcmp(string_file, old_string_file)))
	{	/* Have to do some initializations */
		char	s[256];
		struct stat statbuf;

		if (stringfp != NULL) {
			uninitialize_tuncompress();
			fclose(stringfp);
			stringfp = NULL;
		}
		else memset(loaded_string_table, '\0', STRING_FILE_BLOCKS);
		if (!initialize_common(freq_file, flags)) return 0;	/* don't call initialize_tuncompress since that will load the FULL string table */

		if ((stringfp = fopen(string_file, "r")) == NULL) {
			if (flags & TC_ERRORMSGS) {
				fprintf(stderr, "cannot open cast-dictionary file: %s\n", string_file);
				fprintf(stderr, "(use -H to give a dictionary-dir or run 'buildcast' to make a dictionary)\n");
			}
			return 0;
		}

		sprintf(s, "%s.index", string_file);
		if ((stringindexfp = fopen(s, "r")) == NULL) {
			if (flags & TC_ERRORMSGS)
				fprintf(stderr, "cannot open for reading: %s\n", s);
			fclose(stringfp);
			stringfp = NULL;
			return 0;
		}
		blocksize = 0;
		fscanf(stringindexfp, "%d\n", &blocksize);
		if (blocksize == 0) blocksize = DEF_BLOCKSIZE;

		if (fstat(fileno(stringindexfp), &statbuf) == -1) {
			fprintf(stderr, "error in quick_tuncompress/fstat on '%s.index'\n", string_file);
			fclose(stringfp);
			stringfp = NULL;
			fclose(stringindexfp);
			stringindexfp = NULL;
			return 0;
		}

		if ((stringindexbuf = (char *)malloc(statbuf.st_size + 1)) == NULL) {
			if (flags & TC_ERRORMSGS)
				fprintf(stderr, "quick_tuncompress: malloc failure!\n");
			fclose(stringfp);
			stringfp = NULL;
			fclose(stringindexfp);
			stringindexfp = NULL;
			return 0;
		}

		stringindexsize = 0;
		while(fscanf(stringindexfp, "%d\n", &dummy) == 1) {
			*((unsigned short *)(stringindexbuf+stringindexsize)) = (unsigned short)dummy;
			stringindexsize+=sizeof(unsigned short);
		}
		fclose(stringindexfp);

		strcpy(old_freq_file, freq_file);
		strcpy(old_string_file, string_file);
	}
	else rewind(stringfp);

	build_partial_string(compress_string_table, stringfp, stringindexbuf, stringindexsize, pattern, len, blocksize, loaded_string_table);
	newlen = tuncompress(pattern, len, newpattern, maxnewlen, flags);
#if	0
	printf("quick_tuncompress: len=%d newlen=%d newpat=%s pat=", len, newlen, newpattern);
	for (i=0; i<len; i++) printf("%d ", pattern[i]);
	printf("\n");
#endif	/*0*/
	return newlen;
}

