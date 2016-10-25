/* Copyright (c) 1994 Sun Wu, Udi Manber, Burra Gopal.  All Rights Reserved. */
/* ./glimpse/index/pirs.h */

#include <autoconf.h>	/* include configured defs */

#ifndef _GLIMPSE_H_
#define _GLIMPSE_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
/*#include <sys/uio.h>*/
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#undef log
#include "agrep.h"

#ifndef S_ISREG
/* #define S_ISREG(mode) (0100000&(mode)) */
#define S_ISREG(mode)   (((mode) & (_S_IFMT)) == (_S_IFREG))
#endif

#ifndef S_ISDIR
/* #define S_ISDIR(mode) (0040000&(mode)) */
#define S_ISDIR(mode)   (((mode) & (_S_IFMT)) == (_S_IFDIR))
#endif

#define IC_PORTRELEASE	20	/* time till used TCP port is released */
#ifndef ON
#define ON		1
#endif
#ifndef OFF
#define OFF		0
#endif
#ifndef CHAR
#define CHAR 		unsigned char
#endif
#define MAX_INCLUSIVE	256	/* max number of inclusive patterns for
				   files to be indexed even if filetype.c
				   says otherwise. */
#define MAX_EXCLUSIVE	256 	/* max number of exclusive patterns 
				   for not_to_be_indexed files  */
#define MAX_FILTER	256	/* max number of filter patterns */
#define DEF_I_THRESHOLD	40000	/* 100000 originally, debugging 10000 */
#define AVG_OCCURRENCES	8	/* #of places a word occurs on average: sizeof(.glimpse_partitions)/`wc -l .glimpse_index`: divisible by INDEX_SET_SIZE */
#define MAX_LIST	0177777
#define DEFAULT_PART_SIZE (1 << 13)
#define MAX_64K_HASH	(64*1024)
#define MAX_256K_HASH	(256*1024)
#define MAX_4K_HASH	(4*1024)

#define DISKBLOCKSIZE	8192
#define BLOCK_SIZE	(1024*64)
#define MAX_PARTITION	255
#define MaxNumPartition	250     /* it's not 255, since there is fragmentation*/

/* The idea behind our encoding is: dividend = divisor * quotient + remainder */
#define MaxNum4bPartition (16 -  2)	/* since 10 and 0 can't be in LSB/MSB */
#define MaxNum8bPartition (256 - 2)
#define MaxNum12bPartition (MaxNum4bPartition*MaxNum8bPartition)
#define MaxNum16bPartition (MaxNum8bPartition*MaxNum8bPartition)
#define MaxNum24bPartition (MaxNum8bPartition*MaxNum16bPartition)
#define MaxNum32bPartition (MaxNum8bPartition*MaxNum24bPartition)

/* These help in encoding byte-level indices: 1st byte's top 2 bits tell the #of bytes - 1 in offset-difference encoding; offset-diff 0 => new file follows */
#define MaxNum1BPartition (MaxNum8bPartition & 0x3f)			/* 62: top byte is 0x00 | x % MaxNum8bPartition === x; just encode x */
#define MaxNum2BPartition (MaxNum1BPartition * MaxNum8bPartition)	/* top byte = 0x40 | x / MaxNum8bPartition; rest is x % ~; encode both separately */
#define MaxNum3BPartition (MaxNum1BPartition * MaxNum16bPartition)	/* top byte = 0x80 | x / MaxNum16bPartition; rest is x % ~; encode both separately */
#define MaxNum4BPartition (MaxNum1BPartition * MaxNum24bPartition)	/* top byte = 0xc0 | x / MaxNum24bPartition; rest is x % ~; encode both separately */

#define DEF_NUMERIC_WORD_PERCENT 50	/* warn if > this many % of words added by file are numeric */
#define MIN_WORDS		50	/* before we inform about numeric words */
#define MAX_SEARCH_PERCENT	20	/* warn user if searching > this % of blocks */
#define DEF_MAX_INDEX_PERCENT	80	/* if word in > 80%, say everywhere for one-file-per-block */
#define DONT_CONFUSE_SORT 1
#define WORD_END_MARK 	2
#define ALL_INDEX_MARK	3		/* If this, then word is in > 60% of blocks */
#define ATTR_END_MARK	4		/* After list of attributes before file offset/block numbers */

