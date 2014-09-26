/* Copyright (c) 1994 Sun Wu, Udi Manber, Burra Gopal.  All Rights Reserved. */
/* ./glimpse/index/build_in.c */

/* --------------------------------------------------------------
    build_index():  build an index list from a set of files.
    INPUT: a set of file names
	   char **name_list[];
	   a partition table
	   int p_table[];
    OUTPUT: an index list;
	   char *index_list;
	   the index list is a char string as follows:
	   each entry of the index list contains two parts:
	   name and indices, where name is an ascii character string,
           and indices is a list of short integer. (unsigned char)
           We use newline as a 'record delimiter' (a 'record is logically
	   a word associated with its indices), and WORD_END_MARK to separate
	   a word from its list of indices (s.t. fscanf %s works).
	   Since we restrict the max number of partitions to be 255.
	   a byte is enough to represent the index value. Note that there
	   cannot be a partition #ed '\n'.
	   An example index list: (in logical view)
           this 12 19 \n is 9 17 12 18 19 \n an 7 12 \n example 16 \n
-----------------------------------------------------------------------*/

#include "glimpse.h"
#include <errno.h>

#define debugt
#define BINARY 1
/* #define SW_DEBUG  the original sw output of index set */

/* This flag must always be defined: it is used only in build_in.c */
/* #define UDI_DEBUG  the original outputs of each indexed file */

/* Some variables used throughout */
#if	BG_DEBUG
extern FILE  *LOGFILE; 	/* file descriptor for LOG output */
#endif	/*BG_DEBUG*/
extern FILE  *STATFILE;	/* file descriptor for statistical data about indexed files */
extern FILE  *MESSAGEFILE;	/* file descriptor for important messages meant for the user */
extern char  INDEX_DIR[MAX_LINE_LEN];
extern char  sync_path[MAX_LINE_LEN];
extern struct stat istbuf;
extern struct stat excstbuf;
extern struct stat incstbuf;

void insert_h();
void insert_index();

extern int ICurrentFileOffset;
extern int NextICurrentFileOffset;

/* Some options used throughout */
extern int OneFilePerBlock;
extern int IndexNumber;
extern int CountWords;
extern int StructuredIndex;
extern int InterpretSpecial;
extern int total_size;
extern int MAXWORDSPERFILE;
extern int NUMERICWORDPERCENT;
extern int AddToIndex;
extern int DeleteFromIndex;
extern int FastIndex;
extern int BuildDictionary;
extern int BuildDictionaryExisting;
extern int CompressAfterBuild;
extern int IncludeHigherPriority;
extern int FilenamesOnStdin;
extern int UseFilters;
extern int ByteLevelIndex;
extern int RecordLevelIndex;
extern int StoreByteOffset;
extern int rdelim_len;
extern char rdelim[MAX_LINE_LEN];
extern char old_rdelim[MAX_LINE_LEN];
/* int IndexUnderscore; */
extern int IndexableFile;
extern int MAX_INDEX_PERCENT;
extern int MAX_PER_MB;
extern int I_THRESHOLD;
extern int usemalloc;
extern int BigHashTable;

extern int AddedMaxWordsMessage;
extern int AddedMixedWordsMessage;

extern int  icount; /* count the number of my_malloc for indices structure */
extern int  hash_icount; /* to see how much was added to the current hash table */
extern int  save_icount; /* to see how much was added to the index by the current file */
extern int  numeric_icount; /* to see how many numeric words were there in the current file */

extern int num_filter;
extern int filter_len[MAX_FILTER];
extern CHAR *filter[MAX_FILTER];
extern CHAR *filter_command[MAX_FILTER];
extern int REAL_PARTITION, REAL_INDEX_BUF, MAX_ALL_INDEX, FILEMASK_SIZE;
extern int mask_int[32];
struct indices	*deletedlist = NULL;

char **name_list[MAXNUM_INDIRECT];
unsigned int *disable_list = NULL;
int *size_list[MAXNUM_INDIRECT];	/* temporary area to store size of each file */
extern int  p_table[MAX_PARTITION];
int  p_size_list[MAX_PARTITION];	/* sum of the sizes of the files in each partition */
int part_num;   /* number of partitions */

extern int memory_usage;

/* borrowd from getword.c */
extern int PrintedLongWordWarning;
extern int indexable_char[256];
extern char *getword();

extern int file_num;
extern int old_file_num;
extern int attr_num;

extern int  bp;                          /* buffer pointer */
extern unsigned char word[MAX_WORD_BUF];
extern int FirstTraverse1;

extern struct  indices *ip;
extern int HashTableSize;
struct token **hash_table; /*[MAX_64K_HASH];*/

build_index()
{
	int	i;

	if (AddToIndex || FastIndex) {
		FirstTraverse1 = OFF;
	}
	if ((total_size < LIMIT_64K_HASH*1024*1024) || !BigHashTable) {
		hash_table = (struct token **)my_malloc(sizeof(struct token *) * MAX_64K_HASH);
		HashTableSize = MAX_64K_HASH;
	}
	else {
		hash_table = (struct token **)my_malloc(sizeof(struct token *) * MAX_256K_HASH);
		HashTableSize = MAX_256K_HASH;
	}
        build_hash();
        /* traverse1(); ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ removed on oct/8/96, bgopal, to see if crazysegvs disappear on lec */
        return;
}

