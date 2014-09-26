/* Copyright (c) 1994 Burra Gopal, Udi Manber.  All Rights Reserved. */

/*
 * hash.c:	Hash table manipulation routines. Can be used to compute
 *		the dictionary as well as compress files.
 */

#include "defs.h"

int next_free_hash = 0;
hash_entry *free_hash = NULL; /*[DEF_MAX_WORDS]; */

int next_free_str = 0;
char *free_str = NULL; /*[DEF_MAX_WORDS * AVG_WORD_LEN]; */

extern int usemalloc;

/* -----------------------------------------------------------------
input: a word (a string of ascii character terminated by NULL)
output: a hash_value of the input word.
hash function: if the word has length <= 4
        the hash value is just a concatenation of the last four bits
        of the characters.
        if the word has length > 4, then after the above operation,
        the hash value is updated by adding each remaining character.
        (and AND with the 16-bits mask).
---------------------------------------------------------------- */
int
thash64k(word, len)
unsigned char *word;
int len;
{
    unsigned int hash_value=0;
    unsigned int mask_4=017;
    unsigned int mask_16=0177777;
    int i;

    if(len<=4) {
        for(i=0; i<len; i++) {
            hash_value = (hash_value << 4) | (word[i]&mask_4);
            /* hash_value = hash_value  & mask_16; */
        }
    }
    else {
        for(i=0; i<4; i++) {
            hash_value = (hash_value << 4) | (word[i]&mask_4);
            /* hash_value = hash_value & mask_16;  */
        }
        for(i=4; i<len; i++)
            hash_value = mask_16 & (hash_value + word[i]);
    }
    return(hash_value & mask_16);
}

hash_entry *
get_hash(hash_table, word, len, i)
	hash_entry *hash_table[HASH_TABLE_SIZE];
	unsigned char	*word;
	int	len;
	int	*i;
{
	hash_entry *e;

	*i = thash64k(word, len);
	e = hash_table[*i];
	while(e != NULL) {
		if (!strcmp(e->word, (char *)word)) break;
		else e = e->next;
	}
	return e;
}

/*
 * Assigns either the freq or the offset to the hash-entry. The kind of
 * information in the entry depends on the caller. Advice: different
 * hash-tables must be used to store information gathered during
 * the build operation and the compress operation by the appropriate
 * module. This can be specified by passing -1's for offset/freq resply.
 */
hash_entry *
insert_hash(hash_table, word, len, freq, offset)
	hash_entry *hash_table[HASH_TABLE_SIZE];
	unsigned char	*word;
	int	len, freq, offset;
{
	int	i;
	hash_entry *e;

	e = get_hash(hash_table, word, len, &i);

	if (e == NULL) {
		hashalloc(e);
		stralloc(e->word, len + 2);
		strcpy(e->word, (char *)word);
		e->val.offset = 0;
		e->next = hash_table[i];
		hash_table[i] = e;
	}

	if ((offset == -1) && (freq != -1)) {
		e->val.attribute.freq += freq;
		/* e->val.attribute.index has to be accessed from outside this function */
	}
	else if ((offset != -1) && (freq == -1)) {
		e->val.offset = offset;
		/* used in building the string table from the dictionary */
	}
	else {
		fprintf(stderr, "error in accessing hash-table [frequencies/offsets]. skipping...\n");
		return (NULL);
	}

#if	0
	printf("%d %x\n", i, e);
#endif	/*0*/
	return e;
}

/*
 * HASHFILE format: the hash-file is a sequence of "'\0' hash-index word-index word-name"
 * The '\0' is there to indicate that this is not a padded line. Padded lines simply have
 * a '\n' as the first character (words don't have '\0' or '\n'). The hash and word indices
 * are 2 unsigned short integers in binary, MSB first. The word name therefore starts from the
 * 5th character and continues until a '\0' or '\n' is encountered. The total size of the
 * hash-table is therefore (|avgwordlen|+5)*numwords = appx 12 * 50000 = .6MB.
 * Note that there can be multiple lines with the same hash-index.
 */