#define AVG_WORD_LEN	12		/* average word length is 8-9 including '\0': have safety margin */
#define MAX_NAME_SIZE   256
#define MAX_NAME_LEN	MAX_NAME_SIZE
#define MaxNameLength	MAX_NAME_SIZE
#define MAX_LINE_SIZE	1024
#define MAX_LINE_LEN	1024
#define MAX_SORTLINE_LEN (MAX_LINE_LEN * 16)	/* Can be ((MaxNum16bPartition*sizeof(int)+MAX_NAME_LEN)*MAX_INDEX_PERCENT/100) in the worst case */
#define MAX_NAME_BUF	MAX_NAME_SIZE
#define MAX_WORD_SIZE	64	/* w/o '\0'; was 24 in 2.1 */
#define MAX_WORD_LEN	MAX_WORD_SIZE
#define MAX_WORD_BUF	80	/* was 32 in 2.1 */
#define MAX_PAT		256  
#define MAXNUM_INDIRECT	MaxNum8bPartition
#define MAX_INDEX_BUF	(MAX_PARTITION + 1 + 2*MAX_WORD_BUF + 2)	/* index line length without OneFilePerBlock */
#define DEF_REAL_INDEX_BUF	(MaxNum16bPartition*3  + 2*MAX_WORD_BUF + 2)	/* index line length with OneFilePerBlock */

/* Must write fresh code to calculate these sets based by multiplying defaults below with round(file_num, MaxNum16bPartition) */
#define DEF_FILESET_SIZE	MaxNum16bPartition	/* used when OneFilePerBlock is ON */
#define DEF_FILEMASK_SIZE	(DEF_FILESET_SIZE/(8*sizeof(int)) + 4)	/* bit mask of files */
#define DEF_REAL_PARTITION	(DEF_FILEMASK_SIZE + 4)	/* must be > MAX_PARTITION + 1 */
/* block must be in 0..DEF_FILESET_SIZE-1, and integers should represent bit-masks */
#define block2index(i)	(i/(8*sizeof(int)))
#define block2mask(i)	(1<<(i%(8*sizeof(int))))	/* not used */
#define round(x, y)	(((x)+(y)-1)/(y))
#define FILES_PER_PARTITION(x)	(16 + round(x, MAX_PARTITION)*16)	/* 16 is minimum length of buffer: thereafter, allow noise upto 16 times average */

#define LIST_GET(list, elem) ((list[(elem)/MaxNum16bPartition] == 0) ? (0) : (list[(elem)/MaxNum16bPartition][(elem)%MaxNum16bPartition]))
#define LIST_SUREGET(list, elem) (list[(elem)/MaxNum16bPartition][(elem)%MaxNum16bPartition])

#define LIST_ADD(list, elem, what, type) \
{\
	int	index = (elem /*+ 1*/)/MaxNum16bPartition;\
	if (list[index] == NULL) {\
		list[index] = (type *)malloc(sizeof(type)*MaxNum16bPartition);\
		memset(list[index], '\0', sizeof(type)*MaxNum16bPartition);\
	}\
	LIST_SUREGET(list, elem) = what;\
}

#define DEFAULT_REGION_LIMIT 256	/* default limit for a record: for ByteLevelIndex: pattern is ignored since can't avoid false matches w/o search */
#define MAX_REGION_LIMIT Max_record	/* max amount of space I am going allocate for a record bounded by a delimiter: was 16384! Fixed -bg */
#define MAX_PER_LINE	(MAX_SORTLINE_LEN / 2)	/* #of words that can occur on one line before we split it up: not implemented at present */
#define DEF_MAX_PER_MB	500	/* Maximum number of times a word should occur in a megabyte before we say its everywhere */
#define DEF_ALL_INDEX	10000	/* Must be < DEF_MAX_ALL_INDEX */
#define DEF_MAX_ALL_INDEX	(DEF_REAL_INDEX_BUF / 2)	/* THIS * 2 must be < DEF_REAL_INDEX_BUF to prevent seg-faults! */

