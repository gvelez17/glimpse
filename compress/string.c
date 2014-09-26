/* Copyright (c) 1994 Burra Gopal, Udi Manber.  All Rights Reserved. */

/*
 * string.c:	String table manipulation routines. Can be used to compute
 *		the dictionary as well as uncompress files.
 */

#include "defs.h"

extern int MAX_WORDS;
extern int RESERVED_CHARS;

int next_free_strtable = 0;
char *free_strtable = NULL; /*[DEF_MAX_WORDS * AVG_WORD_LEN]; */

extern int usemalloc;

/* debugging only */
int
dump_string(string_table, string_file, index_file)
	char	**string_table;
	unsigned char	*string_file, *index_file;
{
	FILE	*stringfp;
	FILE	*indexfp;
	int	i;

	if ((stringfp = fopen(string_file, "w")) == NULL) {
		fprintf(stderr, "cannot open for writing: %s\n", string_file);
		return 0;
	}
	if ((indexfp = fopen(index_file, "r")) == NULL) {
		fprintf(stderr, "cannot open for reading: %s\n", index_file);
		fclose(stringfp);
		return 0;
	}

	for(i=0; i<MAX_WORDS; i++) fprintf(stringfp, "%s\n", string_table[i]);

	fflush(stringfp);
	fclose(stringfp);
	fclose(indexfp);
	return 1;
}

/*
 * VERY particular to the format of the string-table file: which is a series
 * of words separated by newlines -- this does an fscanf+strlen in one scan.
 */
int
mystringread(fp, str)
	FILE	*fp;
	char	*str;
{
	int	numread = 0;
	int	c;

	while((numread <= MAX_WORD_LEN) && ((c = getc(fp)) != EOF)) {
		if (c == '\n') {
			if (numread==0) break;	/* first char '\n' => in padded area */
			c = '\0';
			str[numread++] = c;
			return numread;
		}
		else str[numread++] = c;
	}
	str[numread] = '\0';
	if (c == EOF) return -1;
	return numread;
}

int
build_string(string_table, stringfp, bytestoread, initialwordindex)
	char	*string_table[DEF_MAX_WORDS]; /*[MAX_WORD_LEN+2]; */
	FILE	*stringfp;
	int	bytestoread;
	int	initialwordindex;
{
	int	wordindex = initialwordindex;
	int	numread = 0;
	int	ret;
	char	dummybuf[MAX_WORD_BUF];
	char	*word;

	if (bytestoread == -1) {	/* read until end of file */
		while (wordindex < MAX_WORDS) {
			if (usemalloc) word = dummybuf;
			else {
				if (free_strtable == NULL) free_strtable = (char *)malloc(AVG_WORD_LEN * DEF_MAX_WORDS);
				if (free_strtable == NULL) break;
				word = &free_strtable[next_free_strtable];
			}
			if ((ret = mystringread(stringfp, word)) == 0) continue;
			if (ret == -1) break;
			if (usemalloc) {
				if ((word = (char *)malloc(ret + 2)) == NULL) break;
				strcpy(word, dummybuf);
			}
			else next_free_strtable += ret + 2;
			string_table[wordindex] = word;
#if	0
			printf("word=%s index=%d\n", string_table[wordindex], wordindex);
#endif	/*0*/
			wordindex ++;
		}
	}
	else {	/* read only the specified number of bytes */
		while((wordindex < MAX_WORDS) && (bytestoread > numread)) {
			if (usemalloc) word = dummybuf;
			else {
				if (free_strtable == NULL) free_strtable = (char *)malloc(AVG_WORD_LEN * DEF_MAX_WORDS);
				if (free_strtable == NULL) break;
				word = &free_strtable[next_free_strtable];
			}
			if ((ret = mystringread(stringfp, word)) <= 0) break;	/* quit if EOF OR if padded area */
			if (usemalloc) {
				if ((word = (char *)malloc(ret + 2)) == NULL) break;
				strcpy(word, dummybuf);
			}
			else next_free_strtable += ret + 2;
			string_table[wordindex] = word;
#if	0
			printf("word=%s index=%d\n", string_table[wordindex], wordindex);
#endif	/*0*/
			wordindex ++;
			numread += ret;
		}
	}
	return wordindex;
}

/*
 * Interprets srcbuf as a set of srclen/2 short integers. It looks for all the
 * short-integers encoding words in the matched line and loads only those blocks
 * of the string table. Note: srcbuf must be aligned on a short-int boundary.
 */