/* used when computing compress's dictionary */
int
dump_hash(hash_table, HASHFILE)
	hash_entry *hash_table[HASH_TABLE_SIZE];
	unsigned char	*HASHFILE;
{
	int	i;
	FILE	*hashfp;
	int	wordindex;
	hash_entry *e, *t;

	if ((hashfp = fopen((char *)HASHFILE, "w")) == NULL) {
		fprintf(stderr, "cannot open for writing: %s\n", HASHFILE);
		return 0;
	}

	/* We have a guarantee that the wordindex + 1 cannot exceed MAX_WORDS */
	wordindex = 0;
	for(i=0; i<HASH_TABLE_SIZE; i++) {
		e = hash_table[i];
		while (e != NULL) {
			fprintf(hashfp, "%d %d %s\n", i, wordindex, e->word);
			t = e->next;
			strfree(e->word);
			hashfree(e);
			e = t;
			wordindex ++;
		}
	}

	fclose(hashfp);
	return wordindex;
}

/*
 * These are routines that operate on hash-tables of 4K size (used in tbuild.c)
 */

/* crazy hash function that operates on 4K hashtables */
thash4k(word, len)
	char 	*word;
	int	len;
{
    unsigned int hash_value=0;
    unsigned int mask_3=07;
    unsigned int mask_12=07777;
    int i;

#if	0
    /* discard prefix = the directory name */
    if (len<=1) return 0;
    i = len-1;
    while(word[i] != '/') i--;
    if ((i > 0) && (word[i] == '/')) {
	word = &word[i+1];
	len = strlen(word);
    }
#endif	/*0*/

    if(len<=4) {
	for(i=0; i<len; i++) {
       	    hash_value = (hash_value << 3) | (word[i]&mask_3);
	}
    }
    else {
	for(i=0; i<4; i++) {
       	    hash_value = (hash_value << 3) | (word[i]&mask_3);
	}
	for(i=4; i<len; i++) 
	    hash_value = mask_12 & (hash_value + word[i]);
    }
    return(hash_value & mask_12);
}

hash_entry *
get_small_hash(hash_table, word, len, i)
	hash_entry *hash_table[SMALL_HASH_TABLE_SIZE];
	unsigned char	*word;
	int	len;
	int	*i;
{
	hash_entry *e;

	*i = thash4k(word, len);
	e = hash_table[*i];
	while(e != NULL) {
		if (!strcmp(e->word, (char *)word)) break;
		else e = e->next;
	}
	return e;
}

hash_entry *
insert_small_hash(hash_table, word, len, freq, offset)
	hash_entry *hash_table[SMALL_HASH_TABLE_SIZE];
	unsigned char	*word;
	int	len, freq, offset;
{
	int	i;
	hash_entry *e;

	e = get_small_hash(hash_table, word, len, &i);

	if (e == NULL) {
		hashalloc(e);
		stralloc(e->word, len + 2);
		strcpy(e->word, (char *)word);
		e->val.offset = 0;
		e->next = hash_table[i];
		hash_table[i] = e;
	}

	if ((offset == -1) && (freq != -1)) {
		e->val.attribute.freq += freq;
		/* e->val.attribute.index has to be accessed from outside this function */
	}
	else if ((offset != -1) && (freq == -1)) {
		e->val.offset = offset;
		/* used in building the string table from the dictionary */
	}
	else {
		fprintf(stderr, "error in accessing hash-table [frequencies/offsets]. skipping...\n");
		return (NULL);
	}

#if	0
	printf("%d %x\n", i, e);
#endif	/*0*/
	return e;
}

int
dump_small_hash(hash_table, HASHFILE)
	hash_entry *hash_table[SMALL_HASH_TABLE_SIZE];
	unsigned char	*HASHFILE;
{
	int	i;
	FILE	*hashfp;
	int	wordindex;
	hash_entry *e, *t;

	if ((hashfp = fopen((char *)HASHFILE, "w")) == NULL) {
		fprintf(stderr, "cannot open for writing: %s\n", HASHFILE);
		return 0;
	}

	/* We have a guarantee that the wordindex + 1 cannot exceed MAX_WORDS */
	wordindex = 0;
	for(i=0; i<SMALL_HASH_TABLE_SIZE; i++) {
		e = hash_table[i];
		while (e != NULL) {
			fprintf(hashfp, "%d %d %s\n", thash64k(e->word, strlen(e->word)), wordindex, e->word);	/* must look like I used 64K table */
			t = e->next;
			strfree(e->word);
			hashfree(e);
			e = t;
			wordindex ++;
		}
	}

	fclose(hashfp);
	return wordindex;
}