/* Default file names */
/* temporarily changed filter configuration file name to avoid
   conflicts between stable and experimental versions of glimpse. --CV
   9/14/99 */
#define FILTER_FILE	".glimpse_filters"

#define ATTRIBUTE_FILE	".glimpse_attributes"
#define INDEX_FILE	".glimpse_index"
#define MINI_FILE	".glimpse_turbo"
#define P_TABLE		".glimpse_partitions"
#define NAME_LIST	".glimpse_filenames"
#define NAME_LIST_INDEX	".glimpse_filenames_index"
#define NAME_HASH	".glimpse_filehash"
#define NAME_HASH_INDEX	".glimpse_filehash_index"
#define DEF_TIME_FILE	".glimpse_filetimes"
#define DEF_LOG_FILE	".glimpse_log"
#define DEF_MESSAGE_FILE ".glimpse_messages"
#define DEF_STAT_FILE	".glimpse_statistics"
#define PROHIBIT_LIST	".glimpse_exclude"
#define INCLUDE_LIST	".glimpse_include"
#define DEBUG_FILE	".glimpse_debug"
#define I2		".glimpse_tmpi2"
#define I3		".glimpse_tmpi3"
#define I1		".glimpse_tmpi1"
#define O1		".glimpse_tmpo1"
#define O2		".glimpse_tmpo2"
#define O3		".glimpse_tmpo3"
#define DEF_LOCK_FILE	".glimpse_lock"
#define HARVEST_PREFIX	"glimpse"	/* so that Darren can filterout error messages a user should see from the stuff outputted by glimpse on an error */

#define MASK_INT \
{ 0x00000001, 0x00000002, 0x00000004, 0x00000008, 0x00000010, 0x00000020, 0x00000040, 0x00000080,\
  0x00000100, 0x00000200, 0x00000400, 0x00000800, 0x00001000, 0x00002000, 0x00004000, 0x00008000,\
  0x00010000, 0x00020000, 0x00040000, 0x00080000, 0x00100000, 0x00200000, 0x00400000, 0x00800000,\
  0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000, 0x20000000, 0x40000000, 0x80000000\
}

#define INDEXABLE(c)	(indexable_char[c])

#if	SFS_COMPAT
#define IGNORED_SUFFIXES {".glimpse_filehash", ".glimpse_filehash.prev", ".glimpse_filehash_index", ".glimpse_filehash_index.prev", ".glimpse_filenames", ".glimpse_filenames.prev", ".glimpse_filenames_index", ".glimpse_filenames_index.prev", ".glimpse_filetimes", ".glimpse_index", ".glimpse_partitions", ".glimpse_statistics", ".glimpse_messages", ".glimpse_exclude", ".glimpse_include", ".glimpse_filters", ".glimpse_attributes", ".glimpse_turbo"}
#define NUM_SUFFIXES	18
#else
#define IGNORED_SUFFIXES {"gz", "Z", "z", "zip", "o", "hqx", "tar", "glimpse_times", "glimpse_index", "glimpse_partitions"}
#define NUM_SUFFIXES	10
#endif

#define EXTRACT_INFO_SUFFIX {".htm", ".html", ".shtm", ".shtml", ".jhtml", ".phtml",".HTM",".HTML", ".abra"}
#define NUM_EXTRACT_INFO_SUFFIX	9

/* Version and release year: same for glimpse and glimspeindex since glimpse HAS to interpret glimpseindex */
#define GLIMPSE_VERSION	"4.18.7"
#define GLIMPSE_DATE	"2015"
#define GLIMPSE_EMAIL	"gvelez17@gmail.com"
#define GLIMPSE_URL	"http://webglimpse.net/"

/* Some extern functions used in structured queries */
extern int attr_name_to_id(), attr_load_names(), attr_dump_names();
extern char *attr_id_to_name();

/* Data structures for hash-tables in build_in.c */
struct  token {			/* each token stores a unique word and unique attribute */	
        struct token *next_t;	/* keep it a pointer even with tokenalloc to keep build_in.c same */
	char *word; 			
	struct indices *ip;	/* points to the head of the list of indices */
	struct indices *lastip;	/* tail of this list = last elemet (for increasing order insertion) */
	unsigned int attribute;
	unsigned int totalcount;/* no. of indices structures in a token */
        };