/* ----------------------------------------------------------------------
traverse()
function: traverse the hash list of indices = a hash list is a array of
linked list, where every node in a linked list contains a word whose
hash_value is the same.
While traversing the hash list, traverse() output a stream of index list.
It also frees the memory used in hash_table.
------------------------------------------------------------------------*/
#define CRAZYSEGV	0
traverse()
{
    int numseencount = 0;
    int numelements;
    int numonline;
    int  i, j, attribute;
    struct token *tp, *tp_old;
    struct indices *ip, *ip_old;
#if	!CRAZYSEGV
    FILE   *f_out;
#else
    unsigned char onechar[4];
    unsigned char onestring[MAX_LINE_LEN];
    int	f_out;
#endif
    char   s[MAX_LINE_LEN];
    char   *word;
    int	x = -1, y=0, diff, temp, even_words=1;	/* 0 is an even number */
    int fputcerr; /* added by dgh 5-8-96 */

#ifdef	SW_DEBUG
    printf("in traverse()\n");
#endif
    sprintf(s, "%s/%s", INDEX_DIR, I2);
#if	!CRAZYSEGV
    if ((f_out = fopen(s, "w")) == NULL) {
#else
    if ((f_out = open(s, O_WRONLY|O_CREAT|O_TRUNC, 0600)) == -1) {
#endif
	fprintf(stderr, "Cannot open %s for writing\n", s);
	exit(2);
    }
    for(i=0; i<HashTableSize; i++) {
        if(hash_table[i] == NULL) continue;
        tp = hash_table[i];
        tp_old = tp;
        while(tp != NULL) {   /* traverse the token list */
	    word = tp->word;
            while(*word != '\0') {  /* copy the word to output */
#if	!CRAZYSEGV
		fputcerr=fputc(*word++, f_out);/* change from putc to fputc */
				       /* by dgh, 8-5-96 */
#else
		write(f_out, word, 1);
		word++;
#endif

            }

	    /* Look for stop lists */
	    if (OneFilePerBlock && !ByteLevelIndex && (file_num > MaxNum8bPartition) && (tp->totalcount > (file_num * MAX_INDEX_PERCENT / 100))) {
#if	!CRAZYSEGV
		putc(ALL_INDEX_MARK, f_out);
#else
		onechar[0] = ALL_INDEX_MARK;
		write(f_out, onechar, 1);
#endif
		if (StructuredIndex) {  /* force big-endian as usual */
		    attribute = encode16b(tp->attribute);
#if	!CRAZYSEGV
		    putc((attribute&0x0000ff00)>>8, f_out);
		    putc((attribute&0x000000ff), f_out);
#else
		    onechar[0] = (attribute&0x0000ff00)>>8;
		    onechar[1] = (attribute&0x000000ff);
		    write(f_out, onechar, 2);
#endif
		}
#if	!CRAZYSEGV
		putc(DONT_CONFUSE_SORT, f_out);
#else
		onechar[0] = DONT_CONFUSE_SORT;
		write(f_out, onechar, 1);
#endif
		goto next_token;
	    }
	    else if (ByteLevelIndex && (tp->totalcount > ( (((total_size>>20) > 0) && ((total_size>>20)*MAX_PER_MB < MAX_ALL_INDEX)) ? ((total_size>>20) * MAX_PER_MB) : MAX_ALL_INDEX) )) {
#if	!CRAZYSEGV
		putc(ALL_INDEX_MARK, f_out);
#else
		onechar[0] = ALL_INDEX_MARK;
		write(f_out, onechar, 1);
#endif
		if (StructuredIndex) {  /* force big-endian as usual */
		    attribute = encode16b(tp->attribute);
#if	!CRAZYSEGV
		    putc((attribute&0x0000ff00)>>8, f_out);
		    putc((attribute&0x000000ff), f_out);
#else
		    onechar[0] = (attribute&0x0000ff00)>>8;
		    onechar[1] = (attribute&0x000000ff);
		    write(f_out, onechar, 2);
#endif
		}
#if	!CRAZYSEGV
		putc(DONT_CONFUSE_SORT, f_out);
#else
		onechar[0] = DONT_CONFUSE_SORT;
		write(f_out, onechar, 2);
#endif
		goto next_token;
	    }

#if	!CRAZYSEGV
	    putc(WORD_END_MARK, f_out);
#else
	    onechar[0] = WORD_END_MARK;
	    write(f_out, onechar, 1);
#endif
	    if (StructuredIndex) {  /* force big-endian as usual */
		attribute = encode16b(tp->attribute);
#if	!CRAZYSEGV
		putc((attribute&0x0000ff00)>>8, f_out);
		putc((attribute&0x000000ff), f_out);
#else
		    onechar[0] = (attribute&0x0000ff00)>>8;
		    onechar[1] = (attribute&0x000000ff);
		    write(f_out, onechar, 2);
#endif
	    }

	    numonline = 0;
	    x = -1;
	    y = 0;
	    even_words = 1;

	    ip = tp->ip;	/* traverse the indices list */
            ip_old = ip;
	    numelements = 0;
            while(ip != NULL) {
		numelements ++;
		if (CountWords) {
#if	!CRAZYSEGV
		    fprintf(f_out, "%d", ip->offset[0]);
#else
		    sprintf(onestring, "%d", ip->offset[0]);
		    write(f_out, onestring, strlen(onestring));
#endif
		}
		else {
		    if (ByteLevelIndex) {
			for (j=0; j < INDEX_SET_SIZE; j++) {
			    if (ip->index[j] == INDEX_ELEM_FREE) continue;
			    if ((ip->offset[j] <= y) && (y > 0) && (x == ip->index[j])) {	/* consecutive offsets not increasing in same file! */
				fprintf(stderr, "ignoring (%d, %d) > (%d, %d)\n", x, y, ip->index[j], ip->offset[j]);
				continue;	/* error! */
			    }
			    if (numonline >= MAX_PER_LINE) {
				/* terminate current line since it is too late to put ALL_INDEX_MARK now ... Unfortunate since sort is screwedup */
#if	!CRAZYSEGV
				putc('\n', f_out);
#else
			        onechar[0] = '\n';
			        write(f_out, onechar, 1);
#endif

#if	0
				putc('\n', stdout);
#endif	/*0*/

				word = tp->word;
				while(*word != '\0') {  /* copy the word to output */
#if	!CRAZYSEGV
				    putc(*word++, f_out);
#else
				    write(f_out, word, 1);
				    word ++;
#endif
				}
#if	!CRAZYSEGV
				putc(WORD_END_MARK, f_out);
#else
			        onechar[0] = WORD_END_MARK;
			        write(f_out, onechar, 1);
#endif
				if (StructuredIndex) {  /* force big-endian as usual */
				    attribute = encode16b(tp->attribute);
#if	!CRAZYSEGV
				    putc((attribute&0x0000ff00)>>8, f_out);
				    putc((attribute&0x000000ff), f_out);
#else
				    onechar[0] = (attribute&0x0000ff00)>>8;
				    onechar[1] = (attribute&0x000000ff);
				    write(f_out, onechar, 2);
#endif
				}
				numonline = 0;
				x = -1;	/* to force code below to output it as if it is a fresh file */
				y = 0;	/* must output first offset as is, rather than difference */
			    }

			    if (x != ip->index[j]) {
				if (x != -1) {
				    temp = encode8b(0);
#if	!CRAZYSEGV
				    putc(temp, f_out);	/* can never ordinarily happen since ICurrentFileOffset is always ++d => delimiter (unless RecordLevelIndex) */
#else
				    onechar[0] = temp;
				    write(f_out, onechar, 1);
#endif
				}
				if (file_num <= MaxNum8bPartition) {
				    x = encode8b(ip->index[j]);
#if	!CRAZYSEGV
				    putc(x&0x000000ff, f_out);
#else
				    onechar[0] = x&0x000000ff;
				    write(f_out, onechar, 1);
#endif
				}
				else if (file_num <= MaxNum16bPartition) {
				    x = encode16b(ip->index[j]);
#if	!CRAZYSEGV
				    putc((x&0x0000ff00)>>8, f_out);
				    putc(x&0x000000ff, f_out);
#else
				    onechar[0] = (x&0x0000ff00)>>8;
				    onechar[1] = x&0x000000ff;
				    write(f_out, onechar, 2);
#endif
				}
				else {
				    x = encode24b(ip->index[j]);
#if	!CRAZYSEGV
				    putc((x&0x00ff0000)>>16, f_out);
				    putc((x&0x0000ff00)>>8, f_out);
				    putc(x&0x000000ff, f_out);
#else
				    onechar[0] = (x&0x00ff0000)>>16;
				    onechar[1] = (x&0x0000ff00)>>8;
				    onechar[2] = x&0x000000ff;
				    write(f_out, onechar, 3);
#endif
				}
				x = ip->index[j];	/* for next round */
#if	0
				printf("#######x=%d ", x);
#endif	/*0*/
				y = 0;
			    }

			    diff = ip->offset[j] - y;
			    y = ip->offset[j];
			    if (diff < MaxNum1BPartition) {
				temp = encode8b(diff);
#if	!CRAZYSEGV
				putc(temp, f_out);
#else
			        onechar[0] = temp;
			        write(f_out, onechar, 1);
#endif
			    }
			    else if (diff < MaxNum2BPartition) {
				temp = encode8b((diff/MaxNum8bPartition) | 0x40);
#if	!CRAZYSEGV
				putc(temp, f_out);
#else
			        onechar[0] = temp;
			        write(f_out, onechar, 1);
#endif
				temp = encode8b(diff % MaxNum8bPartition);
#if	!CRAZYSEGV
				putc(temp, f_out);
#else
			        onechar[0] = temp;
			        write(f_out, onechar, 1);
#endif
			    }
			    else if (diff < MaxNum3BPartition) {
				temp = encode8b((diff/MaxNum16bPartition) | 0x80);
#if	!CRAZYSEGV
				putc(temp, f_out);
#else
			        onechar[0] = temp;
			        write(f_out, onechar, 1);
#endif
				temp = encode16b(diff % MaxNum16bPartition);
#if	!CRAZYSEGV
				putc((temp & 0x0000ff00) >> 8, f_out);
				putc(temp & 0x000000ff, f_out);
#else
			        onechar[0] = (temp & 0x0000ff00) >> 8;
			        onechar[1] = temp & 0x000000ff;
			        write(f_out, onechar, 2);
#endif
			    }
			    else {
				temp = encode8b((diff/MaxNum24bPartition) | 0xc0);
#if	!CRAZYSEGV
				putc(temp, f_out);
#else
				onechar[0] = temp;
				write(f_out, onechar, 1);
#endif
				temp = encode24b(diff % MaxNum24bPartition);
#if	!CRAZYSEGV
				putc((temp & 0x00ff0000) >> 16, f_out);
				putc((temp & 0x0000ff00) >> 8, f_out);
				putc(temp & 0x000000ff, f_out);
#else
			        onechar[0] = (temp & 0x00ff0000) >> 16;
				onechar[1] = (temp & 0x0000ff00) >> 8;
				onechar[2] = temp & 0x000000ff;
			        write(f_out, onechar, 3);
#endif
			    }
			    numonline ++;
			}
		    } /* ByteLevelIndex */
		    else if (OneFilePerBlock) {
			if (file_num <= MaxNum8bPartition) {
			    for(j=0; j < INDEX_SET_SIZE; j++) {
				if (ip->index[j] == INDEX_ELEM_FREE) continue;
#if	!CRAZYSEGV
				putc(encode8b(ip->index[j]), f_out);
#else
			        onechar[0] = encode8b(ip->index[j]);
			        write(f_out, onechar, 1);
#endif

			    }
			}
			else if (file_num <= MaxNum12bPartition) {
			  for(j=0; j < INDEX_SET_SIZE; j++) {
			    if (ip->index[j] == INDEX_ELEM_FREE) continue;
			    x = encode12b(ip->index[j]);
			    if (even_words) {
#if	!CRAZYSEGV
				putc(x & 0x000000ff, f_out);	/* lsb */
#else
			        onechar[0] = x & 0x000000ff;
			        write(f_out, onechar, 1);
#endif
				y = (x & 0x00000f00)>>8;	/* msb */
				even_words = 0;
			    }
			    else {	/* odd number of words so far */
			        y |= (x&0x00000f00)>>4; /* msb of x into msb of y */
#if	!CRAZYSEGV
				putc(y, f_out);
				putc(x&0x000000ff, f_out);
#else
			        onechar[0] = y;
			        onechar[1] = x&0x000000ff;
			        write(f_out, onechar, 2);
#endif
				even_words = 1;
			    }
			  }
			}
			else if (file_num <= MaxNum16bPartition) {
			    for(j=0; j < INDEX_SET_SIZE; j++) {
				if (ip->index[j] == INDEX_ELEM_FREE) continue;
				x = encode16b(ip->index[j]);
#if	!CRAZYSEGV
				putc((x&0x0000ff00)>>8, f_out);
				putc(x&0x000000ff, f_out);
#else
				onechar[0] = (x&0x0000ff00)>>8;
				onechar[1] = x&0x000000ff;
			        write(f_out, onechar, 2);
#endif
			    }
			}
			else {
			    for(j=0; j < INDEX_SET_SIZE; j++) {
				if (ip->index[j] == INDEX_ELEM_FREE) continue;
				x = encode24b(ip->index[j]);
#if	!CRAZYSEGV
				putc((x&0x00ff0000)>>16, f_out);
				putc((x&0x0000ff00)>>8, f_out);
				putc(x&0x000000ff, f_out);
#else
				onechar[0] = (x&0x00ff0000)>>16;
				onechar[1] = (x&0x0000ff00)>>8;
				onechar[2] = x&0x000000ff;
			        write(f_out, onechar, 3);
#endif
			    }
			}
		    } /* OneFilePerBlock */
		    else {	/* normal partitions */
			for(j=0; j < INDEX_SET_SIZE; j++) {
			    if (ip->index[j] == INDEX_ELEM_FREE) continue;
#if	!CRAZYSEGV
			    putc(ip->index[j], f_out);
#else
			    onechar[0] = ip->index[j];
			    write(f_out, onechar, 1);
#endif
			}
		    }
		}
                ip = ip->next_i;   /* go to next indices */
                indicesfree(ip_old, sizeof(struct indices));
                ip_old = ip;
            }
	    if (!ByteLevelIndex && OneFilePerBlock && !even_words && (file_num > MaxNum8bPartition) && (file_num <= MaxNum12bPartition)) {
#if	!CRAZYSEGV
		putc(y, f_out);
#else
		onechar[0] = y;
		write(f_out, onechar, 1);
#endif

	    }

next_token:

#if	!CRAZYSEGV
	    if (putc('\n', f_out) == EOF) {
#else
	    onechar[0] = '\n';
	    if (write(f_out, onechar, 1) <= 0) {
#endif
		fprintf(stderr, "Error: write failed at %s:%d\n", __FILE__, __LINE__); 
		exit(2);
	    }
            tp = tp->next_t;    /* go to next token */
#if	0
	    fprintf(stderr, "numelements=%d\n", numelements);
#endif	/*0*/
#if	BG_DEBUG
	    memory_usage -= (strlen(tp_old->word) + 1);
#endif	/*BG_DEBUG*/
	    wordfree(tp_old->word, 0);
            tokenfree(tp_old, sizeof(struct token));
            tp_old = tp;
	    numseencount ++;
        }
    }
    tokenallfree();
    indicesallfree();
    wordallfree();
#if	BG_DEBUG
    fprintf(stderr, "out of traverse(): saved/freed %d tokens: new usage: %d\n", numseencount, memory_usage);
#endif
#if	!CRAZYSEGV
    fflush(f_out);
    fclose(f_out);
#else
    close(f_out);
#endif
}

traverse1()
{
    FILE *i1, *i2, *i3;
    int ret;
    char s[MAX_LINE_LEN], es1[MAX_LINE_LEN], es2[MAX_LINE_LEN], es3[MAX_LINE_LEN];
    char s1[MAX_LINE_LEN];
    extern int errno;
    static int maxsortlinelen = 0;
    int	i;

    if (maxsortlinelen <= 0) {
	if (file_num < MaxNum8bPartition)
	    maxsortlinelen = round((MaxNum8bPartition * sizeof(int) + MAX_NAME_SIZE), MAX_LINE_LEN) * MAX_LINE_LEN;
	else if (file_num < MaxNum12bPartition)
	    maxsortlinelen = round((MaxNum12bPartition * sizeof(int) + MAX_NAME_SIZE), MAX_LINE_LEN) * MAX_LINE_LEN;
	else maxsortlinelen = MAX_SORTLINE_LEN;
    }

    traverse();	/* will produce .i2 and my_free allocated memory */

#if	USESORT_Z_OPTION
#if	DONTUSESORT_T_OPTION || SFS_COMPAT
    sprintf(s, "exec %s -z %d '%s/%s' > '%s/%s'\n", SYSTEM_SORT, maxsortlinelen, escapesinglequote(INDEX_DIR, es1), I2, escapesinglequote(INDEX_DIR, es2), O2);
#else
    sprintf(s, "exec %s -T '%s' -z %d '%s/%s' > '%s/%s'\n", SYSTEM_SORT, escapesinglequote(INDEX_DIR, es1), maxsortlinelen, escapesinglequote(INDEX_DIR, es2), I2, escapesinglequote(INDEX_DIR, es3), O2);
#endif
#else
#if	DONTUSESORT_T_OPTION || SFS_COMPAT
    sprintf(s, "exec %s '%s/%s' > '%s/%s'\n", SYSTEM_SORT, escapesinglequote(INDEX_DIR, es1), I2, escapesinglequote(INDEX_DIR, es2), O2);
#else
    sprintf(s, "exec %s -T '%s' '%s/%s' > '%s/%s'\n", SYSTEM_SORT, escapesinglequote(INDEX_DIR, es1), escapesinglequote(INDEX_DIR, es2), I2, escapesinglequote(INDEX_DIR, es3), O2);
#endif
#endif

#ifdef SW_DEBUG
    printf("%s", s);
#endif
    if((ret=system(s)) != 0) {
	sprintf(s1, "system('%s') failed at:\n\t File=%s, Line=%d, Errno=%d", s, __FILE__, __LINE__, errno);
	perror(s1);
	fprintf(stderr, "Please try to run the program again\n(If there's no memory, increase the swap area / don't use -M and -B options)\n");
	exit(2);
    }

#ifdef SW_DEBUG
    printf("mv .o2 .i2\n"); fflush(stdout);
#endif
#if SFS_COMPAT
    sprintf(s, "%s/%s", INDEX_DIR, O2);
    sprintf(s1, "%s/%s", INDEX_DIR, I2);
    rename(s, s1);
#else
    sprintf(s, "exec %s '%s/%s' '%s/%s'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), O2, escapesinglequote(INDEX_DIR, es2), I2);
    system(s);
#endif
    system(sync_path);	/* sync() has a bug */
#if	0
    printf("traversed\n");
    sprintf(s, "exec %s -10 '%s/%s'\n", SYSTEM_HEAD, escapesinglequote(INDEX_DIR, es1), I2);
    system(s);
#endif	/*0*/

    /*
     * This flag is set from outside iff build-fast | build-addto option is set.
     */
    if(FirstTraverse1) {
	/* Mention whether numbers are indexed */
	if(IndexNumber) sprintf(s, "exec %s %%1234567890 > '%s/%s'\n", SYSTEM_ECHO, escapesinglequote(INDEX_DIR, es1), INDEX_FILE);
	else sprintf(s, "exec %s %% > '%s/%s'\n", SYSTEM_ECHO, escapesinglequote(INDEX_DIR,es1), INDEX_FILE);
	system(s);

	/* Put the magic number: 0 if not 1file/blk, numfiles otherwise */
	if (OneFilePerBlock) {
		if (ByteLevelIndex) sprintf(s, "exec %s %%-%d >> '%s/%s'\n", SYSTEM_ECHO, file_num, escapesinglequote(INDEX_DIR, es1), INDEX_FILE);
		else sprintf(s, "exec %s %%%d >> '%s/%s'\n", SYSTEM_ECHO, file_num, escapesinglequote(INDEX_DIR, es1), INDEX_FILE);
	}
	else sprintf(s, "exec %s %%0 >> '%s/%s'\n", SYSTEM_ECHO, escapesinglequote(INDEX_DIR, es1), INDEX_FILE);
	system(s);

	/* Put the magic number: 0 if not structured index, 1 if so */
	if (StructuredIndex) sprintf(s, "exec %s %%%d >> '%s/%s'\n", SYSTEM_ECHO, attr_num, escapesinglequote(INDEX_DIR, es1), INDEX_FILE);
	else if (RecordLevelIndex) sprintf(s, "exec %s %%-2 %s >> '%s/%s'\n", SYSTEM_ECHO, old_rdelim, escapesinglequote(INDEX_DIR, es1), INDEX_FILE);
	else sprintf(s, "exec %s %%0 >> '%s/%s'\n", SYSTEM_ECHO, escapesinglequote(INDEX_DIR, es1), INDEX_FILE);
	system(s);

#ifdef	SW_DEBUG
	sprintf(s, "exec %s -l %s/.glimpse*\n", SYSTEM_LS, escapesinglequote(INDEX_DIR, es1));
	system(s);
#endif
        sprintf(s, "exec %s '%s/%s' >> '%s/%s'\n", SYSTEM_CAT, escapesinglequote(INDEX_DIR, es1), I2, escapesinglequote(INDEX_DIR, es2), INDEX_FILE);
        system(s);

#if	SFS_COMPAT
	sprintf(s, "%s/%s", INDEX_DIR, I2);
	unlink(s);
#else
	sprintf(s, "exec %s '%s/%s'\n", SYSTEM_RM, escapesinglequote(INDEX_DIR, es1), I2);
	system(s);
#endif

#ifdef	SW_DEBUG
	sprintf(s, "exec %s -l %s/.glimpse*\n", SYSTEM_LS, escapesinglequote(INDEX_DIR, es1));
	system(s);
#endif
#if	0
    printf("catted\n");
    sprintf(s, "exec %s -10 '%s/%s'\n", SYSTEM_HEAD, escapesinglequote(INDEX_DIR, es1), INDEX_FILE);
    system(s);
#endif	/*0*/
        FirstTraverse1 = 0;
	system(sync_path);	/* sync() has a bug */
        return;
    }
    /* else not first-traverse */

    sprintf(s, "%s/%s", INDEX_DIR, INDEX_FILE);
    if((i1 = fopen(s, "r")) == NULL) {	/* new stuff */
        fprintf(stderr, "can't open %s for reading\n", s);
        exit(2);
    }
    sprintf(s, "%s/%s", INDEX_DIR, I2);
    if((i2 = fopen(s, "r")) == NULL) {	/* old stuff */
        fprintf(stderr, "can't open %s for reading\n", s);
        exit(2);
    }
    sprintf(s, "%s/%s", INDEX_DIR, I3);
    if((i3 = fopen(s, "w")) == NULL) {	/* result */
        fprintf(stderr, "can't open %s for writing\n", s);
        exit(2);
    }

    /* Copy the 3 option fields (indexnumber, onefileperblock, structuredqueries) */
    fgets(s, 256, i1);
    s[255] = '\0';
    fputs(s, i3);
    fgets(s, 256, i1);
    s[255] = '\0';
    fputs(s, i3);
    fgets(s, 256, i1);
    s[255] = '\0';
    fputs(s, i3);

    merge_in(i2, i1, i3);
    /* merge_in(i1, i2, i3); */
#ifdef	BG_DEBUG
    fprintf(stderr, "out of merge_in()\n");
#endif	/*BG_DEBUG*/
    fclose(i1);
    fflush(i2);
    fclose(i2);
    fflush(i3);
    fclose(i3);
    system(sync_path);	/* sync() has a bug */
#ifdef SW_DEBUG
    printf("mv .i3 %s\n", INDEX_FILE); fflush(stdout);
#endif
#if	SFS_COMPAT
    sprintf(s, "%s/%s", INDEX_DIR, I3);
    sprintf(s1, "%s/%s", INDEX_DIR, INDEX_FILE);
    rename(s, s1);
#else
    sprintf(s, "exec %s '%s/%s' '%s/%s'", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), I3, escapesinglequote(INDEX_DIR, es2), INDEX_FILE);
    system(s);
#endif
/* #ifdef SW_DEBUG */
#if 0
    printf("ls -l .i2 %s\n", INDEX_FILE); fflush(stdout);
    sprintf(s, "exec %s -l %s/.glimpse*", SYSTEM_LS, escapesinglequote(INDEX_DIR, es1));
    printf("%d\n", system(s));
#endif
#if	0
    printf("merged\n");
    sprintf(s, "exec %s -10 '%s/%s'\n", SYSTEM_HEAD, escapesinglequote(INDEX_DIR, es1), INDEX_FILE);
    system(s);
#endif	/*0*/
}

/* --------------------------------------------------------------------
build_hash():
input: a set of filenames in name_list[], a partition table p_table[]
output: a hash table hash_table[].
-----------------------------------------------------------------------*/
build_hash()
{
    int	fd;                          /* opened file number */
    int  i, pn;                  /* pn: current partition */
    int  num_read;
    char word[256];
    struct stat stbuf;
    int offset;
    int toread;
    unsigned char *buffer;	/* running pointer for getword = place where reads begin */
    unsigned char *bx;		/* running pointer for read-loop, initially buffer */
    unsigned char *buffer_end;	/* place where getword should stop */
    unsigned char *buffer_begin;/* constant pointer to beginning */
    unsigned char *next_record;	/* pointer that tells where the current record ends: if buffer (returned by getword) is >= this, increment ICurrentFileOffset */
    unsigned char *last_record;	/* pointer that tells where the last record ends: may or may not be > buffer_end, but surely <= bx the last byte read */
    int residue;	/* extra variable to store buffer_begin + BLOCK_SIZE - buffer_end */
    int tried_once = 0;
    int attribute;
    int ret;
    char outname[MAX_LINE_LEN];
    char *unlinkname = NULL;
    int pid = getpid();

    if (StructuredIndex) region_initialize();
    init_hash_table();
#ifdef debug
    printf("entering build_hash(), part_num=%d\n", part_num);
#endif

    tried_once = 0;
try_again_1:
    buffer_begin = buffer = (unsigned char *) my_malloc(sizeof(char)* BLOCK_SIZE + 10);	/* always read in units of BLOCK_SIZE or less */
    if(buffer == NULL) {
	fprintf(stderr, "not enough memory in build_hash\n");
	if (tried_once) return;
	traverse1();
	init_hash_table();
	tried_once = 1;
	goto try_again_1;
    }
    bx = buffer;

    if (OneFilePerBlock) {
	for(i=0; i<file_num; i++) {
	    unlinkname = NULL;
	    if ((disable_list != NULL) && (i<old_file_num) && (disable_list[block2index(i)] & mask_int[i%(8*sizeof(int))])) continue;
	    if (LIST_GET(name_list, i) == NULL) continue;
	    if ((ret = tuncompress_file(LIST_GET(name_list, i), outname, TC_EASYSEARCH | TC_OVERWRITE | TC_NOPROMPT)) > 0) {	/* do not remove old .TZ file */
		if (StructuredIndex && (-1 == region_create(outname))) {
		    fprintf(stderr, "permission denied or non-existent file: %s\n", LIST_GET(name_list, i));
		    remove_filename(i, -1);
		    continue;
		}
		if (((fd = my_open(outname, O_RDONLY, 0)) == -1) ) {
		    fprintf(stderr, "permission denied or non-existent file: %s\n", LIST_GET(name_list, i));
		    remove_filename(i, -1);
		    if (StructuredIndex) region_destroy();	/* cannot happen! */
		    unlink(outname);
		    continue;
		}
		unlinkname = outname;
		goto index_file1;
	    }

	    /* Try to apply the filter */
	    sprintf(outname, "%s/.glimpse_apply.%d", INDEX_DIR, pid);
	    if ((ret = apply_filter(LIST_GET(name_list, i), outname)) == 1) {
		/* Some pattern matched AND some filter was successful */
		if (StructuredIndex && (-1 == region_create(outname))) {
		    fprintf(stderr, "permission denied or non-existent file: %s\n", LIST_GET(name_list, i));
		    remove_filename(i, -1);
		    continue;
		}
		if (((fd = my_open(outname, O_RDONLY)) == -1) ) {	/* error: shouldn't have returned 1! */
		    fprintf(stderr, "permission denied or non-existent file: %s\n", LIST_GET(name_list, i));
		    remove_filename(i, -1);
		    if (StructuredIndex) region_destroy();	/* cannot happen! */
		    unlink(outname);
		    continue;
		}
		unlinkname = outname;
		goto index_file1;
	    }
	    else if (ret == 2) {
		/* Some pattern matched but no filter was successful */
		if (filetype(LIST_GET(name_list, i), 0, NULL, NULL)) {	/* try to index input file if it satisfies filetype */
		    remove_filename(i, -1);
		    unlink(outname);
		    continue;
		}
		unlinkname = outname;
	    }

	    if (StructuredIndex && (-1 == region_create(LIST_GET(name_list, i)))) {
		fprintf(stderr, "permission denied or non-existent file: %s\n", LIST_GET(name_list, i));
		remove_filename(i, -1);
		continue;
	    }
            if (((fd = my_open(LIST_GET(name_list, i), O_RDONLY, 0)) == -1) ) {
		fprintf(stderr, "permission denied or non-existent file: %s\n", LIST_GET(name_list, i));
		remove_filename(i, -1);
		if (StructuredIndex) region_destroy();	/* cannot happen! */
		if (unlinkname != NULL) unlink(unlinkname);
		continue;
            }

	index_file1:
#ifdef SW_DEBUG
	    if (AddToIndex || FastIndex) printf("adding words of %s in %d\n", LIST_GET(name_list,i), i);
	    printf("%s\n", LIST_GET(name_list, i));
#endif
	    /* my_stat(LIST_GET(name_list, i), &stbuf); Chris Dalton */
	    fstat(fd, &stbuf);
#ifdef	SW_DEBUG
	    printf("filesize: %d\n", stbuf.st_size);
#endif

#ifdef	UDI_DEBUG
	    printf("%s  ", LIST_GET(name_list, i));
	    printf("size: %d  ", stbuf.st_size);
#endif

	    /* buffer always points to a BLOCK_SIZE block of allocated memory */
	    buffer = buffer_begin;
	    residue = 0;
	    if (RecordLevelIndex) {
		if (!StoreByteOffset) NextICurrentFileOffset = ICurrentFileOffset = 1;
		else NextICurrentFileOffset = ICurrentFileOffset = 0;
	    }
	    for (offset = 0; offset < stbuf.st_size; offset += BLOCK_SIZE) {
		offset -= residue;
		if (!RecordLevelIndex) NextICurrentFileOffset = ICurrentFileOffset = offset;
		toread = offset + BLOCK_SIZE >= stbuf.st_size ? stbuf.st_size - offset : BLOCK_SIZE;
		lseek(fd, offset, SEEK_SET);

		bx= buffer;
		num_read = 0;
		while ((toread > 0) && ((num_read = read(fd, bx, toread)) < toread)) {
		    if (num_read <= 0) {
			buffer = bx;
			fprintf(stderr, "read error on file %s at offset %d\n", LIST_GET(name_list, i), offset);
			goto break_break1;	/* C doesn't have break; break; */
		    }
		    bx += num_read;
		    toread -= num_read;
		}
		if (num_read >= toread) {
			bx += num_read;
			toread -= num_read;
		}
		buffer_end = bx;
		residue = 0;
		if (buffer_end == buffer_begin + BLOCK_SIZE) {
			if (RecordLevelIndex) {
				buffer_end = backward_delimiter(buffer_end /* NOT bx */, buffer, rdelim, rdelim_len, 0);
			}
			else {
				while ((INDEXABLE(*(buffer_end-1))) && (buffer_end > buffer_begin + MAX_WORD_SIZE)) buffer_end --;
			}
			residue = buffer_begin + BLOCK_SIZE - buffer_end;
			/* if (residue > 0) printf("residue = %d in %s at %d\n", residue, LIST_GET(name_list, i), offset); */
		}
		if (RecordLevelIndex) {
			next_record = forward_delimiter(buffer, buffer_end, rdelim, rdelim_len, 0);
		}
		bx = buffer;
 
		PrintedLongWordWarning = 0;
		while ((buffer=(unsigned char *) getword(LIST_GET(name_list, i), word, buffer, buffer_end, &attribute, &next_record)) < buffer_end) {
		    if (RecordLevelIndex) {
			if (buffer >= next_record) {
			    next_record = forward_delimiter(buffer, buffer_end, rdelim, rdelim_len, 0);
			    if (StoreByteOffset) ICurrentFileOffset += next_record - buffer;
			    else ICurrentFileOffset ++;
			}
		    }
		    /* printf("%s\n", word); */
		    if(word[0] == '\0') continue;
		    if(icount - hash_icount >= I_THRESHOLD) {
#if	BG_DEBUG
			fprintf(LOGFILE, "reached I_THRESHOLD at %d\n", icount - hash_icount);
#endif	/*BG_DEBUG*/
			traverse1();
			init_hash_table();
			hash_icount = icount;
		    }
		    insert_h(word, i, attribute);
		}
		if (word[0] != '\0') {
		    /* printf("%s\n", word); */
		    if(icount - hash_icount >= I_THRESHOLD) {
#if	BG_DEBUG
			fprintf(LOGFILE, "reached I_THRESHOLD at %d\n", icount - hash_icount);
#endif	/*BG_DEBUG*/
			traverse1();
			init_hash_table();
			hash_icount = icount;
		    }
		    insert_h(word, i, attribute);
		}
		if (RecordLevelIndex) {
			if (buffer >= next_record) {
			    /* next_record = forward_delimiter(buffer, buffer_end, rdelim, rdelim_len, 0); */
			    ICurrentFileOffset ++;
			}
		}
		buffer = buffer_begin;
		next_record = buffer;
	    }

	break_break1:
            close(fd);
	    if (unlinkname != NULL) unlink(unlinkname);

#ifdef	UDI_DEBUG
	    printf("add to index: %d\n",icount-save_icount);
#endif
	    if ((MAXWORDSPERFILE > 0) && (icount-save_icount > MAXWORDSPERFILE)) {
		fprintf(MESSAGEFILE, "%d words are contributed by %s\n",
			icount-save_icount, LIST_GET(name_list, i));
		AddedMaxWordsMessage = ON;
	    }

	    if (IndexNumber && NUMERICWORDPERCENT && (numeric_icount * 100 > (icount - save_icount) * NUMERICWORDPERCENT) && (icount - save_icount > MIN_WORDS)) {
		fprintf(MESSAGEFILE, "NUMBERS occur in %d%% of %d words contributed by %s\n", (numeric_icount * 100)/(icount - save_icount), icount - save_icount, LIST_GET(name_list, i));
		AddedMixedWordsMessage = ON;
	    }

	    numeric_icount=0;
	    save_icount=icount;
	    if (StructuredIndex) region_destroy();
        }

	traverse1();
	init_hash_table();
	hash_icount = icount;
	my_free(buffer_begin, BLOCK_SIZE + 10);
	return;
    }

    for(pn=1; pn < part_num; pn++)	/* partition # 0 is not accessed */
    {
	if (pn == '\n') continue;	/* There cannot be a partition # '\n' or 0: see partition.c */
	for(i=p_table[pn]; i<p_table[pn+1]; i++) {
	    unlinkname = NULL;
	    if ((disable_list != NULL) && (i<old_file_num) && (disable_list[block2index(i)] & mask_int[i%(8*sizeof(int))])) continue;
	    if (LIST_GET(name_list, i) == NULL) continue;
	    if (BuildDictionaryExisting) {
		if (((fd = my_open(LIST_GET(name_list, i), O_RDONLY, 0)) == -1) ) {
		    fprintf(stderr, "permission denied or non-existent file: %s\n", LIST_GET(name_list, i));
		    remove_filename(i, -1);
		    continue;
		}
		if (!CompressAfterBuild) unlinkname = LIST_GET(name_list, i);	/* not needed anymore */
		goto index_file2;
	    }
	    if ((ret = tuncompress_file(LIST_GET(name_list, i), outname, TC_EASYSEARCH | TC_OVERWRITE | TC_NOPROMPT)) > 0) {	/* do not remove old .TZ file */
		if (StructuredIndex && (-1 == region_create(outname))) {
		    fprintf(stderr, "permission denied or non-existent file: %s\n", LIST_GET(name_list, i));
		    remove_filename(i, -1);
		    continue;
		}
		if (((fd = my_open(outname, O_RDONLY, 0)) == -1) ) {
		    fprintf(stderr, "permission denied or non-existent file: %s\n", LIST_GET(name_list, i));
		    remove_filename(i, -1);
		    if (StructuredIndex) region_destroy();	/* cannot happen! */
		    unlink(outname);
		    continue;
		}
		if (BuildDictionary && CompressAfterBuild) strcpy(LIST_GET(name_list, i), outname); /* name of clear file will be smaller, so enough space */
		else unlinkname = outname;
		goto index_file2;
	    }

	    /* Try to apply the filter */
	    sprintf(outname, "%s/.glimpse_apply.%d", INDEX_DIR, pid);
	    if ((ret = apply_filter(LIST_GET(name_list, i), outname)) == 1) {
		/* Some pattern matched AND some filter was successful */
		if (StructuredIndex && (-1 == region_create(outname))) {
		    fprintf(stderr, "permission denied or non-existent file: %s\n", LIST_GET(name_list, i));
		    remove_filename(i, -1);
		    continue;
		}
		if (((fd = my_open(outname, O_RDONLY)) == -1) ) {	/* error: shouldn't have returned 1! */
		    fprintf(stderr, "permission denied or non-existent file: %s\n", LIST_GET(name_list, i));
		    remove_filename(i, -1);
		    if (StructuredIndex) region_destroy();	/* cannot happen! */
		    unlink(outname);
		    continue;
		}
		unlinkname = outname;
		goto index_file2;
	    }
	    else if (ret == 2) {
		/* Some pattern matched but no filter was successful */
		if (filetype(LIST_GET(name_list, i), 0, NULL, NULL)) {	/* try to index input file if it satisfies filetype */
		    remove_filename(i, -1);
		    unlink(outname);
		    continue;
		}
		unlinkname = outname;
	    }

	    if (StructuredIndex && (-1 == region_create(LIST_GET(name_list, i)))) {
		fprintf(stderr, "permission denied or non-existent file: %s\n", LIST_GET(name_list, i));
		remove_filename(i, -1);
		continue;
	    }
            if (((fd = my_open(LIST_GET(name_list, i), O_RDONLY)) == -1) ) {
		fprintf(stderr, "permission denied or non-existent file: %s\n", LIST_GET(name_list, i));
		remove_filename(i, -1);
		if (StructuredIndex) region_destroy();	/* cannot happen! */
		if (unlinkname != NULL) unlink(unlinkname);
		continue;
            }

	index_file2:
#ifdef SW_DEBUG
	    if (AddToIndex || FastIndex) printf("adding words of %s in %d\n", LIST_GET(name_list, i), pn);
	    printf("%s\n", LIST_GET(name_list, i));
#endif
	    /* my_stat(LIST_GET(name_list, i), &stbuf); Chris Dalton */
	    fstat(fd, &stbuf);
#ifdef	SW_DEBUG
	    printf("filesize: %d\n", stbuf.st_size);
#endif

#ifdef	UDI_DEBUG
	    printf("%s  ", LIST_GET(name_list, i));
	    printf("size: %d  ", stbuf.st_size);
#endif

	    /* buffer always points to a BLOCK_SIZE block of allocated memory */
	    buffer = buffer_begin;
	    residue = 0;
	    if (RecordLevelIndex) {
		if (!StoreByteOffset) NextICurrentFileOffset = ICurrentFileOffset = 1;
		else NextICurrentFileOffset = ICurrentFileOffset = 0;
	    }
	    for (offset = 0; offset < stbuf.st_size; offset += BLOCK_SIZE) {
		offset -= residue;
		if (!RecordLevelIndex) NextICurrentFileOffset = ICurrentFileOffset = offset;
		toread = offset + BLOCK_SIZE >= stbuf.st_size ? stbuf.st_size - offset : BLOCK_SIZE;
		lseek(fd, offset, SEEK_SET);

		bx= buffer;
		num_read = 0;
		while ((toread > 0) && ((num_read = read(fd, bx, toread)) < toread)) {
		    if (num_read <= 0) {
			buffer = bx;
			fprintf(stderr, "read error on file %s at offset %d\n", LIST_GET(name_list, i), offset);
			goto break_break2;	/* C doesn't have break; break; */
		    }
		    bx += num_read;
		    toread -= num_read;
		}
		if (num_read >= toread) {
			bx += num_read;
			toread -= num_read;
		}
		buffer_end = bx;
		residue = 0;
		if (buffer_end == buffer_begin + BLOCK_SIZE) {
			if (RecordLevelIndex) {
				buffer_end = backward_delimiter(buffer_end /* NOT bx */, buffer, rdelim, rdelim_len, 0);
			}
			else {
				while ((INDEXABLE(*(buffer_end-1))) && (buffer_end > buffer_begin + MAX_WORD_SIZE)) buffer_end --;
			}
			residue = buffer_begin + BLOCK_SIZE - buffer_end;
			/* if (residue > 0) printf("residue = %d in %s at %d\n", residue, LIST_GET(name_list, i), offset); */
		}
		if (RecordLevelIndex) {
			next_record = forward_delimiter(buffer, buffer_end, rdelim, rdelim_len, 0);
		}
		bx = buffer;
 
		PrintedLongWordWarning = 0;
		while ((buffer=(unsigned char *) getword(LIST_GET(name_list, i), word, buffer, buffer_end, &attribute, &next_record)) < buffer_end) {
		    if (RecordLevelIndex) {
			if (buffer >= next_record) {
			    next_record = forward_delimiter(buffer, buffer_end, rdelim, rdelim_len, 0);
			    if (StoreByteOffset) ICurrentFileOffset += next_record - buffer;
			    else ICurrentFileOffset ++;
			}
		    }
		    /* printf("%s\n", word); */
		    if(word[0] == '\0') continue;
		    if(icount - hash_icount >= I_THRESHOLD) {
#if	BG_DEBUG
			fprintf(LOGFILE, "reached I_THRESHOLD at %d\n", icount - hash_icount);
#endif	/*BG_DEBUG*/
			traverse1();
			init_hash_table();
			hash_icount = icount;
		    }
		    insert_h(word, pn, attribute);
		}
		if (word[0] != '\0') {
		    /* printf("%s\n", word); */
		    if(icount - hash_icount >= I_THRESHOLD) {
#if	BG_DEBUG
			fprintf(LOGFILE, "reached I_THRESHOLD at %d\n", icount - hash_icount);
#endif	/*BG_DEBUG*/
			traverse1();
			init_hash_table();
			hash_icount = icount;
		    }
		    insert_h(word, pn, attribute);
		}
		if (RecordLevelIndex) {
			if (buffer >= next_record) {
			    /* next_record = forward_delimiter(buffer, buffer_end, rdelim, rdelim_len, 0); */
			    ICurrentFileOffset ++;
			}
		}
		buffer = buffer_begin;
		next_record = buffer;
	    }

	break_break2:
            close(fd);
	    if (unlinkname != NULL) unlink(unlinkname);

#ifdef	UDI_DEBUG
	    printf("add to index: %d\n",icount-save_icount);
#endif
	    if ((MAXWORDSPERFILE > 0) && (icount-save_icount > MAXWORDSPERFILE)) {
		fprintf(MESSAGEFILE, "%d words are contributed by %s\n",
			icount-save_icount, LIST_GET(name_list, i));
		AddedMaxWordsMessage = ON;
	    }

	    if (IndexNumber && NUMERICWORDPERCENT && (numeric_icount * 100 > (icount - save_icount) * NUMERICWORDPERCENT) && (icount - save_icount > MIN_WORDS)) {
		fprintf(MESSAGEFILE, "NUMBERS occur in %d%% of %d words contributed by %s\n", (numeric_icount * 100)/(icount - save_icount), icount - save_icount, LIST_GET(name_list, i));
		AddedMixedWordsMessage = ON;
	    }

	    numeric_icount=0;
	    save_icount=icount;
	    if (StructuredIndex) region_destroy();
        }
    }
    traverse1();
    init_hash_table();
    hash_icount = icount;
    my_free(buffer_begin, BLOCK_SIZE + 10);
}

init_hash_table()
{
    int i;

    for(i=0; i<HashTableSize; i++) hash_table[i] = (struct token *)NULL;
    return;
}


/* ------------------------------------------------------------------------
input: a word (a string), a hash table (each entry points to a list of
       tokens. (a token is a structure containing 'word' and a pointer
       to a list of indices)).
function: insert the word to appropriate position in the table.
          if the inserted word is already in the data structure, then
          update the list of indices corresponding to that 'word'.
          otherwise create a new token.
THERE ARE NO STATE CHANGES UNLESS WE ARE SURE THAT MALLOCS WON'T FAIL: BG
---------------------------------------------------------------------------*/
void
insert_h(word, pn, attribute)
char *word;
int  pn;
int attribute;
{
    int hash_value=0;
    struct token *tp;
    struct token *tp_bak;
    struct indices *iip;
    int  wordlen = strlen(word);
    int j;
    static int tried_once = 0;

    /* all words with same attribute at same place in hash table */
    hash_value = hashword(word, wordlen);
    tp_bak = tp = hash_table[hash_value];
    while(tp != NULL) {
        if((strcmp(word, tp->word) == 0) && (tp->attribute == attribute)) {
             insert_index(tp, pn);
	     tried_once = 0;
             return;	/* already in there */
        }
	tp_bak = tp;
        tp = tp->next_t;
    }

    /* this is a new word, insert it */
    if((tp = (struct token *) tokenalloc(sizeof(struct token))) == NULL) {
	tp_bak = NULL;
	if (tried_once == 1) {
		fprintf(stderr, "not enough memory in insert_h1 at icount=%d. skipping...\n", icount);
		tried_once = 0;
		return;	/* ignore word altogether */
	}
	traverse1();
	init_hash_table();
	tried_once = 1;	/* memory allocation failed in malloc#1 */
	insert_h(word, pn, attribute);	/* next call can't fail here since traverse() calls *allfree() */
        return;
    }

    if((tp->word = (char *) wordalloc(sizeof(char) * (wordlen+1))) == NULL) {
	tp_bak = NULL;
	if (tried_once == 2) {
		fprintf(stderr, "not enough memory in insert_h2 at icount=%d. skipping...\n", icount);
		tokenfree(tp, sizeof(struct token));
		tried_once = 0;
		return;	/* ignore word altogether */
	}
	tokenfree(tp, sizeof(struct token));
	traverse1();
	init_hash_table();
	tried_once = 2;	/* memory allocation failed in malloc#2 */
	insert_h(word, pn, attribute);	/* next call can't fail here or above since traverse() calls *allfree() */
        return;
    }
    strcpy(tp->word, word);
    tp->attribute = attribute;

    /* the index list has a first index */
    if((iip = (struct indices *) indicesalloc(sizeof(struct indices))) == NULL) {
	tp_bak = NULL;
	if (tried_once == 3) {
		fprintf(stderr, "not enough memory in insert_h3 at icount=%d. skipping...\n", icount);
		wordfree(tp->word, wordlen + 1);
		tokenfree(tp, sizeof(struct token));
		tried_once = 0;
		return;	/* ignore word altogether */
	}
	wordfree(tp->word, wordlen + 1);
	tokenfree(tp, sizeof(struct token));
	traverse1();
	init_hash_table();
	tried_once = 3;	/* memory allocation failed in malloc#3 */
	insert_h(word, pn, attribute);	/* next call can't fail here or above or above-above since traverse() calls *allfree() */
        return;
    }
    icount++;

    if (IndexNumber && NUMERICWORDPERCENT) {
	int	i=0;
	while(word[i] != '\0') {
	    if (!isalpha(((unsigned char *)word)[i])) break;
	    i++;
	}
	if (word[i] != '\0') numeric_icount ++;
    }

#ifdef SW_DEBUG
    if((icount & 01777) == 0) printf("icount = %d\n", icount);
#endif

    if (!CountWords) {
	iip->index[0] = pn;
	iip->offset[0] = ICurrentFileOffset;
    }
    for (j=1; j<INDEX_SET_SIZE; j++)
	iip->index[j] = INDEX_ELEM_FREE;

    /* assign both head and tail */
    iip->next_i = NULL;
    tp->ip = iip;
    tp->lastip = iip;

    if(tp_bak == NULL) hash_table[hash_value] = tp;
    else  tp_bak->next_t = tp;
    tp->next_t = NULL;
    tp->totalcount = 1;
    tried_once = 0;	/* now sure that there has been no memory allocation failure while inserting this word */
    return;
}

/* -------------------------------------------------------------------
insert_index(): insert an index, i.e., pn, into an indices structure.
The indices structure is a linked list where the 'first' one is always
the active indices structure. When the active one is filled with 8 indices
an indicies structure is created and becomes the active one.
tp points to the token structure. so, tp->ip is always the active
indices structure.
THERE ARE NO STATE CHANGES UNLESS WE ARE SURE THAT MALLOCS WON'T FAIL: BG
------------------------------------------------------------------- */
void
insert_index(tp, pn)
struct token *tp;               /* insert a index into a indices structure */
int pn;
{
    struct indices *iip, *temp;
    struct indices *ip = (ByteLevelIndex ? tp->lastip : tp->ip);
    static int tried_once = 0;
    int j;

    if (CountWords) {	/* I am not interested in maintaining where a word occurs: only the number of times it occurs */
	ip->offset[0] ++;
	return;
    }

    /* Check for stop-list */
    if (OneFilePerBlock && !ByteLevelIndex && (file_num > MaxNum8bPartition) && (tp->totalcount > (file_num * MAX_INDEX_PERCENT / 100))) return;
    if (ByteLevelIndex && (tp->totalcount > ( (((total_size>>20) > 0) && ((total_size>>20)*MAX_PER_MB < MAX_ALL_INDEX)) ? ((total_size>>20) * MAX_PER_MB) : MAX_ALL_INDEX) )) return;

    if (ByteLevelIndex) {
	for (j=INDEX_SET_SIZE; j>0; j--) {
	    if(ip->index[j-1] == INDEX_ELEM_FREE) continue;
	    if ((ip->index[j-1] == pn) && (ip->offset[j-1] == ICurrentFileOffset)) return;	/* in identical position */
	    else break;
	}
    }
    else {
	for (j=INDEX_SET_SIZE; j>0; j--) {
	    if(ip->index[j-1] == INDEX_ELEM_FREE) continue;
	    if (ip->index[j-1] == pn) return;	/* current word is not the first appearance in partition pn */
	    else break;
	}
    }
    /* ip->index[j] is the place to insert new pn provided j < INDEX_SET_SIZE */
    if(j < INDEX_SET_SIZE) {
	ip->offset[j] = ICurrentFileOffset;
        ip->index[j] = pn;
        return;
    }

    if((iip = (struct indices *) indicesalloc(sizeof(struct indices)))==NULL) {
	if (tried_once == 1) {
		fprintf(stderr, "not enough memory in insert_index at icount=%d. skipping...\n", icount);
		tried_once = 0;
		return;	/* ignore index altogether */
	}
	traverse1();
	init_hash_table();
	tried_once = 1;	/* memory allocation failed in malloc#1 */
	insert_index(tp, pn);
        return;
    }
    icount++;

    if (ByteLevelIndex) {
	/* insert at the end */
	tp->lastip->next_i = iip;
	iip->next_i = NULL;
	tp->lastip = iip;
    }
    else {
	iip->next_i = tp->ip;
	tp->ip = iip;
    }

    iip->offset[0] = ICurrentFileOffset;
    iip->index[0] = pn;
    for (j=1; j<INDEX_SET_SIZE; j++)
	iip->index[j] = INDEX_ELEM_FREE;
    tp->totalcount ++;
    if ( (OneFilePerBlock && !ByteLevelIndex && (file_num > MaxNum8bPartition) && (tp->totalcount > (file_num * MAX_INDEX_PERCENT / 100))) ||
	    (ByteLevelIndex && (tp->totalcount > ( (((total_size>>20) > 0) && ((total_size>>20)*MAX_PER_MB < MAX_ALL_INDEX)) ? ((total_size>>20) * MAX_PER_MB) : MAX_ALL_INDEX) )) ) {
	for (iip=tp->ip; iip != NULL; temp = iip, iip = iip->next_i, indicesfree(temp, sizeof(struct indices)));
	tp->ip = NULL;	/* never need to insert anything else here */
    }
/*
printf("returning from insert_index()\n");
fflush(stderr);
*/
    tried_once = 0;
    return;
}


/* Scan the indexed "word" from an index line: see io.c/merge_splits() */
scanword(word, buffer, buffer_end, attr)
unsigned char *word, *buffer, *buffer_end;
unsigned int *attr;
{
    int i = MAX_WORD_SIZE;
    while ((i-- != 0) && (buffer <= buffer_end) && (*buffer != ALL_INDEX_MARK) && (*buffer != WORD_END_MARK) && (*buffer != '\n') && (*buffer != '\0'))
	*word ++ = *buffer ++;
    *word = '\0';
    *attr = encode16b(0);
    if (StructuredIndex) {
	if ((*buffer == ALL_INDEX_MARK) || (*buffer == WORD_END_MARK)) {
	    buffer ++;
	    *attr = ((*buffer) << 8) | (*(buffer + 1));
	}
    }
}

/* Globals used in merge, and also in glimpse's main.c */
extern unsigned int *src_index_set;
extern unsigned int *dest_index_set;
extern unsigned char *src_index_buf;
extern unsigned char *dest_index_buf;
extern unsigned char *merge_index_buf;

/* merge index file f1 and f2, then put the result in index file f3 */
merge_in(f1, f2, f3)
FILE *f1, *f2, *f3;
{
    int src_mark, dest_mark;
    int	src_num, dest_num;
    int src_end_pt, dest_end_pt;
    int cmp=0; /* the result of strcmp */
    int bdx, bdx1, bdx2, merge_len, i, j;
    int TAIL1=0;
    char word1[MAX_WORD_SIZE+6];	/* used only for strcmp() */
    char word2[MAX_WORD_SIZE+6];	/* used only for strcmp() */
    unsigned int attr1, attr2;
    int x=0, y=0, even_words = 1;

    int merge_buf_size=REAL_INDEX_BUF;


    /* LOOK OUT FOR: [memset, fgets, endpt-forloop, scanword] 4-tuples: invariant */

#if	debug
printf("in merge_in()\n"); fflush(stdout);
#endif
    memset(dest_index_buf, '\0', REAL_INDEX_BUF);
    if (fgets(dest_index_buf, REAL_INDEX_BUF, f2) == NULL) dest_index_buf[0] = '\0';
    else dest_index_buf[REAL_INDEX_BUF - 1] = '\0';
    dest_end_pt = strlen(dest_index_buf);
    scanword(word2, dest_index_buf, dest_index_buf+dest_end_pt, &attr2);

#ifdef debug
    printf("word2 = %s\n", word2);
#endif
    memset(src_index_buf, '\0', REAL_INDEX_BUF);
    while(fgets(src_index_buf, REAL_INDEX_BUF, f1)) {
	src_index_buf[REAL_INDEX_BUF - 1] = '\0';
	src_end_pt = strlen(src_index_buf);
    	scanword(word1, src_index_buf, src_index_buf+src_end_pt, &attr1);

#ifdef debug
	printf("word1 = %s\n", word1);
#endif
	while (((cmp = strncmp(word1, word2, MAX_WORD_SIZE+4)) > 0) || (StructuredIndex && (cmp == 0) && (attr1 > attr2))) {
	    fputs(dest_index_buf, f3);

	    memset(dest_index_buf, '\0', dest_end_pt+2);
	    if(fgets(dest_index_buf, REAL_INDEX_BUF, f2) == NULL) {
		dest_index_buf[REAL_INDEX_BUF - 1] = '\0';
		TAIL1 = ON;
		break;
	    }
	    dest_index_buf[REAL_INDEX_BUF - 1] = '\0';
	    dest_end_pt = strlen(dest_index_buf);
    	    scanword(word2, dest_index_buf, dest_index_buf+dest_end_pt, &attr2);
	}
        if(TAIL1 == ON) break;
        if ((cmp == 0) && (attr1 == attr2)) { /* we need to join the index of word1 and word2 */
#ifdef debug
	    printf("joining src_index_buf and dest_index_buf\n");
	    printf("src_index_buf = %s", src_index_buf);
	    printf("dest_index_buf = %s", dest_index_buf);
#endif

	    if (!CountWords && !ByteLevelIndex) {	/* have to look for common indices and exclude them */
		    int oldbdx1, oldbdx2;

		    merge_index_buf[0] = '\0';
		    merge_len = 0;

		    oldbdx1 = bdx1 = 0;
		    /* src_index_buf[src_end_pt] is '\0', src_index_buf[src_end_pt-1] is '\n' */
		    while((bdx1 < src_end_pt) && (src_index_buf[bdx1] != WORD_END_MARK) && (src_index_buf[bdx1] != ALL_INDEX_MARK)) bdx1 ++;
		    if ((bdx1 > oldbdx1) && (bdx1 < src_end_pt)) {	/* src_index_buf[bdx1] is the word-end-mark */
			src_mark = src_index_buf[bdx1];
			src_index_buf[bdx1] = '\0';	/* terminate word */
			strcpy(merge_index_buf, src_index_buf);	/* save the word itself */
			merge_len = strlen(src_index_buf);	/* merge_index_buf[merge_len] is '\0', merge_index_buf[merge_len-1] is a part of the word */
			bdx1 ++;		/* skip word end marker */
			if (StructuredIndex) bdx1 += 2;	/* skip attribute field */
		    }

		    even_words = 1;
		    src_num = 0;
		    if (OneFilePerBlock) memset((char *)src_index_set, '\0', sizeof(int)*REAL_PARTITION);
		    else memset((char *)src_index_set, '\0', sizeof(int) * (MAX_PARTITION + 1));
		    while(bdx1 < src_end_pt - 1) {
			if (OneFilePerBlock) {
			  x = 0;
			  if (file_num <= MaxNum8bPartition) {
			    x = decode8b(src_index_buf[bdx1]);
			    bdx1 ++;
			  }
			  else if (file_num <= MaxNum12bPartition) {
			    if (even_words) {
				x = ((src_index_buf[bdx1+1] & 0x0000000f) << 8) | src_index_buf[bdx1];
				x = decode12b(x);
				bdx1 += 2;
				even_words = 0;
			    }
			    else {	/* odd number of words so far */
				x = ((src_index_buf[bdx1-1] & 0x000000f0) << 4) | src_index_buf[bdx1];
				x = decode12b(x);
				bdx1 ++;
				even_words = 1;
			    }
			  }
			  else if (file_num <= MaxNum16bPartition) {
			    x = (src_index_buf[bdx1] << 8) | src_index_buf[bdx1+1];
			    x = decode16b(x);
			    bdx1 += 2;
			  }
			  else {
			    x = (src_index_buf[bdx1] << 16) | (src_index_buf[bdx1+1] << 8) | src_index_buf[bdx1+2];
			    x = decode24b(x);
			    bdx1 += 3;
			  }
			  src_index_set[block2index(x)] |= mask_int[x % (8*sizeof(int))];
			  src_num ++;
			}
			else src_index_set[src_num++] = src_index_buf[bdx1++];
		    }

		    oldbdx2 = bdx2 = 0;
		    /* dest_index_buf[dest_end_pt] is '\0', dest_index_buf[dest_end_pt-1] is '\n' */
		    while((bdx2 < dest_end_pt) && (dest_index_buf[bdx2] != WORD_END_MARK) && (dest_index_buf[bdx2] != ALL_INDEX_MARK)) bdx2 ++;
		    if ((bdx2 > oldbdx2) && (bdx2 < dest_end_pt)) {	/* dest_index_buf[bdx2] is the word-end-mark */
			dest_mark = dest_index_buf[bdx2];
			dest_index_buf[bdx2] = '\0';	/* terminate word */
			if (merge_len == 0) {
			    strcpy(merge_index_buf, dest_index_buf);	/* save the word itself */
			    merge_len = strlen(merge_index_buf);
			    /* merge_index_buf[merge_len] is '\0', merge_index_buf[merge_len-1] is a part of the word */
			}
			bdx2 ++;		/* skip word end marker */
			if (StructuredIndex) bdx2 += 2;	/* skip attribute field */
		    }

		    even_words = 1;
		    dest_num = 0;
		    if (OneFilePerBlock) memset((char *)dest_index_set, '\0', sizeof(int)*REAL_PARTITION);
		    else memset((char *)dest_index_set, '\0', sizeof(int) * (MAX_PARTITION + 1));
		    while(bdx2 < dest_end_pt - 1) {
			if (OneFilePerBlock) {
			  x = 0;
			  if (file_num <= MaxNum8bPartition) {
			    x = decode8b(dest_index_buf[bdx2]);
			    bdx2 ++;
			  }
			  else if (file_num <= MaxNum12bPartition) {
			    if (even_words) {
				x = ((dest_index_buf[bdx2+1] & 0x0000000f) << 8) | dest_index_buf[bdx2];
				x = decode12b(x);
				bdx2 += 2;
				even_words = 0;
			    }
			    else {	/* odd number of words so far */
				x = ((dest_index_buf[bdx2-1] & 0x000000f0) << 4) | dest_index_buf[bdx2];
				x = decode12b(x);
				bdx2 ++;
				even_words = 1;
			    }
			  }
			  else if (file_num <= MaxNum16bPartition) {
			    x = (dest_index_buf[bdx2] << 8) | dest_index_buf[bdx2+1];
			    x = decode16b(x);
			    bdx2 += 2;
			  }
			  else {
			    x = (dest_index_buf[bdx2] << 16) | (dest_index_buf[bdx2+1] << 8) | dest_index_buf[bdx2+2];
			    x = decode24b(x);
			    bdx2 += 3;
			  }
			  dest_index_set[block2index(x)] |= mask_int[x % (8*sizeof(int))]; 
			  dest_num ++;
			}
			else dest_index_set[dest_num++] = dest_index_buf[bdx2++];
		    }

		    even_words = 1;
		    /* prevent buffer overflows */	
		    if (merge_len > 0) {

			if (merge_len > merge_buf_size - 6) {
				merge_buf_size += 20;
				merge_index_buf = realloc(merge_index_buf, merge_buf_size);
fprintf(stderr,"Realloc + 20 new size is %d",merge_buf_size);
				memset(merge_index_buf + merge_len,'\0',merge_buf_size-merge_len);
			}

			if(OneFilePerBlock &&
			   ((src_mark == ALL_INDEX_MARK) ||
			    (dest_mark == ALL_INDEX_MARK) ||
			    ((file_num > MaxNum8bPartition) && (src_num + dest_num > file_num*MAX_INDEX_PERCENT / 100)) )) {
			    merge_index_buf[merge_len++] = ALL_INDEX_MARK;
			    if (StructuredIndex) {
				merge_index_buf[merge_len++] = (attr1 & 0xff00) >> 8;
				merge_index_buf[merge_len++] = (attr1 & 0xff);
			    }
			    if (file_num <= MaxNum8bPartition)
				merge_index_buf[merge_len ++] = encode8b(DONT_CONFUSE_SORT);
			    else if (file_num <= MaxNum12bPartition) {
				merge_index_buf[merge_len ++] = (encode12b(DONT_CONFUSE_SORT) & 0x00000f00) >> 8;
				merge_index_buf[merge_len ++] = encode12b(DONT_CONFUSE_SORT) & 0x000000ff;
			    }
			    else if (file_num <= MaxNum16bPartition) {
				merge_index_buf[merge_len ++] = (encode16b(DONT_CONFUSE_SORT) & 0x0000ff00) >> 8;
				merge_index_buf[merge_len ++] = encode16b(DONT_CONFUSE_SORT) & 0x000000ff;
			    }
			    else {
				merge_index_buf[merge_len ++] = (encode24b(DONT_CONFUSE_SORT) & 0x00ff0000) >> 16;
				merge_index_buf[merge_len ++] = (encode24b(DONT_CONFUSE_SORT) & 0x0000ff00) >> 8;
				merge_index_buf[merge_len ++] = encode24b(DONT_CONFUSE_SORT) & 0x000000ff;
			    }
			    goto final_merge;
			}

			merge_index_buf[merge_len++] = WORD_END_MARK;
			if (StructuredIndex) {
			    merge_index_buf[merge_len++] = (attr1 & 0xff00) >> 8;
			    merge_index_buf[merge_len++] = (attr1 & 0xff);
			}
			if (OneFilePerBlock) {
			    for (i=0; i<round(file_num, 8*sizeof(int)); i++) dest_index_set[i] |= src_index_set[i];	/* take union */
			    for (i=0; i<round(file_num, 8*sizeof(int)); i++) {
				if (merge_len > (merge_buf_size - 3*8*sizeof(int) - 1)) {
					merge_buf_size *= 2;
					merge_index_buf = realloc(merge_index_buf, merge_buf_size);
fprintf(stderr,"Had to Realloc merge buffer (#2), new size is %d\n",merge_buf_size);
					memset(merge_index_buf + merge_len,'\0',merge_buf_size-merge_len);
				}
				if (dest_index_set[i])
				    for (j=0; j<8*sizeof(int); j++)
					if (dest_index_set[i] & mask_int[j]) {
						x = i*8*sizeof(int) + j;
						if (file_num <= MaxNum8bPartition) {
							merge_index_buf[merge_len++] = encode8b(x);
						}
						else if (file_num <= MaxNum12bPartition) {
						    x = encode12b(x);
						    if (even_words) {
							merge_index_buf[merge_len++] = x & 0x000000ff;	/* lsb */
							y = (x & 0x00000f00)>>8;	/* msb */
							even_words = 0;
						    }
						    else {	/* odd number of words so far */
							y |= (x&0x00000f00)>>4; /* msb of x into msb of y */
							merge_index_buf[merge_len ++] = y;
							merge_index_buf[merge_len ++] = x&0x000000ff;
							even_words = 1;
						    }
						}
						else if (file_num <= MaxNum16bPartition) {
							x = encode16b(x);
							merge_index_buf[merge_len ++] = (x&0x0000ff00)>>8;
							merge_index_buf[merge_len ++] = x&0x000000ff;
						}
						else {
							x = encode24b(x);
							merge_index_buf[merge_len ++] = (x&0x00ff0000)>>16;
							merge_index_buf[merge_len ++] = (x&0x0000ff00)>>8;
							merge_index_buf[merge_len ++] = x&0x000000ff;
						}
					}
			    }
			    if (!even_words && (file_num > MaxNum8bPartition) && (file_num <= MaxNum12bPartition))
				merge_index_buf[merge_len ++] = y;
			}
			else {	/* normal indexing */
			    for (i=0; i<src_num; i++) {
				merge_index_buf[merge_len++] = src_index_set[i];
				 /* Prevent buffer overflow */
				if (merge_len > (merge_buf_size - 3)) {
					merge_buf_size *= 2;
					merge_index_buf = realloc(merge_index_buf, merge_buf_size);
fprintf(stderr,"Had to Realloc merge buffer (#3), new size is %d\n",merge_index_buf);
					memset(merge_index_buf + merge_len,'\0',merge_buf_size-merge_len);
				}
			    }

			    for (j=0; j<dest_num; j++) {
				for (i=0; i<src_num; i++)
				    if (dest_index_set[j] == src_index_set[i]) break;
				if (i>=src_num)     /* did not find match */
				    merge_index_buf[merge_len++] = dest_index_set[j];

				 /* Prevent buffer overflow */
				if (merge_len > (merge_buf_size - 2)) {
					merge_buf_size *= 2;
					merge_index_buf = realloc(merge_index_buf, merge_buf_size);
fprintf(stderr,"Realloc #4, new size is %d\n",merge_index_buf);
					memset(merge_index_buf + merge_len,'\0',merge_buf_size-merge_len);
				}
				
				/* Doesn't matter if dest_index_set is int-array (merge_index_buf being char array) since dest_index_set has only a char */
			    }
			}

		    final_merge:
			merge_index_buf[merge_len++] = '\n';
			merge_index_buf[merge_len] = '\0';
			fputs(merge_index_buf, f3);
			/* fprintf(stderr, "%d+%d=%d ", src_end_pt, dest_end_pt, merge_len); */
		    } /* merge_len > 0 */
	    }
	    else if (CountWords) {	/* indices are frequencies, so just merge them: OneFilPerBlock is ignored */
                    strcpy(merge_index_buf, src_index_buf);
                    bdx = strlen(merge_index_buf);      /* merge_index_buf[bdx] is '\0', merge_index_buf[bdx-1] is '\n' */
                    if (bdx > 1) bdx--; /* now merge_index_buf[bdx] is '\n', merge_index_buf[bdx-1] is the last index  */
                    bdx2 = 0;

                    /* find the first index */
                    if (IndexNumber) while(isalnum(dest_index_buf[bdx2])) bdx2 ++;
                    else while(isalpha(dest_index_buf[bdx2])) bdx2++;

                    /* to skip over the word-end marker of dest_index_buf (which is a blank) */
                    if (bdx2 > 0) bdx2 ++;
		    if (StructuredIndex) bdx2 += 2;	/* this is a nop since CountWords and StructuredIndex don't make sense together */

                    if (bdx >= 1) {
                        merge_index_buf[bdx++] = ' ';   /* blank separated fscanf-able list of integers representing counts */
                    }

                    /* append the indices of word1 to the buffer */
                    if (dest_index_buf[bdx2] > 0) {
                        while((dest_index_buf[bdx2]>0)&&(bdx2<REAL_INDEX_BUF)) merge_index_buf[bdx++] = dest_index_buf[bdx2++];  /* '\n' gets copied */
                        merge_index_buf[bdx] = '\0';
                    }
                    /* else, no need to copy anything */
                    fputs(merge_index_buf, f3);
	    }
	    else {	/* indices are actual occurrences (ByteLevelIndex), so just cat them one after the other, src first since that is i2, the 1st one */
		/* First put out the word */
		bdx1 = 0;
		while ((bdx1<src_end_pt) && (src_index_buf[bdx1] != WORD_END_MARK) && (src_index_buf[bdx1] != ALL_INDEX_MARK) && (src_index_buf[bdx1] != '\n') && (src_index_buf[bdx1] != '\0'))
		    putc(src_index_buf[bdx1 ++], f3);

		/* Now check what end-mark we should put */
		if ((bdx1 >= src_end_pt) || (src_index_buf[bdx1] == ALL_INDEX_MARK) || (src_end_pt + dest_end_pt >= MAX_SORTLINE_LEN)) {
		    putc(ALL_INDEX_MARK, f3);
		    if (StructuredIndex) {
			putc((attr1&0xff00) >> 8, f3);
			putc((attr1&0xff), f3);
		    }
		    putc(DONT_CONFUSE_SORT, f3);
		    putc('\n', f3);
		}
		else {	/* dest can be all index mark */
		    bdx2 = 0;
		    while ((bdx2<dest_end_pt) && (dest_index_buf[bdx2] != WORD_END_MARK) && (dest_index_buf[bdx2] != ALL_INDEX_MARK) && (dest_index_buf[bdx2] != '\n') && (dest_index_buf[bdx2] != '\0')) bdx2 ++;
		    if ((bdx2 >= dest_end_pt) || (dest_index_buf[bdx2] == ALL_INDEX_MARK)) {
			putc(ALL_INDEX_MARK, f3);
			if (StructuredIndex) {
			    putc((attr1&0xff00) >> 8, f3);
			    putc((attr1&0xff), f3);
			}
			putc(DONT_CONFUSE_SORT, f3);
			putc('\n', f3);
		    }
		    else {	/* we have to put out both the lists */
			putc(WORD_END_MARK, f3);
			bdx1 ++;	/* skip over WORD_END_MARK */
			if (StructuredIndex) {
			    putc((attr1&0xff00) >> 8, f3);
			    putc((attr1&0xff), f3);
			    bdx1 += 2;
			}
			while ((bdx1 < src_end_pt) && (src_index_buf[bdx1] != '\n') && (src_index_buf[bdx1] != '\0')) putc(src_index_buf[bdx1++], f3);
			fputc(encode8b(0), f3);	/* instead of the '\n' after end of src_index_buf */
			bdx2 ++;	/* skip over WORD_END_MARK */
			if (StructuredIndex) bdx2 += 2;
			while ((bdx2 < dest_end_pt) && (dest_index_buf[bdx2] != '\n') && (dest_index_buf[bdx2] != '\0')) putc(dest_index_buf[bdx2++], f3);
			putc('\n', f3);
		    }
		}
	    }
#if debug
	    printf("merge_index_buf = %s", merge_index_buf);
#endif /*debug*/
	    memset(dest_index_buf, '\0', dest_end_pt+2);
 	    if(fgets(dest_index_buf, REAL_INDEX_BUF, f2) == 0)  {
		dest_index_buf[REAL_INDEX_BUF - 1] = '\0';
		TAIL1 = ON;
		break;
	    }
	    dest_index_buf[REAL_INDEX_BUF - 1] = '\0';
	    dest_end_pt = strlen(dest_index_buf);
    	    scanword(word2, dest_index_buf, dest_index_buf+dest_end_pt, &attr2);
        }
        else { /* word1 < word2, so output src_index_buf */
            fputs(src_index_buf, f3);
        }
	memset(src_index_buf, '\0', src_end_pt+2);
    }
    if(TAIL1) {
        if(cmp != 0) fputs(src_index_buf, f3);
	memset(src_index_buf, '\0', src_end_pt+2);
        while(fgets(src_index_buf, REAL_INDEX_BUF, f1)) {
	    src_index_buf[REAL_INDEX_BUF - 1] = '\0';
	    src_end_pt = strlen(src_index_buf);
            fputs(src_index_buf, f3);
	    memset(src_index_buf, '\0', src_end_pt+2);
        }
    }
    else { /* output the tail of f2 */
        fputs(dest_index_buf, f3); 
	memset(dest_index_buf, '\0', dest_end_pt+2);
        while(fgets(dest_index_buf, REAL_INDEX_BUF, f2)) {
	    dest_index_buf[REAL_INDEX_BUF - 1] = '\0';
	    dest_end_pt = strlen(dest_index_buf);
            fputs(dest_index_buf, f3);
	    memset(dest_index_buf, '\0', dest_end_pt+2);
        }
    }
    return;
}

remove_filename(fileindex, new_partition)
	int	fileindex, new_partition;
{
	if ((fileindex < 0) || (fileindex >= MaxNum24bPartition)) return;
#if	BG_DEBUG
	fprintf(LOGFILE, "removing %s from index\n", LIST_GET(name_list, fileindex));
	memory_usage -= (strlen(LIST_GET(name_list, fileindex)) + 2);
#endif	/*BG_DEBUG*/
	my_free(LIST_GET(name_list, fileindex), 0);
	LIST_SUREGET(name_list, fileindex) = NULL;
	if ((disable_list != NULL) && (fileindex < old_file_num)) disable_list[block2index(fileindex)] |= mask_int[fileindex % (8*sizeof(int))];
}

/* returns the set of deleted files in the struct indices format: note that by construction, the list is sorted according to fileindex */
struct indices*
get_removed_indices()
{
	int	i, j;
	char	*name;
	struct indices *head = NULL, **tail, *new;

	tail = &head;
	for (i=0; i<file_num; i++) {
		name=LIST_GET(name_list, i);
#if	0
		if ((name == NULL) || (name[0] == '\0')) printf("DEL %d\n", i);
#endif
		if ((name == NULL) || (name[0] == '\0')) {
			if ((*tail == NULL) || ((*tail)->index[INDEX_SET_SIZE - 1] != INDEX_ELEM_FREE)) {
				new = (struct indices *)my_malloc(sizeof(struct indices));
				memset(new, '\0', sizeof(struct indices));
				for (j=0; j<INDEX_SET_SIZE; j++) new->index[j] = INDEX_ELEM_FREE;
				if (*tail != NULL) {
					(*tail)->next_i = new;
					tail = &(*tail)->next_i;
				}
				else head = new;
			}
			for (j=0; j<INDEX_SET_SIZE; j++) if ((*tail)->index[j] == INDEX_ELEM_FREE) break;
			/* j must be < INDEX_SET_SIZE */
			(*tail)->index[j] = i;
		}
	}
	return head;
}

/* returns a -ve number if there is no newfileindex for this file (deleted from index), or the new index otherwise */
/* length_of_deletedlist = MaxNum24bPartition + 1 - get_new_index(deletedlist, MaxNum24bPartition + 1); */
get_new_index(deletedlist, oldfileindex)
	struct indices	*deletedlist;
	int	oldfileindex;
{
	int	j;
	int	reduction = 0;
	struct indices *head = deletedlist;
	while (head!=NULL) {
		for (j=0; j<INDEX_SET_SIZE; j++) {
			if (head->index[j] == INDEX_ELEM_FREE) return oldfileindex;	/* crossed the limit */
			else if (oldfileindex == head->index[j]) return -1;	/* oldfileindex has been deleted now */
			else if (oldfileindex > head->index[j]) oldfileindex --;
			else /* oldfileindex < head->index[j] */ return oldfileindex;	/* no more have been deleted before oldfileindex */
		}
		head = head->next_i;
	}
	return oldfileindex;	/* crossed the limit */
}

delete_removed_indices(deletedlist)
	struct indices *deletedlist;
{
	int	j;
	int	reduction = 0;
	struct indices *head = deletedlist, *temp;
	while (head!=NULL) {
		temp = head;
		head = head->next_i;
		my_free(temp, sizeof(struct indices));
	}
	return reduction;
}

/* The savings in index space are not worth it if you call this function w/o OneFilePerBlock: returns new_file_num (after purging index of deleted files) */
int
purge_index()
{
	int	new_file_num;
	int	i, firsti;	/* firsti used to eliminate dead words from index */
	char s[MAX_LINE_LEN], es1[MAX_LINE_LEN], es2[MAX_LINE_LEN], temp_rdelim[MAX_LINE_LEN];
	char s1[MAX_LINE_LEN];
	int j;
	unsigned char c;
	FILE *i_in;
	FILE *i_out;
	int offset, index;
	char indexnumberbuf[256];
	int	onefileperblock, structuredindex;
	int	even_words, dest_even_words;
	int	delim = encode8b(0);
	int	x, y, new_x;
	int	prevy, diff, oldj;

	if (!OneFilePerBlock) return file_num;
#if	0
	/* initialized in glimpse.c now */
	if ((deletedlist = get_removed_indices()) == NULL) return file_num;
	printf("into purge_index()\n");
#endif
	new_file_num = file_num - ( MaxNum24bPartition + 1 - get_new_index(deletedlist, MaxNum24bPartition + 1) );	/* since encoding may change */
	/* else, we have already done merge-splits and the result is in INDEX_FILE */
#if	0
	printf("purging indexing from %d to %d files\n", file_num, new_file_num);
#endif

	sprintf(s, "%s/%s", INDEX_DIR, INDEX_FILE);
	if((i_in = fopen(s, "r")) == NULL) {
		fprintf(stderr, "can't open %s for reading\n", s);
		exit(2);
	}
        sprintf(s, "%s/.glimpse_merge.%d", INDEX_DIR, getpid());
        if ((i_out = fopen(s, "w")) == NULL) {
                fprintf(stderr, "cannot open for writing: %s\n", s);
                exit(2);
        }

	/* modified the original in glimpse's main.c */
	fgets(indexnumberbuf, 256, i_in);
	fputs(indexnumberbuf, i_out);
	fscanf(i_in, "%%%d\n", &onefileperblock);
	if (ByteLevelIndex) fprintf(i_out, "%%-%d\n", new_file_num);        /* #of files might have changed due to -f/-a */
	else fprintf(i_out, "%%%d\n", new_file_num);        /* This was the stupidest thing of all! */
	if ( !fscanf(i_in, "%%%d\n", &structuredindex) )  /* New file format may not have delimiter.  Fixed 10/25/99 as per mhubin. --GV */
             fscanf(i_in, "%%%d%s\n", &structuredindex, temp_rdelim);
	if (structuredindex <= 0) structuredindex = 0;
	if (RecordLevelIndex) fprintf(i_out, "%%-2 %s\n", old_rdelim);     /* robint@zedcor.com */
	else fprintf(i_out, "%%%d\n", attr_num);     /* attributes might have been added during last merge */

        while (fgets(src_index_buf, REAL_INDEX_BUF, i_in)) {
            i = 0;
	    j = 0;
            while ((j < REAL_INDEX_BUF) && (src_index_buf[j] != WORD_END_MARK) && (src_index_buf[j] != ALL_INDEX_MARK) && (src_index_buf[j] != '\0') && (src_index_buf[j] != '\n'))
		dest_index_buf[i++] = src_index_buf[j++];
            if ((j >= REAL_INDEX_BUF) || (src_index_buf[j] == '\0') || (src_index_buf[j] == '\n')) continue;
            /* else it is WORD_END_MARK or ALL_INDEX_MARK */
	    c = src_index_buf[j];
	    dest_index_buf[i++] = src_index_buf[j++];
            if (structuredindex) {      /* convert all attributes to 2B to make merge_in()s easy in build_in.c */
                if (structuredindex < MaxNum8bPartition - 1) {
		    dest_index_buf[i++] = src_index_buf[j++];
                }
                else {
		    dest_index_buf[i++] = src_index_buf[j++];
		    dest_index_buf[i++] = src_index_buf[j++];
                }
            }
            if (c == ALL_INDEX_MARK) {
		dest_index_buf[i++] = DONT_CONFUSE_SORT;
		dest_index_buf[i++] = '\n';
		dest_index_buf[i] = '\0';
		fputs(dest_index_buf, i_out);
                continue;
            }
            /* src_index_buf[j] points to the first byte of the block numbers, dest_index_buf[i] points to the first empty byte to fill up */
	    firsti = i;

	    /*
	     * Main while loop: Copied from glimpse/get_index.c/get_set() with minor modifications.
	     */
	    even_words = 1;
	    dest_even_words = 1;
	    while((j<REAL_INDEX_BUF) && (src_index_buf[j] != '\n') && (src_index_buf[j] != '\0')) {
		/* First get the file name */
		x = 0;
		if (ByteLevelIndex) {
		    if (OneFilePerBlock <= MaxNum8bPartition) {
			x = decode8b(src_index_buf[j]);
			j ++;
		    }
		    else if (OneFilePerBlock <= MaxNum16bPartition) {
			x = (src_index_buf[j] << 8) | src_index_buf[j+1];
			x = decode16b(x);
			j += 2;
		    }
		    else {
			x = (src_index_buf[j] << 16) | (src_index_buf[j+1] << 8) | src_index_buf[j+2];
			x = decode24b(x);
			j += 3;
		    }
		}
		else if (OneFilePerBlock <= MaxNum8bPartition) {
		    x = decode8b(src_index_buf[j]);
		    j ++;
		}
		else if (OneFilePerBlock <= MaxNum12bPartition) {
		    if (even_words) {
			x = ((src_index_buf[j+1] & 0x0000000f) << 8) | src_index_buf[j];
			x = decode12b(x);
			j += 2;
			even_words = 0;
		    }
		    else {	/* odd number of words so far */
			x = ((src_index_buf[j-1] & 0x000000f0) << 4) | src_index_buf[j];
			x = decode12b(x);
			j ++;
			even_words = 1;
		    }
		}
		else if (OneFilePerBlock <= MaxNum16bPartition) {
		    x = (src_index_buf[j] << 8) | src_index_buf[j+1];
		    x = decode16b(x);
		    j += 2;
		}
		else {
		    x = (src_index_buf[j] << 16) | (src_index_buf[j+1] << 8) | src_index_buf[j+2];
		    x = decode24b(x);
		    j += 3;
		}

		/*
		 * This if-decoder: Copied from glimpse/get_index.c/get_set() with minor modifications.
		 */
		if ((new_x = get_new_index(deletedlist, x)) >= 0) {	/* since encoding may change */
		    if (ByteLevelIndex) {
			if (new_file_num <= MaxNum8bPartition) {
			    x = encode8b(new_x);
			    dest_index_buf[i++] = (x&0x000000ff);
			}
			else if (new_file_num <= MaxNum16bPartition) {
			    x = encode16b(new_x);
			    dest_index_buf[i++] = ((x&0x0000ff00)>>8);
			    dest_index_buf[i++] = (x&0x000000ff);
			}
			else {
			    x = encode24b(new_x);
			    dest_index_buf[i++] = ((x&0x00ff0000)>>16);
			    dest_index_buf[i++] = ((x&0x0000ff00)>>8);
			    dest_index_buf[i++] = (x&0x000000ff);
			}
		    } /* ByteLevelIndex */
		    else if (OneFilePerBlock) {
			if (new_file_num <= MaxNum8bPartition) {
				dest_index_buf[i++] = (encode8b(new_x));
			}
			else if (new_file_num <= MaxNum12bPartition) {
			    x = encode12b(new_x);
			    if (dest_even_words) {
				dest_index_buf[i++] = (x & 0x000000ff);	/* lsb */
				y = (x & 0x00000f00)>>8;	/* msb */
				dest_even_words = 0;
			    }
			    else {	/* odd number of words so far */
			        y |= (x&0x00000f00)>>4; /* msb of x into msb of y */
				dest_index_buf[i++] = (y);
				dest_index_buf[i++] = (x&0x000000ff);
				dest_even_words = 1;
			    }
			}
			else if (file_num <= MaxNum16bPartition) {
				x = encode16b(new_x);
				dest_index_buf[i++] = ((x&0x0000ff00)>>8);
				dest_index_buf[i++] = (x&0x000000ff);
			}
			else {
				x = encode24b(new_x);
				dest_index_buf[i++] = ((x&0x00ff0000)>>16);
				dest_index_buf[i++] = ((x&0x0000ff00)>>8);
				dest_index_buf[i++] = (x&0x000000ff);
			}
		    } /* OneFilePerBlock */
		}
		/* else don't put anything */

		prevy = 0;
		if (ByteLevelIndex) {
		    while ((j<REAL_INDEX_BUF) && (src_index_buf[j] != '\n') && (src_index_buf[j] != '\0')) {
			oldj = j;
			y = decode8b(src_index_buf[j]);
			if ((y & 0x000000c0) == 0) {	/* one byte offset */
			    diff = y&0x0000003f;
			    y = prevy + diff;
			    j ++;
			}
			else if ((y & 0x000000c0) == 0x40) {	/* two byte offset */
			    diff = decode8b(src_index_buf[j+1]);
			    y = prevy + (((y & 0x0000003f) * MaxNum8bPartition) + diff);
			    j += 2;
			}
			else if ((y & 0x000000c0) == 0x80) {	/* three byte offset */
			    diff = decode16b((src_index_buf[j+1] << 8) | src_index_buf[j+2]);
			    y = prevy + (((y & 0x0000003f) * MaxNum16bPartition) + diff);
			    j += 3;
			}
			else {	/* four byte offset */
			    diff = decode24b((src_index_buf[j+1] << 16) | (src_index_buf[j+2] << 8) | src_index_buf[j+3]);
			    y = prevy + (((y & 0x0000003f) * MaxNum24bPartition) + diff);
			    j += 4;
			}
			prevy = y;
			if (new_x >= 0) {
			    memcpy(&dest_index_buf[i], &src_index_buf[oldj], j - oldj);
			    i += j - oldj;
			}

			if ((j<REAL_INDEX_BUF) && (src_index_buf[j] == delim)) {	/* look at offsets corr. to a new file now */
				if (new_x >= 0) {
					dest_index_buf[i++] = src_index_buf[j++];
				}
				else j ++;
				break;
			}
		    }
		    /* else don't put anything */
		}
	    }
	    if (!ByteLevelIndex && OneFilePerBlock && !dest_even_words && (new_file_num > MaxNum8bPartition) && (new_file_num <= MaxNum12bPartition))
		dest_index_buf[i++] = (y);
	    if (firsti < i) {
		dest_index_buf[i++] = '\n';
		dest_index_buf[i] = '\0';
		fputs(dest_index_buf, i_out);
	    }
	    else {	/* word with no references, so ditch it: fill up junk for safety */
		dest_index_buf[i] = '\n';
		dest_index_buf[i+1] = '\n';
	    }
	}
        fclose(i_in);
        fflush(i_out);
        fclose(i_out);
#if	SFS_COMPAT
	sprintf(s, "%s/.glimpse_merge.%d", INDEX_DIR, getpid());
	sprintf(s1, "%s/%s", INDEX_DIR, INDEX_FILE);
	rename(s, s1);
#else
        sprintf(s, "exec %s '%s/.glimpse_merge.%d' '%s/%s'", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), getpid(), escapesinglequote(INDEX_DIR, es2), INDEX_FILE);
        system(s);
#endif

	return new_file_num;
}

/* This does not effect global variables other than disable_list: used only in incremental indexing */
initialize_disable_list(files)
int	files;
{
	int	_FILEMASK_SIZE = ((files + 1)/(8*sizeof(int)) + 4);
	int	_REAL_PARTITION = (_FILEMASK_SIZE + 4);
	if (disable_list == NULL) disable_list = (unsigned int *)my_malloc(sizeof(int)*_REAL_PARTITION);
	memset(disable_list, '\0', sizeof(int) * _REAL_PARTITION);	/* nothing is disabled initially */
}