/*
 * These are again routines that operate on big (64k) hash-tables
 */

/* used only during debugging to see if output = input */
int
dump_hash_debug(hash_table, HASHFILE)
	hash_entry *hash_table[HASH_TABLE_SIZE];
	unsigned char	*HASHFILE;
{
	int	i;
	FILE	*hashfp;
	hash_entry *e;

	if ((hashfp = fopen((char *)HASHFILE, "w")) == NULL) {
		fprintf(stderr, "cannot open for writing: %s\n", HASHFILE);
		return 0;
	}

	/* We have a guarantee that the wordindex + 1 cannot exceed MAX_WORDS */
	for(i=0; i<HASH_TABLE_SIZE; i++) {
		e = hash_table[i];
		while (e != NULL) {
			fprintf(hashfp, "%d %d %d %s\n", i, e->val.attribute.freq, e->val.attribute.index, e->word);
			e = e->next;
		}
	}

	fclose(hashfp);
	return 1;
}

/*
 * VERY particular to the format of the hash-table file:
 * -- does an fscanf+2atoi's+strlen all in one scan.
 * Returns 0 if you are in padded are, -1 on EOF, else ~.
 */
int
myhashread(fp, pint1, pint2, str, plen)
	FILE	*fp;
	int	*pint1;
	int	*pint2;
	char	*str;
	int	*plen;
{
	int	numread;
	int	int1, int2;
	int	c;

	if((int1 = getc(fp)) == '\n') return 0;	/* padded area */
	if(int1 != 0) return -1;		/* formatting error! */
	if ((int1 = getc(fp)) == EOF) return -1;
	if ((int2 = getc(fp)) == EOF) return -1;
	*pint1 = (int1 << 8) | int2;		/* hashindex */
	if ((int1 = getc(fp)) == EOF) return -1;
	if ((int2 = getc(fp)) == EOF) return -1;
	*pint2 = (int1 << 8) | int2;		/* wordindex */

	numread = 5;
	*plen = 0;				/* wordname */
	while((c = getc(fp)) != EOF) {
		if ( (c == '\0') || (c == '\n') ){
			ungetc(c, fp);
			str[*plen] = '\0';
			return numread;
		}
		str[(*plen)++] = c;
		numread ++;
		if (numread >= MAX_NAME_LEN) {
			str[*plen - 1] = '\0';
			return numread;
		}
	}
	return -1;
}

int
tbuild_hash(hash_table, hashfp, bytestoread)
	hash_entry	*hash_table[HASH_TABLE_SIZE];
	FILE		*hashfp;
	int		bytestoread;
{
	int	hashindex;
	int	wordindex;
	int	numread = 0;
	int	ret;
	int	len;
	char	*word;
	char	dummybuf[MAX_WORD_BUF];
	hash_entry *e;

	if (bytestoread == -1) {	/* read until end of file */
		while (1)
		{
			if (usemalloc) word = dummybuf;
			else {
				if (free_str == NULL) free_str = (char *)malloc(AVG_WORD_LEN * DEF_MAX_WORDS);
				if (free_str == NULL) break;
				word = &free_str[next_free_str];
			}
			if ((ret = myhashread(hashfp, &hashindex, &wordindex, word, &len)) == 0) continue;
			if (ret == -1) break;
			if ((hashindex >= HASH_TABLE_SIZE) || (hashindex < 0)) continue;	/* ignore */
			hashalloc(e);
			if (usemalloc) {
				if ((word = (char *)malloc(len + 2)) == NULL) break;
				strcpy(word, dummybuf);
			}
			else next_free_str += len + 2;
			e->word = word;
			e->val.attribute.freq = 0;	/* just exists in compress's dict: not found in text-file yet! */
			e->val.attribute.index = wordindex;
			e->next = hash_table[hashindex];
			hash_table[hashindex] = e;
#if	0
			printf("word=%s index=%d\n", word, wordindex);
#endif	/*0*/
		}
	}
	else {	/* read only a specified number of bytes */
		while (bytestoread > numread)
		{
			if (usemalloc) word = dummybuf;
			else {
				if (free_str == NULL) free_str = (char *)malloc(AVG_WORD_LEN * DEF_MAX_WORDS);
				if (free_str == NULL) break;
				word = &free_str[next_free_str];
			}
			if ((ret = myhashread(hashfp, &hashindex, &wordindex, word, &len)) <= 0) break;
			if ((hashindex >= HASH_TABLE_SIZE) || (hashindex < 0)) continue;	/* ignore */
			hashalloc(e);
			if (usemalloc) {
				if ((word = (char *)malloc(len + 2)) == NULL) break;
				strcpy(word, dummybuf);
			}
			else next_free_str += len + 2;
			e->word = word;
			e->val.attribute.freq = 0;	/* just exists in compress's dict: not found in text-file yet! */
			e->val.attribute.index = wordindex;
			e->next = hash_table[hashindex];
			hash_table[hashindex] = e;
			wordindex ++;
			numread += ret;
#if	0
			printf("%d %d %s\n", hashindex, wordindex, word);
#endif	/*0*/
		}
	}

	return (wordindex + 1);	/* the highest indexed word + 1 */
}