#define INDEX_SET_SIZE	4
#define INDEX_ELEM_FREE	(MaxNum24bPartition + 1)	/* can never be equal to a partition value */

struct indices {
	struct indices *next_i;	/* keep it a pointer even with indexalloc to keep build_in.c same */
        /*unsigned*/ int index[INDEX_SET_SIZE]; 	/* changed from char, 31/3/94 */
	/*unsigned*/ int offset[INDEX_SET_SIZE];	/* added 19/9/94 */
	};

/* Added 20/9/94 for get_index.c in glimpse (make it more efficient in space later) */
struct offsets {
	struct offsets *next;
	int offset;	/* NOT unsigned!!! */
	short	sign;	/* if 0, then indeterminate (bothways), 1 then +ve, -1 then -ve */
	short	done;	/* if 0, then this did not have an intersection now, else it has had it */
	};

#define INDICES_PER_TOKEN	(AVG_OCCURRENCES/INDEX_SET_SIZE)	/* average no. of struct indices per struct token: purely empirical result :-) */

/* Memory allocators: in io.c */
extern char *my_malloc();
extern int my_free();
extern FILE *my_fopen();
extern int my_open(), my_stat(), my_lstat();
extern char *wordalloc();
extern int wordfree();
extern int allwordfree();
extern struct indices *indicesalloc();
extern int indicesfree();
extern int allindicesfree();
extern struct token *tokenalloc();
extern int tokenfree();
extern int alltokenfree();

#define LIMIT_64K_HASH	50	/* size of total stuff to be indexed in MB after which 256K hash tables make more sense with the -B option */
#define hashword(word, wordlen)	(((total_size < LIMIT_64K_HASH*1024*1024) || !BigHashTable) ? (hash64k(word, wordlen)) : (hash256k(word, wordlen)));

/*
 * Just stores the word, wordlength and offset present in a line of the index in a structure (when made with -o or -b).
 * Doesn't store the attribute since we just need a hint into .glimpse_index from where agrep should begin search.
 */
#define	WORD_SORTED	0

#if	WORD_SORTED
struct mini {
	char	*word;
	long	offset;
};

/* Region searched with strcmp. #of regions = mini_array_len = (`wc -l .glimpse_index` - 3) / WORDS_PER_REGION */
#define WORDS_PER_REGION	128
#else	/* WORD_SORTED */
struct mini {
	long	offset;
};

/* Range of each mini_array entry is words with same hash32k value => 32K offsets into the index need to be stored */
#define MINI_ARRAY_LEN		(64*1024)
#endif	/* WORD_SORTED */

/* For incremental indexing only */
typedef struct _name_hashelement {
	struct _name_hashelement *next;
	char	*name;
	int	name_len;
	int	index;
} name_hashelement;

/*
 * Limit on number of files is MaxNum24bPartition. To change it, you need
 * to add encode/decode code everywhere, INDEX_ELEM_FREE and MAXNUM_INDIRECT.
 *
 * Limit on number of attributes is MaxNum16bPartition. To change it, you
 * need to add encode/decode code everywhere. That is: merge_splits(),
 * save_data_structures(), traverse(), merge_in() and scanword()
 * in glimpseindex; get_set() in glimpse; and printx.c.
 *
 * No need to change any other data structures.
 */

/* Names of various system commands used in glimpseindex: use mv/rm etc rather than rename()/unlink() since former don't return unless parent-dir is sync-ed */
#define SYSTEM_SORT	"sort"	/* replace with different sort with longer lines. Later write a procedure for sort that doesn't need system() */
#define SYSTEM_LS	"ls"
#define SYSTEM_MV	"mv"	/* this doesn't work with SFS */
#define SYSTEM_RM	"rm"	/* this doesn't work with SFS */
#define SYSTEM_CAT	"cat"
#define SYSTEM_HEAD	"head"
#define SYSTEM_CP	"cp"
#define SYSTEM_ECHO	"echo"
#define SYSTEM_WC	"wc"
#define SYSTEM_AWK	"awk"	/* used at present only in "cast" package */
extern char *escapesinglequote();
#endif /* _GLIMPSE_H_ */
