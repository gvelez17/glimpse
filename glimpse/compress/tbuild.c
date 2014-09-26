/* Copyright (c) 1994 Burra Gopal, Udi Manber.  All Rights Reserved. */

/*
 * build.c: 	Reads a file = list of indices and hashes frequently used
 *		words into a hash-table for fast retrieval of a word's no.
 *		Also maintains a string-table to get to the word from it no.
 */

#include "defs.h"
/* small tables are ok since computation is a 1-time job: plus we need the extra space if we put it in a library for glimpseindex */
extern hash_entry *get_small_hash(), *insert_small_hash();

hash_entry *dict_hash_table[SMALL_HASH_TABLE_SIZE];
hash_entry *freq_strings_table[MAX_WORD_LEN+2];
int freq_strings_num[MAX_WORD_LEN+2];
extern int MAX_WORDS;
extern int RESERVED_CHARS;

/*
 * Computes the dictionary for the compression routines: they need two things:
 * 1. A hash-table which maps words to word numbers.
 * 2. A string-table which maps word numbers to offsets where the word begins
 *    in the input file to this program (default DEF_INDEX_FILE).
 * They are o/p in two output files, hash_file and string_file (default).
 *
 * Algorithm: first build the hash table of atmost 65536 words. Then traverse
 * the table and output the hash/string files in the above format.
 */