/*
 * Interprets srcbuf as a series of words separated by newlines and looks
 * for a complete occurrence of words in patbuf in it. If there IS an occurrence,
 * it builds the hash-table for THAT page. The hashfp must start at the
 * beginning on each call.
 */
int
build_partial_hash(hash_table, hashfp, srcbuf, srclen, patbuf, patlen, blocksize, loaded_hash_table)
	hash_entry *hash_table[HASH_TABLE_SIZE];
	FILE	*hashfp;
	unsigned char	*srcbuf;
	int	srclen;
	unsigned char	*patbuf;
	int	patlen;
	int	blocksize;
	char	loaded_hash_table[HASH_FILE_BLOCKS];
{
	unsigned char	*srcpos;
	unsigned char	*srcinit, *srcend, dest[MAX_NAME_LEN];
	int	blockindex = 0;
	int	i, initlen, endlen;
	unsigned char	*strings[MAX_NAME_LEN];	/* maximum pattern length */
	int	numstrings = 0;
	int	inword = 0;

	/*
	 * Find all the relevant strings in the pattern.
	 */
	i = 0;
	while(i<patlen) {
		if (isalnum(patbuf[i])) {
			if (!inword) {
				strings[numstrings++] = &dest[i];
				inword = 1;
			}
			if (isupper(patbuf[i])) dest[i] = tolower(patbuf[i]);
			else dest[i] = patbuf[i];
		}
		else {
			dest[i] = '\0';	/* ignore that character */
			inword = 0;
		}
		i++;
	}
#if	0
	for (i=0; i<numstrings; i++) printf("word%d=%s\n", i, strings[i]);
	getchar();
#endif	/*0*/

	srcpos = srcbuf;
	while (srcpos < (srcbuf + srclen)) {
		srcinit = srcpos;
		initlen = strlen((char *)srcinit);
		srcend = srcinit + initlen + 1;
		endlen = strlen((char *)srcend);
#if	0
		printf("%s -- %s\n", srcinit, srcend);
#endif	/*0*/
		for (i=0; i<numstrings; i++)
			if ((strcmp((char *)strings[i], (char *)srcinit) >= 0) && (strcmp((char *)strings[i], (char *)srcend) <= 0)) goto include_page;
		blockindex++;
		srcpos += (initlen + endlen + 2);
		continue;

	include_page:	/* Include it if any of the patterns fit within this range */
		if (loaded_hash_table[blockindex++]) continue;
#if	0
		printf("build_partial_hash: hashing words in page# %d\n", blockindex);
#endif	/*0*/
		loaded_hash_table[blockindex - 1] = 1;
		fseek(hashfp, (blockindex-1)*blocksize, 0);
		tbuild_hash(hash_table, hashfp, blocksize);
		srcpos += (initlen + endlen + 2);
	}
	return 0;
}