int
build_partial_string(string_table, stringfp, srcbuf, srclen, linebuf, linelen, blocksize, loaded_string_table)
	char	*string_table[DEF_MAX_WORDS]; /* [MAX_WORD_LEN+2]; */
	FILE	*stringfp;
	unsigned char	*srcbuf;
	int	srclen;
	unsigned char	*linebuf;
	int	linelen;
	int	blocksize;
	char	loaded_string_table[STRING_FILE_BLOCKS];
{
	unsigned char	*srcpos;
	int		blockindex = 0;
	unsigned short	srcinit, srcend;
	unsigned short	wordnums[MAX_NAME_LEN];	/* maximum pattern length */
	int	numwordnums = 0;
	int	i;

	/*
	 * Find all the relevant wordnums in the line.
	 */
	i = 0;
	while(i<linelen) {
		if (linebuf[i] < RESERVED_CHARS) {
			if (linebuf[i] == BEGIN_VERBATIM) {
				if (ISASCII(linebuf[i+1])) {
					while ((linebuf[i] != END_VERBATIM) && (i <linelen)) i ++;
				}
				else i ++;	/* skip over the BEGIN_VERBATIM of non-ascii character */
				i ++;		/* skip over the non-ascii character OR END_VERBATIM: let it overshoot linelen...its ok */
			}
			else i ++;		/* skip over the character encoding a special word OR a special character */
		}
		else {
			wordnums[numwordnums] = (unsigned char)linebuf[i];		/* always big-endian compression */
			wordnums[numwordnums] <<= 8;
			wordnums[numwordnums] |= (unsigned char)linebuf[i+1];
			wordnums[numwordnums] = decode_index(wordnums[numwordnums]);	/* roundabout to avoid buserr */
			numwordnums ++;
			i += sizeof(short);
		}
	}

#if	0
	for (i=0; i<numwordnums; i++) printf("num%d=%d\n", i, wordnums[i]);
	getchar();
#endif	/*0*/

	srcpos = srcbuf;
	srcend = *((unsigned short *)srcpos);
	srcpos += sizeof(short);
	while (srcpos < srcbuf + srclen) {
		srcinit = srcend;
		srcend = *((unsigned short *)srcpos);
		srcpos += sizeof(short);
#if	0
		printf("%d -- %d\n", srcinit, srcend);
#endif	/*0*/
		for (i=0; i<numwordnums; i++)
			if ((wordnums[i] >= srcinit) && (wordnums[i] <= srcend)) goto include_page;

		blockindex++;
		continue;

	include_page:	/* Include it if any of the word-indices fit within this range */
		if (loaded_string_table[blockindex++]) continue;
#if	0
		printf("build_partial_string: hashing words in page# %d\n", blockindex);
#endif	/*0*/
		loaded_string_table[blockindex - 1] = 1;
		fseek(stringfp, (blockindex-1)*blocksize, 0);
		build_string(string_table, stringfp, blocksize, srcinit);
	}
	return 0;
}

pad_string_file(filename, FILEBLOCKSIZE)
	unsigned char *filename;
	int FILEBLOCKSIZE;
{
	FILE	*outfp, *infp, *indexfp;
	int	offset = 0, len;
	unsigned char buf[MAX_NAME_LEN];
	int	pid = getpid();
	int	i;
	unsigned short	wordindex = 0;
	char	es1[MAX_LINE_LEN], es2[MAX_LINE_LEN];

	if ((infp = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "cannot open for reading: %s\n", filename);
		exit(2);
	}
	sprintf(buf, "%s.index", filename);
	if ((indexfp = fopen(buf, "w")) == NULL) {
		fprintf(stderr, "cannot open for writing: %s\n", buf);
		fclose(infp);
		exit(2);
	}
	sprintf(buf, "%s.%d", filename, pid);
	if ((outfp = fopen(buf, "w")) == NULL) {
		fprintf(stderr, "cannot open for writing: %s\n", buf);
		fclose(infp);
		fclose(indexfp);
		exit(2);
	}
	if ((FILEBLOCKSIZE % MIN_BLOCKSIZE) != 0) {
		fprintf(stderr, "invalid block size %d: changing to %d\n", FILEBLOCKSIZE, MIN_BLOCKSIZE);
		FILEBLOCKSIZE = MIN_BLOCKSIZE;
	}
	fprintf(indexfp, "%d\n", FILEBLOCKSIZE);

	buf[0] = '\0';
	if ((char *)buf != fgets(buf, MAX_NAME_LEN, infp)) goto end_of_input;
	len = strlen((char *)buf);
	fputs(buf, outfp);
	fprintf(indexfp, "%d\n", wordindex);
	offset += len;
	wordindex ++;

	while(fgets(buf, MAX_NAME_LEN, infp) == (char *)buf) {
		len = strlen((char *)buf);
		if (offset + len > FILEBLOCKSIZE) {
			for (i=0; i<FILEBLOCKSIZE-offset; i++)	/* fill up with so many newlines until the next block size */
				putc('\n', outfp);
			fputs(buf, outfp);
			fprintf(indexfp, "%d\n", wordindex);
			offset = 0;
		}
		else fputs(buf, outfp);
		offset += len;
		wordindex ++;
	}
	fprintf(indexfp, "%d\n", wordindex);

end_of_input:
	fclose(infp);
	fflush(outfp);
	fclose(outfp);
	fflush(indexfp);
	fclose(indexfp);
	sprintf(buf, "exec %s %s.%d %s\n", SYSTEM_MV, tescapesinglequote(filename, es1), pid, tescapesinglequote(filename, es2));
	system(buf);
	return 0;
}