int
compute_dictionary(THRESHOLD, FILEBLOCKSIZE, SPECIAL_WORDS, comp_dir)
	int	THRESHOLD, FILEBLOCKSIZE;
	char	comp_dir[];
{
	int	c;
	unsigned char	curline[MAX_NAME_LEN];
	int	curchar = 0;
	unsigned char	curword[MAX_NAME_LEN];
	int	curfreq;
	int	curoffset = 0;
	int	curlen;
	int	wordindex = 0;
	FILE	*fp, *freqfp, *tempfp, *awkfp;
	int	pid = getpid();
	int	i;
	hash_entry *e, **t, *p;
	int	dummy, offset, len;
	unsigned char	s[MAX_LINE_LEN];
	unsigned char	rands[MAX_LINE_LEN];
	char	index_file[MAX_LINE_LEN], string_file[MAX_LINE_LEN], hash_file[MAX_LINE_LEN], freq_file[MAX_LINE_LEN], es1[MAX_LINE_LEN], es2[MAX_LINE_LEN];

        strcpy(hash_file, comp_dir);
        strcat(hash_file, "/");
        strcat(hash_file, DEF_HASH_FILE);
        strcpy(freq_file, comp_dir);
        strcat(freq_file, "/");
        strcat(freq_file, DEF_FREQ_FILE);
        strcpy(string_file, comp_dir);
        strcat(string_file, "/");
        strcat(string_file, DEF_STRING_FILE);
        strcpy(index_file, comp_dir);
        strcat(index_file, "/");
        strcat(index_file, DEF_INDEX_FILE);

	memset(dict_hash_table, '\0', sizeof(hash_entry *) * SMALL_HASH_TABLE_SIZE);
	memset(freq_strings_table, '\0', sizeof(hash_entry *) * (MAX_WORD_LEN+2));
	memset(freq_strings_num, '\0', sizeof(int) * (MAX_WORD_LEN+2));
	if (THRESHOLD < 0) THRESHOLD = DEF_THRESHOLD;
	if (SPECIAL_WORDS < 0) SPECIAL_WORDS = DEF_SPECIAL_WORDS;
	if (FILEBLOCKSIZE < 0) FILEBLOCKSIZE = DEF_BLOCKSIZE;

	if (THRESHOLD < MIN_WORD_LEN || THRESHOLD > MAX_THRESHOLD) {
		fprintf(stderr, "threshold must be in [%d, %d]\n", MIN_WORD_LEN, MAX_THRESHOLD);
		return -1;
	}

	if ((SPECIAL_WORDS < 0) || (SPECIAL_WORDS > 256-MAX_SPECIAL_CHARS)) {
		fprintf(stderr, "invalid special words %d: must be in [0, %d]\n", SPECIAL_WORDS, 256-MAX_SPECIAL_CHARS);
		return -1;
	}
	RESERVED_CHARS = SPECIAL_WORDS + MAX_SPECIAL_CHARS;
	MAX_WORDS = MAX_LSB*(256-RESERVED_CHARS);

	if ((fp = fopen(index_file, "r")) == NULL) {
		fprintf(stderr, "cannot open for reading: %s\n", index_file);
		return -1;
	}

	sprintf(s, "/tmp/temp%d", pid);
	if ((tempfp = fopen(s, "w")) == NULL) {
		fprintf(stderr, "cannot open for writing: %s\n", s);
		fclose(fp);
		return -1;
	}

	while((c = getc(fp)) != EOF) {
		if (curchar >= MAX_NAME_LEN) {
			curchar = 0;
			while((c = getc(fp) != '\n') && (c != EOF));
			if (c == EOF) break;
			else continue;
		}
		curline[curchar++] = (unsigned char)c;
		if (c == '\n') {	/* reached end of record */
			int	i = 0;

			if (curline[0] == '%') {	/* initial lines */
				curchar = 0;
				continue;
			}
			curword[0] = '\0';
			while ((i<curchar) && (curline[i] != WORD_END_MARK)) {
				curword[i] = curline[i]; i++;
			}
			curword[i++] = '\0';	/* skip over WORD_END_MARK */
			curlen = strlen((char *)curword);

			curfreq = 0;
			while(i<curchar) {
				unsigned char tempbuf[MAX_NAME_LEN];
				int j = 0;

				while(isdigit(curline[i])) tempbuf[j++] = curline[i++];
				tempbuf[j] = '\0';
				curfreq += atoi(tempbuf);
				i ++;	/* skip over current ' ' or '\n' */
			}
#if	0
			printf("curlen=%d curfreq=%d\n", curlen, curfreq);
#endif	/*0*/

			if ((curlen >= MIN_WORD_LEN) && (curlen*curfreq >= THRESHOLD)) {
				fprintf(tempfp, "%d %d %s\n", curlen*curfreq, curoffset, curword);
				wordindex ++;
			}
			curoffset += curchar;	/* offset cannot begin at 0: .index_list starts with %% and some junk */
			curchar = 0;
#if	0
			printf("word=%s freq=%d\n", curword, curfreq);
#endif	/*0*/
		}
	}
	fclose(fp);

	/*
	 * Now chose MAX_WORDS words with highest frequency.
	 */
	fflush(tempfp);
	fclose(tempfp);
	if (wordindex <= SPECIAL_WORDS) {
		fprintf(stderr, "Warning: very small dictionary with only %d words!\n", wordindex);
	}
	sprintf(s, "exec %s -n -r /tmp/temp%d > /tmp/sort%d\n", SYSTEM_SORT, pid, pid);
	system(s);
	sprintf(s, "exec %s /tmp/temp%d\n", SYSTEM_RM, pid);
	system(s);
	sprintf(s, "exec %s -%d /tmp/sort%d > /tmp/temp%d\n", SYSTEM_HEAD, MAX_WORDS, pid, pid);
	system(s);

	/*
	 * The first ultra-frequent 32 words are stored in a separate table sorted by
	 * lengths and within that according to alphabetical order (=canonical order).
	 */
	sprintf(s, "/tmp/temp%d", pid);
	if ((tempfp = fopen(s, "r")) == NULL) {
		fprintf(stderr, "cannot open for reading: %s\n", s);
		return -1;
	}
	for (i=0; i<SPECIAL_WORDS; i++) {
		if (3 != fscanf(tempfp, "%d %d %s\n", &dummy, &offset, s))
			break;
		len = strlen((char *)s);
		e = (hash_entry *)malloc(sizeof(hash_entry));
		e->word = (char *)malloc(len + 2);
		e->next = NULL;
		strcpy(e->word, (char *)s);
		/* I'm not worried about the offsets now */
		t = &freq_strings_table[len];
		while(*t != NULL) {
			if (strcmp((char *)s, (*t)->word) < 0) {
				e->next = *t;
				break;
			}
			t = &(*t)->next;
		}
		*t = e;	/* valid in both cases */
		freq_strings_num[len]++;
	}

	/*
	 * Put all the other words in the hash/string tables
	 */
	for (; i<MAX_WORDS; i++) {
		if (3 != fscanf(tempfp, "%d %d %s", &dummy, &offset, s)) break;
		insert_small_hash(dict_hash_table, s, strlen((char *)s), -1, offset);	/* dummy doesn't matter now: its is just a computed-value for sort */
	}
	fclose(tempfp);

	/*
	 * At this point, the hash-table and freq-words-table have been computed properly: dump them
	 */
	if ((freqfp = fopen(freq_file, "w")) == NULL) {
		fprintf(stderr, "cannot open for writing: %s\n", freq_file);
		return -1;
	}
	/* random number signature for this set of dictionaries will be in the freq-file (.glimpse_quick) */
	srand(getpid());
	rands[0] = '\0';
	while (strlen((char*)rands) < SIGNATURE_LEN - 1) {
		sprintf(s, "%x", rand());
		strcat((char *)rands, (char *)s);
	}
	fwrite(rands, 1, SIGNATURE_LEN - 1, freqfp);
	fputc('\n', freqfp);
	fprintf(freqfp, "%d\n", SPECIAL_WORDS);
	for (i=0; i<=MAX_WORD_LEN; i++) {
		if (freq_strings_num[i] <= 0) continue;
		fprintf(freqfp, "%d %d\n", i, freq_strings_num[i]);
		e = freq_strings_table[i];
		while (e!=NULL) {
			fprintf(freqfp, "%s\n", e->word);
			p = e;
			e = e->next;
			free(p->word);
			free(p);
		}
	}
	fflush(freqfp);
	fclose(freqfp);
	if (!dump_small_hash(dict_hash_table, hash_file)) return -1;

	/*
	 * Now sort chosen ones case-insensitively according to the name so that
	 * those words all fall into the same page offset in the hash/string tables.
	 */

	/* Alter order of words in .hash_table */
	sprintf(s, "/tmp/sort%d.a", pid);
	if ((awkfp = fopen(s, "w")) == NULL) {
		fprintf(stderr, "cannot open for writing: %s\n", s);
		return -1;
	}
	sprintf(s, "BEGIN {}\n{print $3 \" \" $2 \" \" $1}\nEND {}\n");
	fwrite(s, 1, strlen((char *)s), awkfp);
	fflush(awkfp);
	fclose(awkfp);
#if	0
	sprintf(s, "cat /tmp/sort%d.a\n", pid);
	system(s);
#endif	/*0*/
#if	0
	printf("stage1:");
	getchar();
#endif	/*0*/
	sprintf(s, "exec %s -f /tmp/sort%d.a < '%s' > /tmp/sort%d\n", SYSTEM_AWK, pid, tescapesinglequote(hash_file, es1), pid);
	system(s);
	sprintf(s, "exec %s -d -f /tmp/sort%d > /tmp/temp%d\n", SYSTEM_SORT, pid, pid);
	system(s);

	sprintf(s, "/tmp/sort%d.a", pid);
	if ((awkfp = fopen(s, "w")) == NULL) {
		fprintf(stderr, "cannot open for writing: %s\n", s);
		return -1;
	}
	sprintf(s, "%s", "BEGIN {}\n{print $3 \" \" NR-1 \" \" $1}\nEND {}\n");
	fwrite(s, 1, strlen((char *)s), awkfp);
	fflush(awkfp);
	fclose(awkfp);

	sprintf(s, "exec %s -f /tmp/sort%d.a < /tmp/temp%d > '%s'\n", SYSTEM_AWK, pid, pid, tescapesinglequote(hash_file, es1));	/* reorder and put in new word numbers */
	system(s);

#if	0
	printf("stage2:");
	getchar();
#endif	/*0*/
	/* Now extract string-table, which is the set of 2nd components of the hash-table */
	sprintf(s, "/tmp/sort%d.a", pid);
	if ((awkfp = fopen(s, "w")) == NULL) {
		fprintf(stderr, "cannot open for writing: %s\n", s);
		return -1;
	}
	sprintf(s, "%s", "BEGIN {}\n{print $3}\nEND {}\n");
	fwrite(s, 1, strlen((char *)s), awkfp);
	fflush(awkfp);
	fclose(awkfp);
#if	0
	sprintf(s, "cat /tmp/sort%d.a\n", pid);
	system(s);
#endif	/*0*/
	sprintf(s, "exec %s -f /tmp/sort%d.a < '%s' > '%s'\n", SYSTEM_AWK, pid, tescapesinglequote(hash_file, es1), tescapesinglequote(string_file, es2));
	system(s);

#if	0
	printf("stage3:"); getchar();
#endif	/*0*/
	/*
	 * Now pad the hash-file and string files and store indices to words at
	 * page boundary so that search() on compressed files can be made fast
	 * -- it need not load the whole hash-table: just the page where the
	 * word occurs. The index files are very small (< 1K) so read is fast.
	 * The padded files are in binary -- this is what tcompress/tuncompress
	 * read-in. This is done to save space.
	 */
	pad_hash_file(hash_file, FILEBLOCKSIZE);
	pad_string_file(string_file, FILEBLOCKSIZE);
	return 0;
}