pad_hash_file(filename, FILEBLOCKSIZE)
	unsigned char *filename;
	int FILEBLOCKSIZE;
{
	FILE	*outfp, *infp, *indexfp;
	int	offset = 0, len;
	unsigned char buf[MAX_NAME_LEN];
	int	pid = getpid();
	int	i;
	unsigned char	word[MAX_NAME_LEN];
	unsigned char	prev_word[MAX_NAME_LEN];
	unsigned int	hashindex, wordindex;
	char		es1[MAX_LINE_LEN], es2[MAX_LINE_LEN];

	if ((infp = fopen((char *)filename, "r")) == NULL) {
		fprintf(stderr, "cannot open for reading: %s\n", filename);
		exit(2);
	}
	sprintf((char *)buf, "%s.index", filename);
	if ((indexfp = fopen((const char *)buf, "w")) == NULL) {
		fprintf(stderr, "cannot open for writing: %s\n", buf);
		fclose(infp);
		exit(2);
	}
	sprintf((char *)buf, "%s.%d", filename, pid);
	if ((outfp = fopen((const char *)buf, "w")) == NULL) {
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

	if ((char*)buf != fgets((char *)buf, MAX_NAME_LEN, infp)) goto end_of_input;
	len = strlen((char *)buf);

	sscanf((const char *)buf, "%d %d %s\n", &hashindex, &wordindex, word);
	putc(0, outfp);
	putc((hashindex & 0xff00)>>8, outfp);
	putc((hashindex & 0x00ff), outfp);
	putc((wordindex & 0xff00)>>8, outfp);
	putc((wordindex & 0x00ff), outfp);
	fprintf(outfp, "%s", word);

	buf[len-1] = '\0';			/* fgets gives you the newline too */
        for (i=0; i< len; i++) if (isupper(buf[i])) buf[i] = tolower(buf[i]);
	for (i=len-2; i>=0; i--) if (buf[i] == ' ') { i++; break; }
	if (i < 0) i = 0;
	strcpy((char *)prev_word, (char *)&buf[i]);

	fprintf(indexfp, "%s", &buf[i]);	/* the first word */
	putc(0, indexfp);			/* null terminated */
	offset += strlen((char *)word)+5;

	 while(fgets((char *)buf, MAX_NAME_LEN, infp) == (char *)buf) {
		len = strlen((char *)buf);
		if (offset + len > FILEBLOCKSIZE) {
			/* Put the last char of the prev. page */
			fprintf(indexfp, "%s", prev_word);
			putc(0, indexfp);	/* null terminated */
				
			for (i=0; i<FILEBLOCKSIZE-offset; i++)	/* fill up with so many newlines until the next block size */
				putc('\n', outfp);


			sscanf((const char *)buf, "%d %d %s\n", &hashindex, &wordindex, word);
			putc(0, outfp);
			putc((hashindex & 0xff00)>>8, outfp);
			putc((hashindex & 0x00ff), outfp);
			putc((wordindex & 0xff00)>>8, outfp);
			putc((wordindex & 0x00ff), outfp);
			fprintf(outfp, "%s", word);

                        buf[len-1] = '\0';			/* fgets gives you the newline too */
                        for (i=0; i< len; i++) if (isupper(buf[i])) buf[i] = tolower(buf[i]);
                        for (i=len-2; i>=0; i--) if (buf[i] == ' ') { i++; break; }
                        if (i < 0) i = 0;
                        strcpy((char *)prev_word, (char *)&buf[i]);

			fprintf(indexfp, "%s", &buf[i]);	/* store the first word at each page */
			putc(0, indexfp);			/* null terminated */
			offset = 0;
		}
		else {
			sscanf((const char *)buf, "%d %d %s\n", &hashindex, &wordindex, word);
			putc(0, outfp);
			putc((hashindex & 0xff00)>>8, outfp);
			putc((hashindex & 0x00ff), outfp);
			putc((wordindex & 0xff00)>>8, outfp);
			putc((wordindex & 0x00ff), outfp);
			fprintf(outfp, "%s", word);

                        buf[len-1] = '\0';			/* fgets gives you the newline too */
                        for (i=0; i<len; i++) if (isupper(buf[i])) buf[i] = tolower(buf[i]);
                        for (i=len-2; i>=0; i--) if (buf[i] == ' ') { i++; break; }
                        if (i < 0) i = 0;
                        strcpy((char *)prev_word, (char *)&buf[i]);
		}
		offset += strlen((char *)word)+5;
	}
	fprintf(indexfp, "%s", prev_word);
	putc(0, indexfp);			/* null terminated */

end_of_input:
	fclose(infp);
	fflush(outfp);
	fclose(outfp);
	fflush(indexfp);
	fclose(indexfp);
	sprintf((char *)buf, "exec %s '%s.%d' '%s'\n", SYSTEM_MV, tescapesinglequote(filename, es1), pid, tescapesinglequote(filename, es2));
	system((const char *)buf);
  return(1);	/* by default this function is declared to return int, but not explictly declared, so we need to return something */
}

