/* Copyright (c) 1994 Sun Wu, Udi Manber, Burra Gopal.  All Rights Reserved. */
/* bgopal: (1993-4) redesigned/rewritten using agrep's library interface */
#include <autoconf.h>
#include <sys/param.h>
#include <errno.h>
#include "glimpse.h"
#include "defs.h"
#include <fcntl.h>
#include "checkfile.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>	/* for flock definition */
#if	ISO_CHAR_SET
#include <locale.h>	/* support for 8bit character set */
#endif

#define CLIENTSERVER	1
#define USE_MSGHDR	0
#define USE_UNIXDOMAIN	0
#define DEBUG	0

#define DEF_SERV_PORT	2001
#define MIN_SERV_PORT	1024
#define MAX_SERV_PORT	30000
#define SERVER_QUEUE_SIZE	10	/* number of requests to buffer up while processing one request = 5 */

/* Borrowed from C-Lib */
extern char **environ;
extern int errno;

#if	CLIENTSERVER
#include "communicate.c"
#endif	/*CLIENTSERVER*/

/* For client-server protocol */
CHAR	*SERV_HOST = NULL;
int	SERV_PORT;
char	glimpse_reqbuf[MAX_ARGS*MAX_NAME_LEN];
extern int glimpse_clientdied;	/* set if signal received about dead socket: need agrep variable so that exec() can return quickly */
int	glimpse_reinitialize = 0;

/* Borrowed from agrep.c */
extern int D_length;		/* global variable in agrep */
extern int D;			/* global variable in agrep */
extern int pattern_index;
/* These are used for byte level index search */
extern CHAR CurrentFileName[MAX_LINE_LEN];
extern int SetCurrentFileName;
extern int CurrentByteOffset;
extern int SetCurrentByteOffset;
extern long CurrentFileTime;
extern int SetCurrentFileTime;
extern int execfd;
extern int  agrep_initialfd;
extern CHAR *agrep_inbuffer;
extern int  agrep_inlen;
extern int  agrep_inpointer;
extern FILE *agrep_finalfp;
extern CHAR *agrep_outbuffer;
extern int  agrep_outlen;
extern int  agrep_outpointer;
extern int glimpse_call;	/* prevent agrep from printing out its usage */
extern int glimpse_isserver;	/* prevent agrep from asking for user input */
int	first_search = 1;	/* intra/interaction in process_query() and glimpse_search() */
#if	ISSERVER
int	RemoteFiles = 0;	/* Are the files present locally or remotely? If on, then -NQ is automatically added to all search options for each query */
#endif

/* Borrowed from index/io.c */
extern int InfoAfterFilename;
extern int OneFilePerBlock;
extern int StructuredIndex;
extern unsigned int *dest_index_set;
extern unsigned char *dest_index_buf;
extern unsigned int *src_index_set;
extern unsigned char *src_index_buf;
extern unsigned char *merge_index_buf;
extern int mask_int[32];
extern int indexable_char[256];
int test_indexable_char[256];
extern int p_table[MAX_PARTITION];
extern int GMAX_WORD_SIZE;
extern int IndexNumber;		/* used in getword() */
extern int InterpretSpecial;	/* used to "not-split" agrep-regexps */
extern int UseFilters;		/* defined in build_in.c, used for filtering routines in io.c */
extern int ByteLevelIndex;
extern int RecordLevelIndex;
extern int rdelim_len;
extern char rdelim[MAX_LINE_LEN];
extern char old_rdelim[MAX_LINE_LEN];
extern int file_num;
extern int REAL_PARTITION, REAL_INDEX_BUF, MAX_ALL_INDEX, FILEMASK_SIZE;

/* Borrowed from get_filename.c */
extern int bigbuffer_size;
extern char *bigbuffer;
extern char *outputbuffer;

/* OPTIONS/FLAGS */
int	veryfast = 0;
int	CONTACT_SERVER = 0;	/* Should client try to call server at all or just process query on its own? */
int	NOBYTELEVEL = 0;	/* Some cases where we cannot do byte level fast-search: ALWAYS 0 if !ByteLevelIndex */
int	OPTIMIZEBYTELEVEL = 0;	/* Some cases where we don't want to do byte level search since number of files is small */
int	GCONSTANT = 0;		/* should pattern be taken as-is or parsed? */
int	GLIMITOUTPUT = 0;	/* max no. of output lines: 0=>infinity=default=nolimit */
int	GLIMITTOTALFILE = 0;	/* max no. of files to match: 0=>infinity=default=nolimit */
int	GLIMITPERFILE = 0;	/* not used in glimpse */
int	GBESTMATCH = 0;		/* Should I change -B to -# where # = no. of errors? */
int	GRECURSIVE = 0;
int	GNOPROMPT = 0;
int	GBYTECOUNT = 0;
int	GPRINTFILENUMBER = 0;
int	GPRINTFILETIME = 0;
int	GOUTTAIL = 0;
int	GFILENAMEONLY = 0;	/* how to do it if it is an and expression in structured queries */
int	GNOFILENAME=0;
int	GPRINTNONEXISTENTFILE = 0; /* if filename is not there in index, then at least let user know its name */
int	MATCHFILE = 0;
int	PRINTATTR = 0;
int	PRINTINDEXLINE = 0;
int	Pat_as_is=0;
int	Only_first=0;		/* Do index search only */
int	PRINTAPPXFILEMATCH=0;	/* Print places in file where match occurs: useful with -b only to analyse the index */
int	GCOUNT=0;		/* print number of matches rather than actual matches: used only when PRINTAPPX = 1 */
int	HINTSFROMUSER=0;	/* The user gives the hints about where we should search (result of adding -EQNgy) */
int	WHOLEFILESCOPE=0;	/* used only when foundattr is NOT set: otherwise, scope is whole file anyway */
int	foundattr=0;		/* set in split.c -- != 0 only when StructuredIndex AND query is structured */
int	foundnot=0;		/* set in split.c -- != 0 only when the not operator (~) is present in the pattern */
int	FILENAMESINFILE=0;	/* whether the user is providing an explicit list of filenames to be searched for pattern (if absent, then means all files) */
int	BITFIELDFILE=0;		/* Based on contribution From ada@mail2.umu.se Fri Jul 12 01:56 MST 1996; Christer Holgersson, Sen. SysNet Mgr, Umea University/SUNET, Sweden */
int	BITFIELDOFFSET=0;
int	BITFIELDLENGTH=0;
int	BITFIELDENDIAN=0;
int	GNumDays = 0;		/* whether the user wants files modified within these many days before creating the index: only >0 makes sense */

/* structured queries */
CHAR	***attr_vals;		/* matrix of char pointers: row=max #of attributes, col=max possible values */
CHAR	**attr_found;		/* did the expression corr. to each value in attr_vals match? */
ParseTree *GParse;		/* what kind of expression corr. to attr are we looking for */

/* arbitrary booleans */
ParseTree terminals[MAXNUM_PAT];	/* parse tree's terminal node pointers pt. to elements of this array; also used outside */ 
char	matched_terminals[MAXNUM_PAT];	/* ...[i] is 1 if i'th terminal matched: used in filter_output and eval_tree */
int	num_terminals;		/* number of terminal patterns */
int	ComplexBoolean=0;	/* 1 if we need to use parse trees and the eval function */

/* index search */
CHAR	*pat_list[MAXNUM_PAT];	/* complete words within global pattern */
int	pat_lens[MAXNUM_PAT];	/* their lengths */
int	pat_attr[MAXNUM_PAT];	/* set of attributes */
int	is_mgrep_pat[MAXNUM_PAT];
int	mgrep_pat_index[MAXNUM_PAT];
int	num_mgrep_pat;
CHAR	pat_buf[(MAXNUM_PAT + 2)*MAXPAT];
int	pat_ptr = 0;
extern char INDEX_DIR[MAX_LINE_LEN];
char	*TEMP_DIR = NULL;	/* directory to store glimpse temporary files, usually /tmp unless -T is specified */
char	indexnumberbuf[256];	/* to read in first few lines of the index */
char	*index_argv[MAX_ARGS];
int	index_argc = 0;
int	bestmatcherrors=0;	/* set during index search, used later on */
int	patindex; 
int	patbufpos = -1;
char	tempfile[MAX_NAME_LEN];
char	*filenames_file = NULL;
char	*bitfield_file = NULL;

/* agrep search */
char	*agrep_argv[MAX_ARGS];
int 	agrep_argc = 0;
CHAR	*FileOpt;		/* the option list after -F */
int	fileopt_length;
CHAR	GPattern[MAXPAT];
int	GM;
CHAR	APattern[MAXPAT];
int	AM;
CHAR	GD_pattern[MAXPAT];
int	GD_length;
CHAR	**GTextfiles;
CHAR	**GTextfilenames;
int	*GFileIndex;
int	GNumfiles;
int	GNumpartitions;
CHAR	GProgname[MAXNAME];

/* persistent file descriptors */
#if	BG_DEBUG
FILE *debug; 			/* file descriptor for debugging output */
#endif	/*BG_DEBUG*/
FILE	*timesfp = NULL;
FILE	*timesindexfp = NULL;
FILE	*indexfp = NULL;	/* glimpse index */
FILE	*partfp = NULL;		/* glimpse partitions */
FILE	*minifp = NULL;		/* glimpse turbo */
FILE	*nullfp = NULL;		/* to discard output: agrep -s doesn't work properly */
int	svstdin = 0, svstdout = 1, svstderr = 2;
static int one = 1;		/* to set socket option so that glimpseserver releases socket after death */

/* Index manipulation */
struct offsets **src_offset_table;
struct offsets **multi_dest_offset_table[MAXNUM_PAT];
unsigned int *multi_dest_index_set[MAXNUM_PAT];
extern free_list();
struct stat index_stat_buf, file_stat_buf;
int timesindexsize = 0;
int last_Y_filenumber = 0;

/* Direct agrep access for bytelevel-indices */
extern int COUNT, INVERSE, TCOMPRESSED, NOFILENAME, POST_FILTER, OUTTAIL, BYTECOUNT, SILENT, NEW_FILE,
	LIMITOUTPUT, LIMITPERFILE, LIMITTOTALFILE, PRINTRECORD, DELIMITER, SILENT, FILENAMEONLY, num_of_matched, prev_num_of_matched, FILEOUT;
CHAR	matched_region[MAX_REGION_LIMIT*2 + MAXPATT*2];
int	RegionLimit=DEFAULT_REGION_LIMIT;

/* Returns number of matched records/lines. Uses agrep's options to output stuff nicely; never called with RecordLevelIndex set */
int
glimpse_search(AM, APattern, GD_length, GD_pattern, realfilename, filename, fileindex, src_offset_table, outfp)
	int		AM;
	unsigned char	APattern[];
	int		GD_length;
	unsigned char	GD_pattern[];
	char		*realfilename;
	char		*filename;
	int		fileindex;
	struct offsets	*src_offset_table[];
	FILE		*outfp;
{
	FILE		*infp;
	char		sig[SIGNATURE_LEN];
	struct offsets	**p1, *tp1;
	CHAR		*text, *curtextend, *curtextbegin, c;
	int		times;
	int		num, ret=0, totalret = 0;
	int		prevoffset = 0, begininterval = 0, endinterval = -1;
	CHAR		*beginregionptr = 0, *endregionptr = 0;
	int		beginpage = 0, endpage = -1;
	static int	MAXTIMES, MAXPGTIMES, pagesize;
	static int	first_time = 1;

	/*
	 * If can't open file for read, quit
	 * For each offset for that file:
	 *	seek to that point
	 *	go back until delimiter, go forward until delimiter, output it: MAX_REGION_LIMIT is 16K on either side.
	 *	read in units of RegionLimit
	 *	before outputting matched record, use options to put prefixes (or use memagrep which does everything?)
	 * Algorithm changed: don't read same page in twice.
	 */

	if (first_time) {
		pagesize = DISKBLOCKSIZE;
		MAXTIMES = ((MAX_REGION_LIMIT / RegionLimit) > 1) ? (MAX_REGION_LIMIT / RegionLimit) : 1;
		MAXPGTIMES = ((MAX_REGION_LIMIT / pagesize) > 1) ? (MAX_REGION_LIMIT / pagesize) : 1;
		first_time = 0;
	}
	/* Safety: must end/begin with delim */
	memcpy(matched_region, GD_pattern, GD_length);
	memcpy(matched_region+MAXPATT+2*MAX_REGION_LIMIT, GD_pattern, GD_length);
	text = &matched_region[MAX_REGION_LIMIT+MAXPATT];

	if ((infp = my_fopen(filename, "r")) == NULL) return 0;
	NEW_FILE = ON;
#if	0
	/* Cannot search in .CZ files since offset computations will be incorrect */
	TCOMPRESSED = ON;
	if (!tuncompressible_filename(file_list[i], strlen(file_list[i]))) TCOMPRESSED = OFF;
	num_read = fread(sig, 1, SIGNATURE_LEN, infp);
	if ((TCOMPRESSED == ON) && tuncompressible(sig, num_read)) {
		EASYSEARCH = sig[SIGNATURE_LEN-1];
		if (!EASYSEARCH) {
			fprintf(stderr, "not compressed for easy-search: can miss some matches in: %s\n", CurrentFileName);	/* not filename!!! */
		}
	}
	else TCOMPRESSED = OFF;
#endif	/*0*/

	p1 = &src_offset_table[fileindex];
	while (*p1 != NULL) {
		if ( (begininterval <= (*p1)->offset) && (endinterval > (*p1)->offset) ) {	/* already covered this area */
#if	DEBUG
			printf("ignoring %d in [%d,%d]\n", (*p1)->offset, begininterval, endinterval);
#endif	/*DEBUG*/
			tp1 = *p1;
			*p1 = (*p1)->next;
			my_free(tp1, sizeof(struct offsets));
			continue;
		}

		TCOMPRESSED = OFF;
#if	1
		if ( (beginpage <= (*p1)->offset) && (endpage >= (*p1)->offset) && (text + ((*p1)->offset - prevoffset) + GD_length < endregionptr)) {
			/* beginregionptr = curtextend - GD_length;	/* prevent next curtextbegin to go behind previous curtextend (!) */
			text += ((*p1)->offset - prevoffset);
			prevoffset = (*p1)->offset;
			if (!((curtextend = forward_delimiter(text, endregionptr, GD_pattern, GD_length, 1)) < endregionptr))
				goto fresh_read;
			if (!((curtextbegin = backward_delimiter(text, beginregionptr, GD_pattern, GD_length, 0)) > beginregionptr))
				goto fresh_read;
		}
		else { /* NOT within an area already read: must read another page: if record overlapps page, might read page twice: no time to fix */
		fresh_read:
			prevoffset = (*p1)->offset;
			text = &matched_region[MAX_REGION_LIMIT+MAXPATT];	/* middle: points to occurrence of pattern */
			endpage = beginpage = ((*p1)->offset / pagesize) * pagesize;
			/* endpage = (((*p1)->offset + pagesize) / pagesize) * pagesize */
			endregionptr = beginregionptr = text - ((*p1)->offset - beginpage);	/* overlay physical place starting from this logical point */
			/* endregionptr = text + (endpage - (*p1)->offset); */
			curtextbegin = curtextend = text;
			times = 0;
			while (times < MAXPGTIMES) {
				fseek(infp, endpage, 0);
				num = (&matched_region[MAX_REGION_LIMIT*2+MAXPATT] - endregionptr < pagesize) ? (&matched_region[MAX_REGION_LIMIT*2+MAXPATT] - endregionptr) : pagesize;
				if ((num = fread(endregionptr, 1, num, infp)) <= 0) break;
				endpage += num;
				endregionptr += num;
				if (endregionptr <= text) {
					curtextend = text;	/* error in value of offset: file was modified and offsets no longer true: your RISK! */
					break;
				}
				if (((curtextend = forward_delimiter(text, endregionptr, GD_pattern, GD_length, 1)) < endregionptr) ||
				    (endregionptr >= &matched_region[MAX_REGION_LIMIT*2 + MAXPATT])) break;
				times ++;
			}
			times = 0;
			while (times < MAXPGTIMES) {	/* I have already read the initial page since endpage is beginpage initially */
				if ((curtextbegin = backward_delimiter(text, beginregionptr, GD_pattern, GD_length, 0)) > beginregionptr) break;
				if (beginpage > 0) {
					if (beginregionptr - pagesize < &matched_region[MAXPATT]) {
						if ((num = beginregionptr - &matched_region[MAXPATT]) <= 0) break;
					}
					else num = pagesize;
					beginpage -= num;
					beginregionptr -= num;
				}
				else break;
				times ++;
				fseek(infp, beginpage, 0);
				fread(beginregionptr, 1, num, infp);
			}
		}
#else	/*1*/
		/* Find forward delimiter (including delimiter) */
		times = 0;
		fseek(infp, (*p1)->offset, 0);
		while (times < MAXTIMES) {
			if ((num = fread(text+RegionLimit*times, 1, RegionLimit, infp)) > 0)
				curtextend = forward_delimiter(text, text+RegionLimit*times+num, GD_pattern, GD_length, 1);
			if ((curtextend < text+RegionLimit*times+num) || (num < RegionLimit)) break;
			times ++;
		}
		/* Find backward delimiter (including delimiter) */
		times = 0;
		while (times < MAXTIMES) {
			num = ((*p1)->offset - RegionLimit*(times+1)) > 0 ? ((*p1)->offset - RegionLimit*(times+1)) : 0;
			fseek(infp, num, 0);
			if (num > 0) {
				fread(text-RegionLimit*(times+1), 1, RegionLimit, infp);
				curtextbegin = backward_delimiter(text, text-RegionLimit*(times+1), GD_pattern, GD_length, 0);
			}
			else {
				fread(text-RegionLimit*times-(*p1)->offset, 1, (*p1)->offset, infp);
				curtextbegin = backward_delimiter(text, text-RegionLimit*times-(*p1)->offset, GD_pattern, GD_length, 0);
			}
			if ((num <= 0) || (curtextbegin > text-RegionLimit*(times+1))) break;
			times ++;
		}
#endif	/*1*/

		/* set interval and delete the entry */
		begininterval = (*p1)->offset - (text - curtextbegin);
		endinterval = (*p1)->offset + (curtextend - text); 

		if (strncmp(curtextbegin, GD_pattern, GD_length)) {
			/* always pass enclosing delimiters to agrep; since we have seen text before curtextbegin + we have space, we can overwrite */
			memcpy(curtextbegin - GD_length, GD_pattern, GD_length);
			curtextbegin -= GD_length;
		}
#if	DEBUG
		c = *curtextend;
		*curtextend = '\0';
		printf("%s [%d < %d < %d], text = %d: %s\n", CurrentFileName, begininterval, (*p1)->offset, endinterval, text, curtextbegin);
		*curtextend = c;
#endif	/*DEBUG*/

		tp1 = *p1;
		*p1 = (*p1)->next;
		my_free(tp1, sizeof(struct offsets));
		if (curtextend <= curtextbegin) continue;	/* error in offsets/delims */

		/*
		 * Don't call memagrep since that is heavy weight. Call exec
		 * directly after doing agrep_search()'s preprocessing here.
		 * PS: can add agrep variable not to do delim search if called from here
		 * since that prevents unnecessarily scanning the buffer for the 2nd time.
		 */
		CurrentByteOffset = begininterval+1;
		SetCurrentByteOffset = 1;
		first_search = 1;
		if (first_search) {
			if ((ret = memagrep_search(AM, APattern, curtextend-curtextbegin, curtextbegin, 0, outfp)) > 0)
				totalret ++; /* += ret */
 			else if ((ret < 0) && (errno == AGREP_ERROR)) {
				fclose(infp);
				return -1;
			}
			first_search = 0;
		}
		else {	/* All agrep globals are properly set: has a bug because agrep's globals aren't properly reinitialized without agrep_search :-( */
			agrep_finalfp = (FILE *)outfp;
			agrep_outlen = 0;
			agrep_outbuffer = NULL;
			agrep_outpointer = 0;
			execfd = agrep_initialfd = -1;
			agrep_inbuffer = curtextbegin;
			agrep_inlen = curtextend - curtextbegin;
			agrep_inpointer = 0;
			if ((ret = exec(-1, NULL)) > 0)
				totalret ++; /* += ret; */
 			else if ((ret < 0) && (errno == AGREP_ERROR)) {
				fclose(infp);
				return -1;
			}
		}

		if (((LIMITOUTPUT > 0) && (LIMITOUTPUT <= num_of_matched)) ||
		    ((LIMITPERFILE > 0) && (LIMITPERFILE <= num_of_matched - prev_num_of_matched))) break;	/* done */
		if ((totalret > 0) && FILENAMEONLY) break;
	} /* while *p1 != NULL */

	SetCurrentByteOffset = 0;
	fclose(infp);
	if (totalret > 0) {	/* dirty solution: must handle part of agrep here */
		if (COUNT && !FILEOUT && !SILENT) {
			if(!NOFILENAME) fprintf(outfp, "%s: %d\n", CurrentFileName, totalret);
			else fprintf(outfp, "%d\n", totalret);
		}
		else if (FILEOUT) {
			file_out(realfilename);
		}
	}
	return totalret;
}

/* Sets lastfilenumber that needs to be searched: rest must be discarded */
int
process_Y_option(num_files, num_days, fp)
	int	num_files, num_days;
	FILE	*fp;
{
	CHAR	arrayend[4];

	last_Y_filenumber = 0;
	if ((num_days <= 0) || (fp == NULL) || (timesindexsize <= 0)) return 0;
	last_Y_filenumber = num_files;
	if (num_days * sizeof(int) >= timesindexsize) return 0;	/* everything will be within so many days */
	if (fseek(fp, num_days*sizeof(int), 0) == -1) return -1;
	fread(arrayend, 1, 4, fp);
	if ((last_Y_filenumber = (arrayend[0] << 24) | (arrayend[1] << 16) | (arrayend[2] << 8) | arrayend[3]) > num_files) last_Y_filenumber = num_files;
	if (last_Y_filenumber == 0) {
		last_Y_filenumber = 1;
		printf("Warning: no files modified in the last %d days were found in the index.\nSearching only the most recently modified file...\n", num_days);
	}
	return 0;
}

read_index(indexdir)
char	indexdir[MAXNAME];
{
	char	*home;
	char	s[MAXNAME];
	int	ret;

	if (indexdir[0] == '\0') {
		if ((home = (char *)getenv("HOME")) == NULL) {
			getcwd(indexdir, MAXNAME-1);
			fprintf(stderr, "using working-directory '%s' to locate index\n", indexdir);
		}
		else strncpy(indexdir, home, MAXNAME);
	}
	ret = chdir(indexdir);
	if (getcwd(INDEX_DIR, MAXNAME-1) == NULL) strcpy(INDEX_DIR, indexdir);
	if (ret < 0) {
		fprintf(stderr, "using working-directory '%s' to locate index\n", INDEX_DIR);
	}

	sprintf(s, "%s", INDEX_FILE);
	indexfp = fopen(s, "r");
	if(indexfp == NULL) {
		fprintf(stderr, "can't open glimpse index-file %s/%s\n", INDEX_DIR, INDEX_FILE);
		fprintf(stderr, "(use -H to give an index-directory or run 'glimpseindex' to make an index)\n");
		return -1;
	}
	if (stat(s, &index_stat_buf) == -1) {
		fprintf(stderr, "can't stat %s/%s\n", INDEX_DIR, s);
		fclose(indexfp);
		return -1;
	}

	sprintf(s, "%s", P_TABLE);
	partfp = fopen(s, "r");
	if(partfp == NULL) {
		fprintf(stderr, "can't open glimpse partition-table %s/%s\n", INDEX_DIR, P_TABLE);
		fprintf(stderr, "(use -H to specify an index-directory or run glimpseindex to make an index)\n");
		fclose(indexfp);
		return -1;
	}

	sprintf(s, "%s", DEF_TIME_FILE);
	timesfp = fopen(s, "r");

	sprintf(s, "%s.index", DEF_TIME_FILE);
	timesindexfp = fopen(s, "r");
	if (timesindexfp != NULL) {
		struct stat st;
		fstat(fileno(timesindexfp), &st);
		timesindexsize = st.st_size;
	}

	/* Get options */
#if	BG_DEBUG
	debug = fopen(DEBUG_FILE, "w+");
	if(debug == NULL) {
		fprintf(stderr, "can't open file %s/%s, errno=%d\n", INDEX_DIR, DEBUG_FILE, errno);
		return(-1);
	}
#endif	/*BG_DEBUG*/
	fgets(indexnumberbuf, 256, indexfp);
	if(strstr(indexnumberbuf, "1234567890")) IndexNumber = ON;
	else IndexNumber = OFF;
	fscanf(indexfp, "%%%d\n", &OneFilePerBlock);
	if (OneFilePerBlock < 0) {
		ByteLevelIndex = ON;
		OneFilePerBlock = -OneFilePerBlock;
	}
	else if (OneFilePerBlock == 0) {
		GNumpartitions = get_table(P_TABLE, p_table, MAX_PARTITION, 0);
	}
	fscanf(indexfp, "%%%d%s\n", &StructuredIndex, old_rdelim);
	/* Set WHOLEFILESCOPE for do-it-yourself request processing at client */
	WHOLEFILESCOPE = 1;
	if (StructuredIndex <= 0) {
		if (StructuredIndex == -2) {
			RecordLevelIndex = 1;
			strcpy(rdelim, old_rdelim);
			rdelim_len = strlen(rdelim);
			preprocess_delimiter(rdelim, rdelim_len, rdelim, &rdelim_len);
		}
		WHOLEFILESCOPE = 0;
		StructuredIndex = 0;
		PRINTATTR = 0;	/* doesn't make sense: must not go into filter_output */
	}
	else if (-1 == (StructuredIndex = attr_load_names(ATTRIBUTE_FILE))) {
		fprintf(stderr, "error in reading attribute file %s/%s\n", INDEX_DIR, ATTRIBUTE_FILE);
		return(-1);
	}
#if	BG_DEBUG
	fprintf(debug, "buf = %s OneFilePerBlock=%d StructuredIndex=%d\n", indexnumberbuf, OneFilePerBlock, StructuredIndex);
#endif	/*BG_DEBUG*/
	sprintf(s, "%s", MINI_FILE);
	minifp = fopen(s, "r");
	/* if (minifp==NULL && OneFilePerBlock) fprintf(stderr, "Can't open for reading: %s/%s --- cannot do very fast search\n", INDEX_DIR, MINI_FILE); */
	if (OneFilePerBlock && glimpse_isserver && (minifp != NULL)) read_mini(indexfp, minifp);
	read_filenames();

	/* Once IndexNumber info is available */
	set_indexable_char(indexable_char);
	set_indexable_char(test_indexable_char);
	set_special_char(indexable_char);
	return 0;
}

#define CLEANUP \
{\
	int	q, k;\
	if (timesfp != NULL) fclose(timesfp);\
	if (timesindexfp != NULL) fclose(timesindexfp);\
	if (indexfp != NULL) fclose(indexfp);\
	if (partfp != NULL) fclose(partfp);\
	if (minifp != NULL) fclose(minifp);\
	if (nullfp != NULL) fclose(nullfp);\
	indexfp = partfp = minifp = nullfp = NULL;\
	if (ByteLevelIndex) {\
		if (src_offset_table != NULL) for (k=0; k<OneFilePerBlock; k++) {\
			free_list(&src_offset_table[k]);\
		}\
		for (q=0; q<MAXNUM_PAT; q++) {\
		    if (multi_dest_offset_table[q] != NULL) for (k=0; k<OneFilePerBlock; k++) {\
			free_list(&multi_dest_offset_table[q][k]);\
		    }\
		}\
	}\
	if (StructuredIndex) {\
		attr_free_table();\
	}\
	destroy_filename_hashtable();\
	my_free(SERV_HOST, MAXNAME);\
}

/* Called whenever we get SIGUSR2/SIGHUP (at the end of process_query()) */
reinitialize_server(argc, argv)
	int	argc;
	char	**argv;
{
	int	i, fd;
	CLEANUP;
#if	0
	init_filename_hashtable();
	region_initialize();
	indexfp = partfp = minifp = nullfp = NULL;
	if ((nullfp = fopen("/dev/null", "w")) == NULL) {
		return(-1);
	}
	src_offset_table = NULL;
	for (i=0; i<MAXNUM_PAT; i++) multi_dest_offset_table[i] = NULL;
	if (-1 == read_index(INDEX_DIR)) return(-1);
#if	0
#ifndef	LOCK_UN
#define LOCK_UN	8
#endif
	if ((fd = open(INDEX_DIR, O_RDONLY)) == -1) return -1;
	flock(fd, LOCK_UN);
	close(fd);
#endif
	return 0;
#else
	return execve(argv[0], argv, environ);
#endif
}

/* MUST CARE IF PIPE/SOCKET IS BROKEN! ALSO SIGUSR1 (hardy@cs.colorado.edu) => QUIT CURRENT REQUEST. */
int ignore_signal[32] = {	0,
			0, 0, 1, 1, 1, 1, 1, 1,	/* all the tracing stuff: since default action is to dump core */
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 1, 0, 0 };	/* resource lost: since default action is to dump core */

/* S.t. sockets don't persist: they sometimes have a bad habit of doing so */
void
cleanup()
{
	int	i;

	/* ^C in the middle of a client call */
	if (svstderr != 2) {
		close(2);
		dup(svstderr);
	}
	fprintf(stderr, "server cleaning up...\n");
	CLEANUP;
	for (i=0; i<64; i++) close(i);
	exit(3);
}

void reinitialize(s)
int s;
{
	/* To force main-while loop call reinitialize_server() after do_select() */
	glimpse_reinitialize = 1;
#ifdef __svr4__
	/* Solaris 2.3 insists that you reset the signal handler */
	(void)signal(s, reinitialize);
#endif
}

#define QUITREQUESTMSG "glimpseserver: aborting request...\n"
/* S.t. one request doesn't keep server occupied too long, when client already quits */
void quitrequest(s)
int s;
{
	/*
	 * Don't write onto stderr, since 2 is duped to sockfd => can cause recursive signal!
	 * Also, don't print error message more than once for quitting one request. The
	 * server receives signals for EVERY write it attempts when it finds a match: I could
	 * not find a way to prevent it, but agrep/bitap.c/fill_buf() was fixed to limit it.
	 * -- bg on 16th Feb 1995
	 */
	if (!glimpse_clientdied && (s != SIGUSR1))	/* USR1 is a "friendly" cleanup message */
		write(svstderr, QUITREQUESTMSG, strlen(QUITREQUESTMSG));

	glimpse_clientdied = 1;
#ifdef __svr4__
	/* Solaris 2.3 insists that you reset the signal handler */
	(void)signal(s, quitrequest);
#endif
}

/* The client receives this signal when an output/input pipe is broken, etc. It simply exits from the current request */
void exitrequest()
{
	glimpse_clientdied = 1;
}

main(argc, argv)
int argc;
char *argv[];
{
	int	ret = 0, tried = 0;
	char	indexdir[MAXNAME];
	char	**oldargv = argv;
	int	oldargc = argc;
#if	CLIENTSERVER
	int	sockfd, newsockfd, clilen, len, clpid;
	int	clout;
#if	USE_UNIXDOMAIN
	struct sockaddr_un cli_addr, serv_addr;
#else	/*USE_UNIXDOMAIN*/
	struct sockaddr_in cli_addr, serv_addr;
	struct hostent *hp;
#endif	/*USE_UNIXDOMAIN*/
	int	cli_len;
	int	clargc;
	char	**clargv;
	int	clstdin, clstdout, clstderr;
	int	i;
	char	array[4];
	char	*p, c;
#endif	/*CLIENTSERVER*/
	int	quitwhile;

#if	ISO_CHAR_SET
	setlocale(LC_ALL,"");       /* support for 8bit character set: ew@senate.be, Henrik.Martin@eua.ericsson.se */
#endif
#if	CLIENTSERVER && ISSERVER
	glimpse_isserver = 1;	/* I am the server */
#else	/*CLIENTSERVER && ISSERVER*/
	if (argc <= 1) {
		usage();	/* Client nees at least 1 argument */
		exit(1);
	}
#endif	/*CLIENTSERVER && ISSERVER*/

#define RETURNMAIN(val)\
{\
        CLEANUP;\
        if (val < 0) exit (2);\
	 else if (val == 0) exit (1);\
        else exit (0);\
}

	SERV_HOST = (CHAR *)my_malloc(MAXNAME);
#if	!SYSCALLTESTING
	/* once-only initialization */
	init_filename_hashtable();
	src_offset_table = NULL;
	for (i=0; i<MAXNUM_PAT; i++) multi_dest_offset_table[i] = NULL;
#endif
	gethostname(SERV_HOST, MAXNAME - 2);
	SERV_PORT = DEF_SERV_PORT;
	srand(getpid());
	umask(077);
	strcpy(&GProgname[0], argv[0]);
#if	!SYSCALLTESTING
	region_initialize();
#endif
	indexfp = partfp = minifp = nullfp = NULL;
	if ((nullfp = fopen("/dev/null", "w")) == NULL) {
		fprintf(stderr, "%s: cannot open for writing: /dev/null, errno=%d\n", argv[0], errno);
		RETURNMAIN(-1);
	}
	InterpretSpecial = ON;
	GMAX_WORD_SIZE = MAXPAT;

#if	CLIENTSERVER
#if	!ISSERVER
	/* Install signal handlers so that glimpse doesn't continue to run when pipes get broken, etc. */
	if (((void (*)())-1 == signal(SIGPIPE, exitrequest))
#ifndef	SCO
	    || ((void (*)())-1 == signal(SIGURG, exitrequest))
#endif
	)
	{
		/* Check for return values here since they ensure reliability */
		fprintf(stderr, "glimpse: Unable to install signal-handlers.\n");
		RETURNMAIN(-1);
	}

	/* Check if client has too many arguments: then it is surely running as agrep since I have < half those options! */
	if (argc > MAX_ARGS) goto doityourself;
#endif	/*!ISSERVER*/

#if	!SYSCALLTESTING
	while((--argc > 0) && (*++argv)[0] == '-' ) {
		p = argv[0] + 1;	/* ptr to first character after '-' */
		c = *(argv[0]+1);
		if (*p == '-') {	/* cheesy hack to support --version and --help options */
			if (*(p+1) == 'v') {
				c = 'V';
			} else if (*(p+1) == 'h') {
				c = '?';
			}
		} 
		quitwhile = OFF;
		while (!quitwhile && (*p != '\0')) {
			switch(c) {
			/* Look for -H option at server (only one that makes sense); if client has a -H, then it goes to doityourself */
			case 'H' :
				if (*(p + 1) == '\0') {/* space after - option */
					if (argc <= 1) {
						fprintf(stderr, "%s: a directory name must follow the -H option\n", GProgname);
						RETURNMAIN(usageS());
					}
					argv ++;
					strcpy(indexdir, argv[0]);
					argc --;
				}
				else {
					strcpy(indexdir, p+1);
				}
				quitwhile = ON;
				break;

			/* Recognized by both client and server */
			case 'J' :
				if (*(p + 1) == '\0') {/* space after - option */
					if (argc <= 1) {
						fprintf(stderr, "%s: the server host name must follow the -J option\n", GProgname);
#if	ISSERVER
						RETURNMAIN(usageS());
#else	/*ISSERVER*/
						RETURNMAIN(usage());
#endif	/*ISSERVER*/
					}
					argv ++;
					strcpy(SERV_HOST, argv[0]);
					argc --;
				}
				else {
					strcpy(SERV_HOST, p+1);
				}
				quitwhile = ON;
				break;

			/* Recognized by both client and server */
			case 'K' :
				if (*(p + 1) == '\0') {/* space after - option */
					if (argc <= 1) {
						fprintf(stderr, "%s: the server port must follow the -C option\n", GProgname);
#if	ISSERVER
						RETURNMAIN(usageS());
#else	/*ISSERVER*/
						RETURNMAIN(usage());
#endif	/*ISSERVER*/
					}
					argv ++;
					SERV_PORT = atoi(argv[0]);
					argc --;
				}
				else {
					SERV_PORT = atoi(p+1);
				}
				if ((SERV_PORT < MIN_SERV_PORT) || (SERV_PORT > MAX_SERV_PORT)) {
					fprintf(stderr, "Bad server port %d: must be in [%d, %d]: using default %d\n",
						SERV_PORT, MIN_SERV_PORT, MAX_SERV_PORT, DEF_SERV_PORT);
					SERV_PORT = DEF_SERV_PORT;
				}
				quitwhile = ON;
				break;

#if	ISSERVER
#if	SFS_COMPAT
			case 'R' :
				RemoteFiles = ON;
				break;

			case 'Z' :
				/* No op */
				break;
#endif

			case 'V' :
				printf("\nThis is glimpseindex version %s, %s.\n\n", GLIMPSE_VERSION, GLIMPSE_DATE);
				RETURNMAIN(1);

			case '?' :
				RETURNMAIN(usageS());

			/* server cannot recognize any other option */
			default :
				fprintf(stderr, "%s: server cannot recognize option: '%s'\n", GProgname, p);
				RETURNMAIN(usageS());
#else	/*ISSERVER*/

			/* These have 1 argument each, so must do quitwhile */
			case 'd' :
			case 'e' :
			case 'f' :
			case 'k' :
			case 'D' :
			case 'F' : 
			case 'I' :
			case 'L' :
			case 'R' :
			case 'S' :
			case 'T' :
			case 'Y' :
			case 'p' :
				if (argv[0][2] == '\0') {/* space after - option */
					if(argc <= 1) {
						fprintf(stderr, "%s: the '-%c' option must have an argument\n", GProgname, c);
						RETURNMAIN(usage());
					}
					argv++;
					argc--;
				}
				quitwhile = ON;
				break;

			/* These are illegal */
			case 'm' :
			case 'v' :
				fprintf(stderr, "%s: illegal option: '-%c'\n", GProgname, c);
				RETURNMAIN(usage());

			/* They can't be patterns and filenames since they start with a -, these don't have arguments */
			case '!' :
			case 'a' :
			case 'b' :
			case 'c' :
			case 'h' :
			case 'i' :
			case 'j' :
			case 'l' :
			case 'n' :
			case 'o' :
			case 'q' :
			case 'r' :
			case 's' :
			case 't' :
			case 'u' :
			case 'g' :
			case 'w' :
			case 'x' :
			case 'y' :
			case 'z' :
			case 'A' :
			case 'B' :
			case 'E' :
			case 'G' :
			case 'M' :
			case 'N' :
			case 'O' :
			case 'P' :
			case 'Q' :
			case 'U' :
			case 'W' :
			case 'X' :
			case 'Z' :
				break;

			case 'C':
				CONTACT_SERVER = 1;
				break;
			case 'V' :
				printf("\nThis is glimpse version %s, %s.\n\n", GLIMPSE_VERSION, GLIMPSE_DATE);
				RETURNMAIN(1);

			case '?':
				RETURNMAIN(usage());
			default :
				if (isdigit(c)) quitwhile = ON;
				else {
					fprintf(stderr, "%s: illegal option: '-%c'\n", GProgname, c);
					RETURNMAIN(usage());
				}
				break;
#endif	/*ISSERVER*/
			} /* switch(c) */
			p ++;
			c = *p;
		}
	}
#else
	CONTACT_SERVER = 1;
	argc=0;
#endif

#if	!ISSERVER
	/* Next arg must be the pattern: Check if the user wants to run the client as agrep, or doesn't want to contact the server */
	if ((argc > 1) || (!CONTACT_SERVER)) goto doityourself;
#endif	/*!ISSERVER*/

	argv = oldargv;
	argc = oldargc;
#endif	/*CLIENTSERVER*/

#if	ISSERVER && CLIENTSERVER
	if (-1 == read_index(indexdir)) RETURNMAIN(ret);

	/* Install signal handlers so that glimpseserver doesn't continue to run when sockets get broken, etc. */
	for (i=0; i<32; i++)
		if (ignore_signal[i]) signal(i, SIG_IGN);
	signal(SIGHUP, cleanup);
	signal(SIGINT, cleanup);
	if (((void (*)())-1 == signal(SIGPIPE, quitrequest)) ||
	    ((void (*)())-1 == signal(SIGUSR1, quitrequest)) ||
#ifndef	SCO
	    ((void (*)())-1 == signal(SIGURG, quitrequest)) ||
#endif
	    ((void (*)())-1 == signal(SIGUSR2, reinitialize)) ||
	    ((void (*)())-1 == signal(SIGHUP, reinitialize))) {
		/* Check for return values here since they ensure reliability */
		fprintf(stderr, "glimpseserver: Unable to install signal-handlers.\n");
		RETURNMAIN(-1);
	}

#if	USE_UNIXDOMAIN
	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "server cannot open socket for communication.\n");
		RETURNMAIN(-1);
	}
	char TMP_FILE_NAME[256];
	strcpy(TMP_FILE_NAME,TEMP_DIR) ;
	strcat(TMP_FILE_NAME,"/.glimpse_server");
	unlink(TMP_FILE_NAME);
	memset((char *)&serv_addr, '\0', sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;
	strcpy(serv_addr.sun_path, TMP_FILE_NAME);	/* < 108 ! */
	len = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);
#else	/*USE_UNIXDOMAIN*/
	if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("glimpseserver: Cannot create socket");
		RETURNMAIN(-1);
	}
	memset((char *)&serv_addr, '\0', sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERV_PORT);
#if	0
	/* use host-names not internet style d.d.d.d notation */
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
#else
	/* 
	 * We only want to accept connections from glimpse clients 
	 * on the SERV_HOST, do not use INADDR_ANY!
	 */
	if ((hp = gethostbyname(SERV_HOST)) == NULL) {
		perror("glimpseserver: Cannot resolve host");
		RETURNMAIN(-1);
	}
	memcpy((caddr_t)&serv_addr.sin_addr, hp->h_addr, hp->h_length);
#endif	/*0*/
	len = sizeof(serv_addr);
#endif	/*USE_UNIXDOMAIN*/
	/* test code for glimpse server, get it to realse socket when it dies: contribution by Sheldon Smoker <sheldon@thunder.tig.com> */
	if((setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(char *)&one,sizeof(one))) == -1) {
		fprintf(stderr,"glimpseserver: could not set socket option\n");
		perror("setsockopt");
		exit(1);
	}
	/* end test code */

	if (bind(sockfd, (struct sockaddr *)&serv_addr, len) < 0) {
		perror("glimpseserver: Cannot bind to socket");
		RETURNMAIN(-1);
	}
	listen(sockfd, SERVER_QUEUE_SIZE);

	printf("glimpseserver: On-line (pid = %d, port = %d) waiting for request...\n", getpid(), SERV_PORT);
	fflush(stdout);	/* must fflush to print on server stdout */
	while (1) {
		/*
		 *  Spin until sockfd is ready to do a non-blocking accept(2).
		 *  We only wait for 15 seconds, because SunOS may
		 *  swap us out if we block for 20 seconds or more.
		 *  -- Courtesy: Darren Hardy, hardy@cs.colorado.edu
		 */
		if ((ret = do_select(sockfd, 15)) == 0) {
			if ((errno == EINTR) && glimpse_reinitialize) {
				glimpse_reinitialize = 0;
				CLEANUP;
				close(sockfd);
				sleep(IC_PORTRELEASE);
				reinitialize_server(oldargc, oldargv);
			}
			continue;
		}
		else if (ret != 1) continue;

		/* get parameters */
		ret = 0;
		clargc = 0;
		clargv = NULL;
		cli_len = sizeof(cli_addr);
		if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len)) < 0) continue;
		if (getreq(newsockfd, glimpse_reqbuf, &clstdin, &clstdout, &clstderr, &clargc, &clargv, &clpid) < 0) {
			ret = -1;
#if	DEBUG
			printf("getreq errno: %d\n", errno);
#endif	/*DEBUG*/
			goto end_process;
		}

#if	DEBUG
		printf("server processing request on %x\n", newsockfd);
#endif	/*DEBUG*/
		/*
		 * Server doesn't wait for response, no point using
		svstdin = dup(0);
		close(0);
		dup(clstdin);
		close(clstdin);
		 */
		/*
		 * This is wrong since clstderr == clstdout!
		svstdout = dup(1);
		close(1);
		dup(clstdout);
		close(clstdout);
		svstderr = dup(2);
		close(2);
		dup(clstderr);
		close(clstderr);
		*/
		svstdout = dup(1);
		svstderr = dup(2);
		close(1);
		close(2);
		dup(clstdout);
		dup(clstderr);
		close(clstdout);
		close(clstderr);

                        /*
			 * IMPORTANT: Unbuffered I/O to the client!
                         * Done for Harvest since partial results might be
                         * needed and fflush will not flush partial results
                         * to the client if we type ^C and kill it: it puts
                         * them into /dev/null. This way, output is unbuffered
                         * and the client sees at least some results if killed.
                         */
                        setbuf(stdout, NULL);
                        setbuf(stderr, NULL);

			glimpse_call = 0;
			glimpse_clientdied = 0;
			ret = process_query(clargc, clargv, newsockfd);
		/*
		 * Server doesn't wait for response, no point using
		close(0);
		dup(svstdin);
		close(svstdin);
		svstdin = 0;
		 */
		if (glimpse_clientdied) {
			/*
			 * This code is *ONLY* used as a safety net now.  
			 * The old problem was that users would see portions 
			 * of previous (and usually) unrelated queries!
			 * glimpseserver now uses unbuffered I/O to the
			 * client so all previous fwrite's to now are
			 * gone.  But since this is such a nasty problem
			 * we flush stdout to /dev/null just in case.
			 */
			clout = open("/dev/null", O_WRONLY);
			close(1);
			dup(clout);
			close(clout);
			fflush(stdout);
		} 

		/* Restore svstdout and svstdout to stdout/stderr */
		close(1);
		dup(svstdout);
		close(svstdout);
		svstdout = 1;
		close(2);
		dup(svstderr);
		close(svstderr);
		svstderr = 2;

	end_process:
#if	USE_MSGHDR
		/* send reply and cleanup */
		array[0] = (ret & 0xff000000) >> 24;
		array[1] = (ret & 0xff0000) >> 16;
		array[2] = (ret & 0xff00) >> 8;
		array[3] = (ret & 0xff);
		writen(newsockfd, array, 4);
#endif	/*USE_MSGHDR*/
#if	DEBUG
		write(1, "done\n", 5);
#endif	/*DEBUG*/
		for (i=0; i<clargc; i++)
			if (clargv[i] != NULL) my_free(clargv[i], 0);
		if (clargv != NULL) my_free(clargv, 0);
		close(newsockfd);	/* if !USE_MSGHDR, client directly reads from socket and writes onto stdout until EOF */
	}
#else	/*ISSERVER && CLIENTSERVER*/

#if	CLIENTSERVER
trynewsocket:
#if	USE_UNIXDOMAIN
	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		goto doityourself;
	}
	memset((char *)&serv_addr, '\0', sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;
	char TMP_FILE_NAME[256];
	strcpy(TMP_FILE_NAME,TEMP_DIR) ;
	strcat(TMP_FILE_NAME,"/.glimpse_server");
	strcpy(serv_addr.sun_path, TMP_FILE_NAME);	/* < 108 ! */
	len = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);
#else	/*USE_UNIXDOMAIN*/
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		goto doityourself;
	}
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERV_PORT);
#if	0
	/* use host-names not internet style d.d.d.d notation */
	serv_addr.sin__addr.s_addr = inet_addr(SERV_HOST);
#else	/*0*/
	if ((hp = gethostbyname(SERV_HOST)) == NULL) {
		fprintf(stderr, "gethostbyname (%s) failed\n", SERV_HOST);
		goto doityourself;
	}
	memcpy((caddr_t)&serv_addr.sin_addr, hp->h_addr, hp->h_length);
#endif	/*0*/
	len = sizeof(serv_addr);
#endif	/*USE_UNIXDOMAIN*/

	if (connect(sockfd, (struct sockaddr *)&serv_addr, len) < 0) {
                char errbuf[4096];
                sprintf(errbuf, "glimpse: Cannot contact glimpseserver: %s, port %d:", SERV_HOST, SERV_PORT);
                perror(errbuf);
		/* perror(SERV_HOST); */
#if	DEBUG
		printf("connect errno: %d\n", errno);
#endif	/*DEBUG*/
		close(sockfd);
		if ((errno == ECONNREFUSED) && (tried < 4)) {
			tried ++;
			goto trynewsocket;
		}
		goto doityourself;
	}

	if (sendreq(sockfd, glimpse_reqbuf, fileno(stdin), fileno(stdout), fileno(stderr), argc, argv, getpid()) < 0) {
		perror("sendreq");
#if	DEBUG
		printf("sendreq errno: %d\n", errno);
#endif	/*DEBUG*/
		close(sockfd);
		goto doityourself;
	}

#if	USE_MSGHDR
	if (readn(sockfd, array, 4) != 4) {
		close(sockfd);
		goto doityourself;
	}
	ret = (array[0] << 24) + (array[1] << 16) + (array[2] << 8) + array[3];
#else	/*USE_MSGHDR*/
{
	/* 
	 *  Dump everything the server writes into the socket onto 
	 *  stdout until EOF/error.  Do this in a way so that *everything*
	 *  the server sends is dumped to stdout by the client.  The
	 *  client might die suddenly via ^C or SIGTERM, but we still
	 *  want the results.
	 */
	char tmpbuf[1024];
	int n;

	while ((n = read(sockfd, tmpbuf, 1024)) > 0) {
		write(fileno(stdout), tmpbuf, n);
	}
}
#endif	/*USE_MSGHDR*/

	close(sockfd);
	RETURNMAIN(ret);

doityourself:
#if	DEBUG
	printf("doing it myself :-(\n");
#endif	/*DEBUG*/
#endif	/*CLIENTSERVER*/
	setbuf(stdout, NULL);	/* Unbuffered I/O to always get every result */
	setbuf(stderr, NULL);
	glimpse_call = 0;
	glimpse_clientdied = 0;
	ret = process_query(oldargc, oldargv, fileno(stdin));
	RETURNMAIN(ret);
#endif	/*ISSERVER && CLIENTSERVER*/
}

process_query(argc, argv, newsockfd)
int argc; 
char *argv[];
int newsockfd;
{
	int	searchpercent;
	int	num_blocks;
	int	num_read;
	int	i, j;
	int	iii; /* Udi */
	int	jjj;
	char	c;
	char	*p;
	int	ret;
	int	jj;
	int	quitwhile;
	char	indexdir[MAX_LINE_LEN];
	char	temp_filenames_file[MAX_LINE_LEN];
	char	temp_bitfield_file[MAX_LINE_LEN];
	char	TEMP_FILE[MAX_LINE_LEN];
	char	temp_file[MAX_LINE_LEN];
	int	oldargc = argc;
	char	**oldargv = argv;
	CHAR	dummypat[MAX_PAT];
	int	dummylen=0;
	int	my_M_index, my_P_index, my_b_index, my_A_index, my_l_index = -1, my_B_index = -1;
	char	**outname;
	int	gnum_of_matched = 0;
	int	gprev_num_of_matched = 0;
	int	gfiles_matched = 0;
	int	foundpat = 0;
	int	wholefilescope=0;
	int	nobytelevelmustbeon=0;
	long	get_file_time();

	if ((argc <= 0) || (argv == NULL)) {
		errno = EINVAL;
		return -1;
	}
/*
 * Macro to destroy EVERYTHING before return since we might want to make this a
 * library function later on: convention is that after destroy, objects are made
 * NULL throughout the source code, and are all set to NULL at initialization time.
 * DO agrep_argv, index_argv and FileOpt my_malloc/my_free optimizations later.
 * my_free calls have 2nd parameter = 0 if the size is not easily determinable.
 */
#define RETURN(val) \
{\
	int	q,k;\
\
	first_search = 0;\
	for (k=0; k<MAX_ARGS; k++) {\
		if (agrep_argv[k] != NULL) my_free(agrep_argv[k], 0);\
		if (index_argv[k] != NULL) my_free(index_argv[k], 0);\
		agrep_argv[k] = index_argv[k] = NULL;\
	}\
	if (FileOpt != NULL) my_free(FileOpt, MAXFILEOPT);\
	FileOpt = NULL;\
	for (k=0; k<MAXNUM_PAT; k++) {\
		if (pat_list[k] != NULL) my_free(pat_list[k], 0);\
		pat_list[k] = NULL;\
	}\
	sprintf(tempfile, "%s/.glimpse_tmp.%d", TEMP_DIR, getpid());\
	unlink(tempfile);\
	sprintf(outname[0], "%s/.glimpse_apply.%d", TEMP_DIR, getpid());\
	unlink(outname[0]);\
	my_free(outname[0], 0);\
	my_free(outname, 0);\
	my_free(TEMP_DIR, MAX_LINE_LEN);\
	my_free(filenames_file, MAX_LINE_LEN);\
	my_free(bitfield_file, MAX_LINE_LEN);\
\
	if (ByteLevelIndex) {\
		if (src_offset_table != NULL) for (k=0; k<OneFilePerBlock; k++) {\
			free_list(&src_offset_table[k]);\
		}\
		/* Don't make src_offset_table itself NULL: it will be bzero-d below if !NULL */\
		for (q=0; q<MAXNUM_PAT; q++) {\
		    if (multi_dest_offset_table[q] != NULL) for (k=0; k<OneFilePerBlock; k++) {\
			free_list(&multi_dest_offset_table[q][k]);\
		    }\
		    /* Don't make multi_dest_offset_table[q] itself NULL: it will be bzero-d below if !NULL */\
		}\
	}\
	for (k=0; k<num_terminals;k++)\
		free(terminals[k].data.leaf.value);\
	if (ComplexBoolean) destroy_tree(&GParse);\
	for (k=0; k<GNumfiles; k++) {\
		my_free(GTextfiles[k], 0);\
		GTextfiles[k] = NULL;\
	}\
	/* Don't free the GTextfiles buffer itself since it is allocated once in get_filename.c */\
	return (val);\
}

	/*
	 * Initialize
	 */
	strcpy(&GProgname[0], argv[0]);
	if (argc <= 1) return(usage());
	filenames_file = (char *)my_malloc(MAX_LINE_LEN);
	bitfield_file = (char *)my_malloc(MAX_LINE_LEN);
	TEMP_DIR = (char *)my_malloc(MAX_LINE_LEN);
	strcpy(TEMP_DIR, "/tmp");
	D_length = 0;
	D = 0;
	pattern_index = 0;
	first_search = 1;
	outname  = (char **)my_malloc(sizeof(char *));
	outname[0] = (char *)my_malloc(MAX_LINE_LEN);
	NOBYTELEVEL = 0;
	OPTIMIZEBYTELEVEL = 0;
	GCONSTANT = 0;
	GLIMITOUTPUT = 0;
	GLIMITTOTALFILE = 0;
	GBESTMATCH = 0;
	GRECURSIVE = 0;
	GNOPROMPT = 0;
	GBYTECOUNT = 0;
	GPRINTFILENUMBER = 0;
	GPRINTFILETIME = 0;
	GOUTTAIL = 2;	/* stupid fix, but works */
	GFILENAMEONLY = 0;
	GNOFILENAME = 0;
	GPRINTNONEXISTENTFILE = 0;
	MATCHFILE = 0;
	PRINTATTR = 0;
	PRINTINDEXLINE = 0;
	Pat_as_is=0;
	Only_first = 0;
	PRINTAPPXFILEMATCH = 0;
	GCOUNT = 0;
	HINTSFROMUSER = 0;
	FILENAMESINFILE=0;
	BITFIELDFILE=0;
	BITFIELDOFFSET=0;
	BITFIELDLENGTH=0;
	BITFIELDENDIAN=0;
	GNumDays = 0;
	foundattr = 0;
	foundnot = 0;
	ComplexBoolean = 0;
	bestmatcherrors = 0;
	patbufpos = -1;
	RegionLimit=DEFAULT_REGION_LIMIT;
	strcpy(GD_pattern, "\n");
	GD_length = strlen(GD_pattern);
	indexdir[0] = '\0';
	memset(index_argv, '\0', sizeof(char *) * MAX_ARGS);
	index_argc = 0;
	memset(agrep_argv, '\0', sizeof(char *) * MAX_ARGS);
	agrep_argc = 0;
	FileOpt = NULL;
	fileopt_length = 0;
	memset(pat_list, '\0', sizeof(char *) * MAXNUM_PAT);
	memset(pat_attr, '\0', sizeof(int) * MAXNUM_PAT);
	for (i=0; i<MAX_ARGS; i++)
		index_argv[i] = (char *)my_malloc(MaxNameLength + 2);
	memset(is_mgrep_pat, '\0', sizeof(int) * MAXNUM_PAT);
	memset(mgrep_pat_index, '\0', sizeof(int) *MAXNUM_PAT);
	num_mgrep_pat = 0;
	memset(pat_buf, '\0', (MAXNUM_PAT + 2)*MAXPAT);
	pat_ptr = 0;
	sprintf(tempfile, "%s/.glimpse_tmp.%d", TEMP_DIR, getpid());
	/* Set WHOLEFILESCOPE for per-request processing at server */
	if (StructuredIndex) WHOLEFILESCOPE = 1;
	else WHOLEFILESCOPE = 0;
	last_Y_filenumber = 0;

	if (argc > MAX_ARGS) {
#if	ISSERVER
	fprintf(stderr, "too many arguments %d obtained on server!\n", argc);
#endif	/*ISSERVER*/
		i = fileagrep(oldargc, oldargv, 0, stdout);
		RETURN(i);
	}

	/*
	 * Process what options you can, then call fileagrep_init() to set
	 * options in agrep and get the pattern. Then, call fileagrep_search().
	 * Begin by copying options into agrep_argv assuming glimpse was not
	 * called as agrep (optimistic :-).
	 */

	agrep_argc = 0;
	for (i=0; i<MAX_ARGS; i++) agrep_argv[i] = NULL;
	agrep_argv[agrep_argc] = (char *)my_malloc(strlen(argv[0]) + 2);
	strcpy(agrep_argv[agrep_argc], argv[0]);	/* copy the name of the program anyway */
	agrep_argc ++;

	/* In glimpse, you should never output filenames with zero matches */
	if (agrep_argc + 1 >= MAX_ARGS) {
		fprintf(stderr, "%s: too many options!\n", GProgname);
		RETURN(usage());
	}
	agrep_argv[agrep_argc] = (char *)my_malloc(sizeof(char *));
	agrep_argv[agrep_argc][0] = '-';
	agrep_argv[agrep_argc][1] = 'z';
	agrep_argv[agrep_argc][2] = '\0';
	agrep_argc ++;

	/* In glimpse, you should always print pattern when using mgrep (user can't do -f or -m)! */
	if (agrep_argc + 1 >= MAX_ARGS) {
		fprintf(stderr, "%s: too many options!\n", GProgname);
		RETURN(usage());
	}
	agrep_argv[agrep_argc] = (char *)my_malloc(sizeof(char *));
	agrep_argv[agrep_argc][0] = '-';
	agrep_argv[agrep_argc][1] = 'P';
	agrep_argv[agrep_argc][2] = '\0';
	my_P_index = agrep_argc;
	agrep_argc ++;

	/* In glimpse, you should always output multiple when doing mgrep */
	if (agrep_argc + 1 >= MAX_ARGS) {
		fprintf(stderr, "%s: too many options!\n", GProgname);
		RETURN(usage());
	}
	agrep_argv[agrep_argc] = (char *)my_malloc(sizeof(char *));
	agrep_argv[agrep_argc][0] = '-';
	agrep_argv[agrep_argc][1] = 'M';
	agrep_argv[agrep_argc][2] = '\0';
	my_M_index = agrep_argc;
	agrep_argc ++;

	/* In glimpse, you should print the byte offset if there is a structured query */
	if (agrep_argc + 1 >= MAX_ARGS) {
		fprintf(stderr, "%s: too many options!\n", GProgname);
		RETURN(usage());
	}
	agrep_argv[agrep_argc] = (char *)my_malloc(sizeof(char *));
	agrep_argv[agrep_argc][0] = '-';
	agrep_argv[agrep_argc][1] = 'b';
	agrep_argv[agrep_argc][2] = '\0';
	my_b_index = agrep_argc;
	agrep_argc ++;

	/* In glimpse, you should always have space for doing -m if required */
	if (agrep_argc + 2 >= MAX_ARGS) {
		fprintf(stderr, "%s: too many options!\n", GProgname);
		RETURN(usage());
	}
	agrep_argv[agrep_argc] = (char *)my_malloc(sizeof(char *));
	agrep_argv[agrep_argc][0] = '-';
	agrep_argv[agrep_argc][1] = 'm';
	agrep_argv[agrep_argc][2] = '\0';
	agrep_argc ++;
	agrep_argv[agrep_argc] = (char *)my_malloc(2);	/* no op */
	agrep_argv[agrep_argc][0] = '\0';
	agrep_argc ++;

	/* Add -A option to print filenames as default */
	if (agrep_argc + 1 >= MAX_ARGS) {
		fprintf(stderr, "%s: too many options!\n", GProgname);
		RETURN(usage());
	}
	agrep_argv[agrep_argc] = (char *)my_malloc(sizeof(char *));
	agrep_argv[agrep_argc][0] = '-';
	agrep_argv[agrep_argc][1] = 'A';
	agrep_argv[agrep_argc][2] = '\0';
	my_A_index = agrep_argc;
	agrep_argc ++;

	while((agrep_argc < MAX_ARGS) && (--argc > 0) && (*++argv)[0] == '-' ) {
		p = argv[0] + 1;	/* ptr to first character after '-' */
		c = *(argv[0]+1);
		quitwhile = OFF;
		while (!quitwhile && (*p != '\0')) {
			c = *p;
			switch(c) {
			case 'F' : 
				MATCHFILE = ON;
				FileOpt = (CHAR *)my_malloc(MAXFILEOPT);
				if (*(p + 1) == '\0') {/* space after - option */
					if(argc <= 1) {
						fprintf(stderr, "%s: a file pattern must follow the -F option\n", GProgname);
						RETURN(usage());
					}
					argv++;
					if ((dummylen = strlen(argv[0])) > MAXFILEOPT) {
						fprintf(stderr, "%s: -F option list too long\n", GProgname);
						RETURN(usage());
					}
					strcpy(FileOpt, argv[0]);
					argc--;
				} else {
					if ((dummylen = strlen(p+1)) > MAXFILEOPT) {
						fprintf(stderr, "%s: -F option list too long\n", GProgname);
						RETURN(usage());
					}
					strcpy(FileOpt, p+1);
				} /* else */
				quitwhile = ON;
				break;

			/* search the index only and output the number of blocks */
			case 'N' :
				Only_first = ON;
				break ;

			/* also keep track of the matches in each file */
			case 'Q' :
				PRINTAPPXFILEMATCH = ON;
				break ;

			case 'U' :
				InfoAfterFilename = ON;
				break;
			
			case '!' :
				HINTSFROMUSER = ON;
				break;

			/* go to home directory to find the index: even if server overwrites indexdir here, it won't overwrite INDEX_DIR until read_index() */
			case 'H' :
				if (*(p + 1) == '\0') {/* space after - option */
					if (argc <= 1) {
						fprintf(stderr, "%s: a directory name must follow the -H option\n", GProgname);
						RETURN(usage());
					}
					argv ++;
#if	!ISSERVER
					strcpy(indexdir, argv[0]);
#endif	/*!ISSERVER*/
					argc --;
				}
#if	!ISSERVER
				else {
					strcpy(indexdir, p+1);
				}
				agrep_argv[agrep_argc] = (char *)my_malloc(4);
				strcpy(agrep_argv[agrep_argc], "-H");
				agrep_argc ++;
				agrep_argv[agrep_argc] = (char *)my_malloc(strlen(indexdir) + 2);
				strcpy(agrep_argv[agrep_argc], indexdir);
				agrep_argc ++;
#endif	/*!ISSERVER*/
				quitwhile = ON;
				break;

#if	ISSERVER && SFS_COMPAT
			/* INDEX_DIR will be already set since this is the server, so we can direclty xfer the .glimpse_* files */
			case '.' :
				strcpy(TEMP_FILE, INDEX_DIR);
				strcpy(temp_file, ".");
				strcat(TEMP_FILE, "/.");
				if (*(p + 1) == '\0') {/* space after - option */
					if (argc <= 1) {
						fprintf(stderr, "%s: a file name must follow the -. option\n", GProgname);
						RETURN(usage());
					}
					argv ++;
					strcat(TEMP_FILE, argv[0]);
					strcat(temp_file, argv[0]);
					argc --;
				}
				else {
					strcat(TEMP_FILE, p+1);
					strcat(temp_file, p+1);
				}
				if (!strcmp(temp_file, INDEX_FILE) || !strcmp(temp_file, FILTER_FILE) ||
				    !strcmp(temp_file, ATTRIBUTE_FILE) || !strcmp(temp_file, MINI_FILE) ||
				    !strcmp(temp_file, P_TABLE) || !strcmp(temp_file, PROHIBIT_LIST) ||
				    !strcmp(temp_file, INCLUDE_LIST) || !strcmp(temp_file, NAME_LIST) ||
				    !strcmp(temp_file, NAME_LIST_INDEX) || !strcmp(temp_file, NAME_HASH) ||
				    !strcmp(temp_file, NAME_HASH_INDEX) || !strcmp(temp_file, DEF_STAT_FILE) ||
				    !strcmp(temp_file, DEF_MESSAGE_FILE) || !strcmp(temp_file, DEF_TIME_FILE)) {
					if ((ret = open(TEMP_FILE, O_RDONLY, 0)) <= 0) RETURN(ret);
					while ((num_read = read(ret, matched_region, MAX_REGION_LIMIT*2)) > 0) {
						write(1 /* NOT TO newsockfd since that was got by a syscall!!! */, matched_region, num_read);
					}
					close(ret);
				}
				quitwhile = ON;
				RETURN(0);
#endif	/* ISSERVER */

			/* go to temp directory to create temp files */
			case 'T' :
				if (*(p + 1) == '\0') {/* space after - option */
					if (argc <= 1) {
						fprintf(stderr, "%s: a directory name must follow the -T option\n", GProgname);
						RETURN(usage());
					}
					argv ++;
					strcpy(TEMP_DIR, argv[0]);
					argc --;
				}
				else {
					strcpy(TEMP_DIR, p+1);
				}
				sprintf(tempfile, "%s/.glimpse_tmp.%d", TEMP_DIR, getpid());
				quitwhile = ON;
				break;

			/* To get files within some number of days before indexing was done */
			case 'Y':
				if (*(p + 1) == '\0') {/* space after - option */
					if (argc <= 1) {
						fprintf(stderr, "%s: the number of days must follow the -Y option\n", GProgname);
						RETURN(usage());
					}
					argv ++;
					GNumDays = atoi(argv[0]);
					argc --;
				}
				else {
					GNumDays = atoi(p+1);
				}
				if (GNumDays <= 0) {
					fprintf(stderr, "%s: the number of days %d must be > 0\n", GProgname, GNumDays);
					RETURN(usage());
				}
				quitwhile = ON;
				break;

			case 'R' :
				if (*(p + 1) == '\0') {/* space after - option */
					if (argc <= 1) {
						fprintf(stderr, "%s: the record size must follow the -R option\n", GProgname);
						RETURN(usage());
					}
					argv ++;
					RegionLimit = atoi(argv[0]);
					argc --;
				}
				else {
					RegionLimit = atoi(p+1);
				}
				if ((RegionLimit <= 0) || (RegionLimit > MAX_REGION_LIMIT)) {
					fprintf(stderr, "Bad record size %d: must be in [%d, %d]: using default %d\n",
						RegionLimit, 1, MAX_REGION_LIMIT, DEFAULT_REGION_LIMIT);
					RegionLimit = DEFAULT_REGION_LIMIT;
				}
				quitwhile = ON;
				break;

			/* doesn't matter if we overwrite the value in the client since the same value would have been picked up in main() anyway */
			case 'J' :
				if (*(p + 1) == '\0') {/* space after - option */
					if (argc <= 1) {
						fprintf(stderr, "%s: the server host name must follow the -J option\n", GProgname);
						RETURN(usageS());
					}
					argv ++;
#if	!ISSERVER
					strcpy(SERV_HOST, argv[0]);
#endif	/*!ISSERVER*/
					argc --;
				}
#if	!ISSERVER
				else {
					strcpy(SERV_HOST, p+1);
				}
#endif	/*!ISSERVER*/
				quitwhile = ON;
				break;

			/* doesn't matter if we overwrite the value in the client since the same value would have been picked up in main() anyway */
			case 'K' :
				if (*(p + 1) == '\0') {/* space after - option */
					if (argc <= 1) {
						fprintf(stderr, "%s: the server port must follow the -C option\n", GProgname);
						RETURN(usage());
					}
					argv ++;
#if	!ISSERVER
					SERV_PORT = atoi(argv[0]);
#endif	/*!ISSERVER*/
					argc --;
				}
#if	!ISSERVER
				else {
					SERV_PORT = atoi(p+1);
				}
				if ((SERV_PORT < MIN_SERV_PORT) || (SERV_PORT > MAX_SERV_PORT)) {
					fprintf(stderr, "Bad server port %d: must be in [%d, %d]: using default %d\n",
						SERV_PORT, MIN_SERV_PORT, MAX_SERV_PORT, DEF_SERV_PORT);
					SERV_PORT = DEF_SERV_PORT;
				}
#endif	/*!ISSERVER*/
				quitwhile = ON;
				break;

			/* Based on contribution From ada@mail2.umu.se Fri Jul 12 01:56 MST 1996; Christer Holgersson, Sen. SysNet Mgr, Umea University/SUNET, Sweden */
			/* the bit-mask corresponding to the set of filenames within which the pattern should be searched is explicitly provided in a filename (absolute path name) */
			case 'p' :
				if (*(p + 1) == '\0') {/* space after - option */
					if (argc <= 1) {
						fprintf(stderr, "%s: the bitfield file [and an offset/length/endian separated by :] must follow the -p option\n", GProgname);
						RETURN(usage());
					}
					argv ++;
					strcpy(bitfield_file, argv[0]);
					argc --;
				}
				else {
					strcpy(bitfield_file, p+1);
				}
				/* Find offset and length into bitfield file */
				{
					int iiii = 0;

					BITFIELDOFFSET=0;
					BITFIELDLENGTH=0;
					BITFIELDENDIAN=0;
					iiii = 0;
					while (bitfield_file[iiii] != '\0') {
						if (bitfield_file[iiii] == '\\') {
							iiii ++;
							if (bitfield_file[iiii] == '\0') break;
							if (bitfield_file[iiii] == ':') {
								strcpy(&bitfield_file[iiii-1], &bitfield_file[iiii]);
							}
							else iiii ++;
							continue;
						}
						if (bitfield_file[iiii] == ':') {
							bitfield_file[iiii] = '\0';
							sscanf(&bitfield_file[iiii+1], "%d:%d:%d", &BITFIELDOFFSET, &BITFIELDLENGTH, &BITFIELDENDIAN);
							if ((BITFIELDOFFSET < 0) || (BITFIELDLENGTH < 0) || (BITFIELDENDIAN < 0)) {
								fprintf(stderr, "Wrong offset %d or length %d or endian %d of bitfield file\n", BITFIELDOFFSET, BITFIELDLENGTH, BITFIELDENDIAN);
								RETURN(usage());
							}
							break;
						}
						iiii++;
					}
#if	BG_DEBUG
					fprintf(debug, "BITFIELD %s : %d : %d : %d\n", BITFIELDFILE, BITFIELDOFFSET, BITFIELDLENGTH, BITFIELDENDIAN);
#endif
				}
				if (bitfield_file[0] != '/') {
					getcwd(temp_bitfield_file, MAX_LINE_LEN-1);
					strcat(temp_bitfield_file, "/");
					strcat(temp_bitfield_file, bitfield_file);
					strcpy(bitfield_file, temp_bitfield_file);
				}
				BITFIELDFILE = 1;
				quitwhile = ON;
				break;

			/* the set of filenames within which the pattern should be searched is explicitly provided in a filename (absolute path name) */
			case 'f' :
				if (*(p + 1) == '\0') {/* space after - option */
					if (argc <= 1) {
						fprintf(stderr, "%s: the filenames file must follow the -f option\n", GProgname);
						RETURN(usage());
					}
					argv ++;
					strcpy(filenames_file, argv[0]);
					argc --;
				}
				else {
					strcpy(filenames_file, p+1);
				}
				if (filenames_file[0] != '/') {
					getcwd(temp_filenames_file, MAX_LINE_LEN-1);
					strcat(temp_filenames_file, "/");
					strcat(temp_filenames_file, filenames_file);
					strcpy(filenames_file, temp_filenames_file);
				}
				FILENAMESINFILE = 1;
				quitwhile = ON;
				break;

			case 'C' :
				CONTACT_SERVER = 1;
				break;

			case 'a' :
				PRINTATTR = 1;
				break;

			case 'E':
				PRINTINDEXLINE = 1;
				break;

			case 'W':
				wholefilescope = 1;
				break;

			case 'z' :
				UseFilters = 1;
				break;

			case 'r' :
				GRECURSIVE = 1;
				break;

			case 'V' :
				printf("\nThis is glimpse version %s, %s.\n\n", GLIMPSE_VERSION, GLIMPSE_DATE);
				RETURN(0);

			/* Must let 'm' fall thru to default once multipatterns are done in agrep */
			case 'm' :
			case 'v' :
				fprintf(stderr, "%s: illegal option: '-%c'\n", GProgname, c);
				RETURN(usage());

			case 'I' :
			case 'D' :
			case 'S' :
				/* There is no space after these options */
				agrep_argv[agrep_argc] = (char *)my_malloc(strlen(argv[0]) + 2);
				agrep_argv[agrep_argc][0] = '-';
				strcpy(agrep_argv[agrep_argc] + 1, p);
				agrep_argc ++;
				quitwhile = ON;
				break;

			case 'l':
				GFILENAMEONLY = 1;
				my_l_index = agrep_argc;
				agrep_argv[agrep_argc] = (char *)my_malloc(4);
				agrep_argv[agrep_argc][0] =  '-';
				agrep_argv[agrep_argc][1] = c;
				agrep_argv[agrep_argc][2] = '\0';
				agrep_argc ++;
				break;

			/*
			 * Copy the set of options for agrep: put them in separate argvs
			 * even if they are together after one '-' (easier to process).
			 * These are agrep options which glimpse has to peek into.
			 */
			default:
				agrep_argv[agrep_argc] = (char *)my_malloc(16);
				agrep_argv[agrep_argc][0] =  '-';
				agrep_argv[agrep_argc][1] = c;
				agrep_argv[agrep_argc][2] = '\0';
				agrep_argc ++;

				if (c == 'n') {
					nobytelevelmustbeon=1;
				}
				else if (c == 'X') GPRINTNONEXISTENTFILE = 1;
				else if (c == 'j') GPRINTFILETIME = 1;
				else if (c == 'b') GBYTECOUNT = 1;
				else if (c == 'g') GPRINTFILENUMBER = 1;
				else if (c == 't') GOUTTAIL = 1;
				else if (c == 'y') GNOPROMPT = 1;
				else if (c == 'h') GNOFILENAME = 1;
				else if (c == 'c') GCOUNT = 1;
				else if (c == 'B') {
					GBESTMATCH = 1;
					my_B_index = agrep_argc - 1;
				}
				/* the following options are followed by a parameter */
				else if ((c == 'e') || (c == 'd') || (c == 'L') || (c == 'k')) {
					if (*(p + 1) == '\0') {/* space after - option */
						if(argc <= 1) {
							fprintf(stderr, "%s: the '-%c' option must have an argument\n", GProgname, c);
							RETURN(usage());
						}
						argv++;
						if ( (c == 'd') && ((D_length = strlen(argv[0])) > MAX_NAME_SIZE) ) {
							fprintf(stderr, "%s: delimiter pattern too long (has > %d chars)\n", GProgname, MAX_NAME_SIZE);
							RETURN(usage());
							/* Should this be RegionLimit if ByteLevelIndex? */
						}
						else if (c == 'L') {
							GLIMITOUTPUT = GLIMITTOTALFILE = GLIMITPERFILE = 0;
							sscanf(argv[0], "%d:%d:%d", &GLIMITOUTPUT, &GLIMITTOTALFILE, &GLIMITPERFILE);
							if ((GLIMITOUTPUT < 0) || (GLIMITTOTALFILE < 0) || (GLIMITPERFILE < 0)) {
								fprintf(stderr, "%s: invalid output limit %s\n", GProgname, argv[0]);
								RETURN(usage());
							}
						}
						agrep_argv[agrep_argc] = (char *)my_malloc(strlen(argv[0]) + 2);
						strcpy(agrep_argv[agrep_argc], argv[0]);
						if (c == 'd') {
							preprocess_delimiter(argv[0], D_length, GD_pattern, &GD_length);
							if (GOUTTAIL == 2) GOUTTAIL = 0;
							/* Should this be RegionLimit if ByteLevelIndex? */
						}
						if (c == 'k') GCONSTANT = 1;
						argc--;
					} else {
						if ( (c == 'd') && ((D_length = strlen(p+1)) > MAX_NAME_SIZE) ) {
							fprintf(stderr, "%s: delimiter pattern too long (has > %d chars)\n", GProgname, MAX_NAME_SIZE);
							RETURN(usage());
							/* Should this be RegionLimit if ByteLevelIndex? */
						}
						else if (c == 'L') {
							GLIMITOUTPUT = GLIMITTOTALFILE = GLIMITPERFILE = 0;
							sscanf(p+1, "%d:%d:%d", &GLIMITOUTPUT, &GLIMITTOTALFILE, &GLIMITPERFILE);
							if ((GLIMITOUTPUT < 0) || (GLIMITTOTALFILE < 0) || (GLIMITPERFILE < 0)) {
								fprintf(stderr, "%s: invalid output limit %s\n", GProgname, p+1);
								RETURN(usage());
							}
						}
						agrep_argv[agrep_argc] = (char *)my_malloc(strlen(p+1) + 2);
						strcpy(agrep_argv[agrep_argc], p+1);
						if (c == 'd') {
							preprocess_delimiter(p+1, D_length-2, GD_pattern, &GD_length);
							if (GOUTTAIL == 2) GOUTTAIL = 0;
							/* Should this be RegionLimit if ByteLevelIndex? */
						}
						if (c == 'k') GCONSTANT = 1;
					}
					agrep_argc ++;
#if	DEBUG
					fprintf(stderr, "%d = %s\n", agrep_argc, agrep_argv[agrep_argc - 1]);
#endif	/*DEBUG*/
					quitwhile = ON;
					if ((c == 'e') || (c == 'k')) foundpat = 1;
				}
				/* else it is something that glimpse doesn't know and agrep needs to look at */

				break;	/* from default: */

			} /* switch(c) */
			p ++;
		}
	} /* while (--argc > 0 && (*++argv)[0] == '-') */

/* exitloop: */
	if ((GBESTMATCH == ON) && (MATCHFILE == ON) && (Only_first == ON))
		fprintf(stderr, "%s: Warning: the number of matches may be incorrect when -B is used with -F.\n", HARVEST_PREFIX);

	if (GOUTTAIL) GOUTTAIL = 1;

	if (GNOFILENAME) {
		agrep_argv[my_A_index][1] = 'Z';	/* ignore the -A option */
	}

#if	ISSERVER
	if (RemoteFiles) {	/* force -NQ so that won't start looking for files! */
		Only_first = ON;
		PRINTAPPXFILEMATCH = ON;
	}
#endif

	if (argc > 0) {
		/* copy the rest of the options the pattern and the filenames if any verbatim */
		for (i=0; i<argc; i++) {
			if (agrep_argc >= MAX_ARGS) break;
			agrep_argv[agrep_argc] = (char *)my_malloc(strlen(argv[0]) + 2);
			strcpy(agrep_argv[agrep_argc], argv[0]);
			agrep_argc ++;
			argv ++; }
		if (!foundpat) argc --;
	}

#if	0
	for (j=0; j<agrep_argc; j++) printf("agrep_argv[%d] = %s\n", j, agrep_argv[j]);
	printf("argc = %d\n", argc);
#endif	/*0*/

	/*
	 * Now perform the search by first looking at the index
	 * and obtaining the files to search; and then search
	 * them and output the result. If argc > 0, glimpse
	 * runs as agrep: otherwise, it searches index, etc.
	 */

	if (argc <= 0) {
		if (RecordLevelIndex) {	/* based on work done for robint@zedcor.com Robin Thomas, Art Today, Tucson, AZ */
			/*
			if ((D_length > 0) && strcmp(GD_pattern, rdelim)) {
				fprintf(stderr, "Index created for delimiter `%s': cannot search with delimiter `%s'\n", rdelim, GD_pattern);
				RETURN(-1);
			}
			SHOULD I HAVE THIS CHECK? MAYBE GD_pattern is a SUBSTRING OF rdelim??? But this is safest thing to do... robint@zedcor.com
			*/
			RegionLimit = 0;	/* region is EXACTLY the same record number, not a portion of a file within some offset+length */
		}
		glimpse_call = 1;
		/* Initialize some data structures, read the index */
		if (GRECURSIVE == 1) {
			fprintf(stderr, "illegal option: '-r'\n");
			RETURN(usage());
		}
		num_terminals = 0;
		GParse = NULL;
		memset(terminals, '\0', sizeof(ParseTree) * MAXNUM_PAT);
#if	!ISSERVER
		if (-1 == read_index(indexdir)) RETURN(-1);
#endif	/*!ISSERVER*/

/*
This handles the -n option with ByteLevelIndex: disabled as of now, else should go into file search...
*/
		if (nobytelevelmustbeon && (ByteLevelIndex && !RecordLevelIndex)) {	/* with RecordLevelIndex, we'll do search, so don't set NOBYTELEVEL */
			/* fprintf(stderr, "Warning: -n option used with byte-level index: must SEARCH the files\n"); */
			NOBYTELEVEL=ON;
		}

		WHOLEFILESCOPE = (WHOLEFILESCOPE || wholefilescope);

		if (ByteLevelIndex) {
			/* Must zero them here in addition to index search so that RETURN macro runs correctly */
			if ((src_offset_table == NULL) &&
			    ((src_offset_table = (struct offsets **)my_malloc(sizeof(struct offsets *) * OneFilePerBlock)) == NULL)) exit(2);
			memset(src_offset_table, '\0', sizeof(struct offsets *) * OneFilePerBlock);
			for (i=0; i<MAXNUM_PAT; i++) {
				if ((multi_dest_offset_table[i] == NULL) &&
				    ((multi_dest_offset_table[i] = (struct offsets **)my_malloc(sizeof(struct offsets *) * OneFilePerBlock)) == NULL)) exit(2);
				memset(multi_dest_offset_table[i], '\0', sizeof(struct offsets *) * OneFilePerBlock);
			}
		}
		read_filters(INDEX_DIR, UseFilters);

		if (glimpse_clientdied) RETURN(0);
		/* Now initialize agrep, set the options and get the actual pattern into GPattern */
		if ((GM = fileagrep_init(agrep_argc, agrep_argv, MAXPAT, GPattern)) <= 0) {
			/* this printf need not be there: agrep prints messages if error */
			fprintf(stderr, "%s: error in options or arguments to `agrep'\n", HARVEST_PREFIX);
			RETURN(usage());
		}
		patindex = pattern_index;
		for (j=0; j<GM; j++) {
			if (GPattern[j] == '\\') j++;
			else if (test_indexable_char[GPattern[j]]) break;
		}
		if (j >= GM) {
			fprintf(stderr, "%s: pattern '%s' has no characters that were indexed: glimpse cannot search for it\n", HARVEST_PREFIX, GPattern);
			for (j=0; j<GM; j++) {
				if (GPattern[j] == '\\') j++;
				else if (isdigit(GPattern[j])) break;
			}
			if (j < GM) {
				fprintf(stderr, "\t(to search for numbers, make the index using 'glimpseindex -n ...')\n");
			}
			RETURN(-1);
		}

		/* Split GPattern into individual boolean terms */
		if (GCONSTANT) {
			strcpy(APattern, GPattern);
			GParse = NULL;
			ComplexBoolean = 0;
			terminals[0].op = 0;
			terminals[0].type = LEAF;
			terminals[0].terminalindex = 0;
			terminals[0].data.leaf.attribute = 0;
			terminals[0].data.leaf.value = (CHAR *)malloc(GM + 1);
			strcpy(terminals[0].data.leaf.value, GPattern);
			num_terminals = 1;
		}
		else if (split_pattern(GPattern, GM, APattern, terminals, &num_terminals, &GParse, StructuredIndex) <= 0) RETURN(-1);
#if	BG_DEBUG
		fprintf(debug, "GPattern = %s, APattern = %s, num_terminals = %d\n", GPattern, APattern, num_terminals);
#endif	/*BG_DEBUG*/

		/* Set scope for booleans */
		if (foundnot && !wholefilescope) {
			fprintf(stderr, "To use the NOT (~) operation, you must use whole file scope (-W) for booleans.\n");
			RETURN(usage())
		}
		if (foundattr) WHOLEFILESCOPE = 1;	/* makes no sense to search attribute=value expressions without WHOLEFILESCOPE */
		else if (!ComplexBoolean && !PRINTATTR && !((long)GParse & AND_EXP)) WHOLEFILESCOPE = 0;	/* ORs can be done without WHOLEFILESCOPE */
		if (WHOLEFILESCOPE <= 0) agrep_argv[my_b_index][1] = 'Z';
/*
		if (!ComplexBoolean && ((long)GParse & AND_EXP) && (my_l_index != -1)) agrep_argv[my_l_index][1] = 'Z';
*/
		if ((ComplexBoolean || ((long)GParse & AND_EXP)) && (my_l_index != -1)) agrep_argv[my_l_index][1] = 'Z';

		/* Now re-initialize agrep_argv with APattern instead of GPattern */
		my_free(agrep_argv[patindex], 0);
		AM=strlen(APattern);
		agrep_argv[patindex] = (char *)my_malloc(AM + 2);
		strcpy(agrep_argv[patindex], APattern);

		if (HINTSFROMUSER) {
			int	num=0, x, y, i, j;
			char	temp[MAX_NAME_SIZE+2];
			struct offsets *o, *tailo, *heado;

			while(1) {
				if ((num = readline(newsockfd, dest_index_buf, REAL_INDEX_BUF)) <= 0) {
					fprintf(stderr, "Input format error with -U option\n");
					RETURN(-1);
				}
				dest_index_buf[num+1] = '\n';
				if (!strncmp(dest_index_buf, "BEGIN", strlen("BEGIN"))) break;
			}
			sscanf(&dest_index_buf[strlen("BEGIN")], "%d%d%d", &bestmatcherrors, &NOBYTELEVEL, &OPTIMIZEBYTELEVEL);
			/* printf("BEGIN %d %d %d\n", bestmatcherrors, NOBYTELEVEL, OPTIMIZEBYTELEVEL);	*/
			num = readline(newsockfd, dest_index_buf, REAL_INDEX_BUF);
			while (num > 0) {
				dest_index_buf[num+1] = '\n';
				if (!strncmp(dest_index_buf, "END", strlen("END"))) break;
				i = j = 0;
				while ((j<MAX_NAME_SIZE) && (dest_index_buf[i] != FILE_END_MARK) && (dest_index_buf[i] != '[') && (dest_index_buf[i] != '\n'))
					temp[j++] = dest_index_buf[i++];
				temp[j] = '\0';
				x = atoi(temp);
				GFileIndex[GNumfiles] = x;
				if (x == file_num - 1) {
					bigbuffer[bigbuffer_size] = '\0';
					GTextfiles[GNumfiles++] = (CHAR *)strdup(GTextfilenames[x]);
					bigbuffer[bigbuffer_size] = '\n';
				}
				else {
					*(GTextfilenames[x+1] - 1) = '\0';
					GTextfiles[GNumfiles++] = (CHAR *)strdup(GTextfilenames[x]);
					*(GTextfilenames[x+1] - 1) = '\n';
				}
				/* printf("%d %s [", x, GTextfiles[GNumfiles-1]); */
				src_index_set[block2index(x)] |= block2mask(x);
				if (ByteLevelIndex && !NOBYTELEVEL) {	/* NOBYTELEVEL is 0 here with RecordLevelIndex */
					heado = tailo = NULL;
				onemorey:
					j = 0;
					while ((j<MAX_NAME_SIZE) && ((dest_index_buf[i] == FILE_END_MARK) || (dest_index_buf[i] == '['))) i++;
					while ((j<MAX_NAME_SIZE) && (dest_index_buf[i] != FILE_END_MARK) && (dest_index_buf[i] != '\n') && (dest_index_buf[i] != ']'))
						temp[j++] = dest_index_buf[i++];
					temp[j] = '\0';
					y = atoi(temp);
					/* printf(" %d", y); */
					o = (struct offsets *)my_malloc(sizeof(struct offsets));
					o->offset = y;
					o->next = NULL;
					o->sign = o->done = 0;
					if (heado == NULL) {
						heado = o;
						tailo = o;
					}
					else {
						tailo->next = o;
						tailo = o;
					}
					if (dest_index_buf[i] == FILE_END_MARK) goto onemorey;
					src_offset_table[x] = heado;
				}
				/* printf("]\n"); */
				num = readline(newsockfd, dest_index_buf, REAL_INDEX_BUF);
			}
			goto search_files;
		}

		/*
		 * Copy the agrep-options that are relevant to index search into
		 * index_argv (see man-pages for which options are relevant).
		 * Also, adjust patindex whenever options are skipped over.
		 * NOTE: agrep_argv does NOT contain two options after one '-'.
		 */
		index_argc = 0;
		for (j=0; j<agrep_argc; j++) {
			if (agrep_argv[j][0] == '-') {
				if ((agrep_argv[j][1] == 'c') || (agrep_argv[j][1] == 'h') || (agrep_argv[j][1] == 'l') || (agrep_argv[j][1] == 'n') ||
				    (agrep_argv[j][1] == 's') || (agrep_argv[j][1] == 't') || (agrep_argv[j][1] == 'G') || (agrep_argv[j][1] == 'O') ||
				    (agrep_argv[j][1] == 'b') || (agrep_argv[j][1] == 'i') || (agrep_argv[j][1] == 'u') || (agrep_argv[j][1] == 'g') ||
				    (agrep_argv[j][1] == 'E') || (agrep_argv[j][1] == 'Z') || (agrep_argv[j][1] == 'j') || (agrep_argv[j][1] == 'X')) {
				    patindex --;
				    continue;
				}
				if ((agrep_argv[j][1] == 'd') || (agrep_argv[j][1] == 'L')) {	/* skip over the argument too */
				    j++;
				    patindex -= 2;
				    continue;
				}
				if ((agrep_argv[j][1] == 'e') || (agrep_argv[j][1] == 'm')) {
					strcpy(index_argv[index_argc], agrep_argv[j]);
					index_argc ++; j++;
					strcpy(index_argv[index_argc], agrep_argv[j]);
					if (agrep_argv[j-1][1] == 'm') patbufpos = index_argc;	/* where to put the patbuf if fast-boolean by mgrep() */
					index_argc ++;
				}
				else {	/* No arguments: just copy THAT option: maybe, change some options */
					strcpy(index_argv[index_argc], agrep_argv[j]);
					if (agrep_argv[j][1] == 'A') index_argv[index_argc][1] = 'h';
					else if (agrep_argv[j][1] == 'x') index_argv[index_argc][1] = 'w';
					index_argc++;
				}
			}
			else {	/* This is either the pattern itself or a filename */
				strcpy(index_argv[index_argc], agrep_argv[j]);
				index_argc++;
			}
		}
		sprintf(index_argv[index_argc], "%s", INDEX_FILE);
		index_argc ++;
#if	0
		for (j=0; j<index_argc; j++) printf("index_argv[%d] = %s\n", j, index_argv[j]);
		printf("patindex = %d\n", patindex);
#endif	/*0*/

		/* process -Y option BEFORE search_index() so that get_set() doesn't even have to collect data for very old files */
		ret = process_Y_option(file_num, GNumDays, timesindexfp);

		/* Search the index and process index-search-only options; Worry about file-pattern */
		ret = search_index(GParse);
		if ((ret <= 0) && (!OneFilePerBlock || (src_index_set[REAL_PARTITION - 1] != 1))) RETURN(-1);	/* previously was: if ret < 0 then return... */
		num_blocks=0;
		if (OneFilePerBlock) {
			if (ByteLevelIndex || RecordLevelIndex) {

			if (src_offset_table)   /* Don't iterate if null */
			for(iii=0; iii<OneFilePerBlock; iii++) {
				if (src_offset_table[iii] != NULL) num_blocks ++;
			}
			/* printf("num_blocks = %d\n", num_blocks); */
			}
			if (src_index_set[REAL_PARTITION - 1] == 1) {
				num_blocks = OneFilePerBlock;
				for(iii=0; iii<round(OneFilePerBlock, 8*sizeof(int)) - 1; iii++) {
					src_index_set[iii] = 0xffffffff;
				}
				src_index_set[iii] = 0;
				for (jjj=0; jjj<8*sizeof(int); jjj++) {
					if (iii*8*sizeof(int) + jjj >= OneFilePerBlock) break;
					src_index_set[iii] |= mask_int[jjj];
				}
			}
			else for(iii=0; iii<round(OneFilePerBlock, 8*sizeof(int)); iii++) {
				if (src_index_set[iii] == 0) continue;
				for (jjj=0; jjj < 8*sizeof(int); jjj++)
					if (src_index_set[iii] & mask_int[jjj])
						if (!ByteLevelIndex || NOBYTELEVEL) {
							num_blocks ++;
						}
						else {
							if (src_offset_table[iii*8*sizeof(int) + jjj] != NULL)
								num_blocks ++;
							else src_index_set[iii] &= ~mask_int[jjj];
						}
			}
			if (num_blocks > OneFilePerBlock) num_blocks = OneFilePerBlock;	/* roundoff */
		}
		else {
			for (iii=0; iii<MAX_PARTITION; iii++)
				if (src_index_set[iii]) num_blocks++;
		}
		if (num_blocks <= 0) RETURN (0);
		if ((src_index_set[REAL_PARTITION - 1] == 1) && !Only_first && !OPTIMIZEBYTELEVEL) {
			fprintf(stderr, "Warning: pattern has words present in the stop-list: must SEARCH the files\n");
		}
		if (RecordLevelIndex && GFILENAMEONLY && veryfast && (src_index_set[REAL_PARTITION - 1] != 1)) Only_first = 1;
		/* if just the NOBYTELEVEL flag is set, then it is an optimization which glimpse does and user need not be warned */
#if	DEBUG
		fprintf(stderr, "--> search=%d optimize=%d times=%d all=%d blocks=%d len=%d pat=%s scope=%d\n",
			NOBYTELEVEL, OPTIMIZEBYTELEVEL, src_index_set[REAL_PARTITION - 2], src_index_set[REAL_PARTITION - 1], num_blocks, strlen(APattern), APattern, WHOLEFILESCOPE);
#endif	/*DEBUG*/

		/* Based on contribution From ada@mail2.umu.se Fri Jul 12 01:56 MST 1996; Christer Holgersson, Sen. SysNet Mgr, Umea University/SUNET, Sweden */
		if (BITFIELDFILE) {
			int i, len = -1, nextchar;
			FILE	*fp;
			fp = fopen(bitfield_file, "r");
			if (fp != NULL) {
				if (BITFIELDENDIAN >= 2) {	/* is a BIG-ENDIAN 4B integer list of indexes of files in .glimpse_filenames (sparse set) */
					if (BITFIELDLENGTH == 0) BITFIELDLENGTH = file_num;
					if (BITFIELDOFFSET > 0) fseek(fp, BITFIELDOFFSET, (long)0);
					if (OneFilePerBlock) {
						for (i=0; i<round(file_num, 8*sizeof(int)); i++)
							multi_dest_index_set[0][i] = 0;
					}
					else {
						for (i=0; i<MAX_PARTITION; i++)
							multi_dest_index_set[0][i] = 0;
					}
					i = -1;
					while ((nextchar = getc(fp)) != EOF) {
						nextchar = nextchar & 0xff;
						i = nextchar << 24;
						if ((nextchar = getc(fp)) == EOF) break;
						nextchar = nextchar & 0xff;
						i |= nextchar << 16;
						if ((nextchar = getc(fp)) == EOF) break;
						nextchar = nextchar & 0xff;
						i |= nextchar << 8;
						if ((nextchar = getc(fp)) == EOF) break;
						nextchar = nextchar & 0xff;
						i |= nextchar;

						if (OneFilePerBlock) {
							if (i < file_num) multi_dest_index_set[0][block2index(i)] |= mask_int[i%(8*sizeof(int))];
						}
						else {
							if (i < MAX_PARTITION) multi_dest_index_set[0][i] = 1;
						}
					}
					fclose(fp);
					if (i == -1) {
						fprintf(stderr, "Error in reading %d bytes from offset %d in bitfield file %s ... ignoring it\n", BITFIELDOFFSET, BITFIELDLENGTH, bitfield_file);
						/* ignore index_file */
					}
					else {	/* intersect files in index_file with those that were obtained after pattern search */
						if (OneFilePerBlock) {
							for (i=0; i<round(file_num, 8*sizeof(int)); i++)
								src_index_set[i] &= multi_dest_index_set[0][i];
						}
						else {
							for (i=0; i<MAX_PARTITION; i++)
								src_index_set[i] &= multi_dest_index_set[0][i];
						}
					}
				}
				else {
					if (BITFIELDLENGTH == 0) BITFIELDLENGTH = sizeof(int)*REAL_PARTITION /* sizeof(int)*FILEMASK_SIZE */;
					if (BITFIELDOFFSET > 0) fseek(fp, BITFIELDOFFSET, (long)0);
					if (OneFilePerBlock) {
						for (i=0; i<round(file_num, 8*sizeof(int)); i++)
							multi_dest_index_set[0][i] = 0;
					}
					else {
						for (i=0; i<MAX_PARTITION; i++)
							multi_dest_index_set[0][i] = 0;
					}
					i = 0;
					while ((i < BITFIELDLENGTH) && (nextchar = getc(fp)) != EOF) {
						nextchar = nextchar & 0xff;
						if (OneFilePerBlock) {
							if (BITFIELDENDIAN == 1) {	/* little-endian: little end of integer was dumped first in bitfield_file */
								multi_dest_index_set[0][i/4] |= (nextchar << (8*(i%4)));
							}
							else if (BITFIELDENDIAN == 0) {	/* big-endian: big end of integer is first was dumped first in bitfield_file */
								multi_dest_index_set[0][i/4] |= (nextchar << (8*(4-1-(i%4))));
							}
						}
						else {
							if (i < MAX_PARTITION) {	/* interpretation of "bit" changes without OneFilePerBlock */
								multi_dest_index_set[0][i] = (nextchar != 0) ? 1 : 0;
							}
							else break;	/* BITFIELDLENGTH, by above definition, is always > MAX_PARTITION: see io.c */
						}
						i++;
					}
					fclose(fp);
					if (i <= 0) {
						fprintf(stderr, "Error in reading %d bytes from offset %d in bitfield file %s ... ignoring it\n", BITFIELDOFFSET, BITFIELDLENGTH, bitfield_file);
						/* ignore bitfield_file */
					}
					else {	/* intersect files in bitfield_file with those that were obtained after pattern search */
						if (OneFilePerBlock) {
							for (i=0; i<round(file_num, 8*sizeof(int)); i++)
								src_index_set[i] &= multi_dest_index_set[0][i];
						}
						else {
							for (i=0; i<MAX_PARTITION; i++)
								src_index_set[i] &= multi_dest_index_set[0][i];
						}
					}
				}
			}
		}

		if (FILENAMESINFILE) mask_filenames(src_index_set, filenames_file, file_num, num_blocks);	/* keep only those files that are in filenames_file */
		if (BITFIELDFILE || FILENAMESINFILE) {
			num_blocks=0;
			if (OneFilePerBlock) {
				for(iii=0; iii<round(OneFilePerBlock, 8*sizeof(int)); iii++) {
					if (src_index_set[iii] == 0) continue;
					for (jjj=0; jjj < 8*sizeof(int); jjj++)
						if (src_index_set[iii] & mask_int[jjj])
							if (!ByteLevelIndex || NOBYTELEVEL) {
								num_blocks ++;
							}
							else {
								if (src_offset_table[iii*8*sizeof(int) + jjj] != NULL)
									num_blocks ++;
								else src_index_set[iii] &= ~mask_int[jjj];
							}
				}
				if (num_blocks > OneFilePerBlock) num_blocks = OneFilePerBlock;	/* roundoff */
			}
			else {
				for (iii=0; iii<MAX_PARTITION; iii++)
					if (src_index_set[iii]) num_blocks++;
			}
			if (num_blocks <= 0) RETURN (0);
		}

		dummypat[0] = '\0';
		if (!MATCHFILE)	{	/* the argc,argv don't matter */
			get_filenames(src_index_set, 0, NULL, dummylen, dummypat, file_num);

			if (Only_first) { /* search the index only */
				fprintf(stderr, "There are matches to %d out of %d %s\n", num_blocks, (OneFilePerBlock > 0) ? OneFilePerBlock : GNumpartitions, (OneFilePerBlock > 0) ? "files" : "blocks");
				if (num_blocks > 0) {
					char	cc[8];
					cc[0] = 'y';
#if	!ISSERVER
					if (!GNOPROMPT) {
						fprintf(stderr, "Do you want to see the file names? (y/n)");
						fgets(cc, 4, stdin);
					}
#endif	/*!ISSERVER*/
					if (!SILENT && (cc[0] == 'y')) {
						if (PRINTAPPXFILEMATCH && Only_first && GPRINTFILENUMBER) {
							printf("BEGIN %d %d %d\n", bestmatcherrors, NOBYTELEVEL, OPTIMIZEBYTELEVEL);
						}
						for (jjj=0; jjj<GNumfiles; jjj++) {
							if ((GLIMITOUTPUT > 0) && (jjj >= GLIMITOUTPUT)) break;
							if (ByteLevelIndex && !NOBYTELEVEL && (src_index_set[REAL_PARTITION - 1] != 1) && (src_offset_table[GFileIndex[jjj]] == NULL)) continue;
							if (GPRINTFILENUMBER) printf("%d", GFileIndex[jjj]);
							else printf("%s", GTextfiles[jjj]);
							if (PRINTAPPXFILEMATCH) {
							    if (GCOUNT) {
								int n = 0;
								printf(": ");
								if (ByteLevelIndex && (src_offset_table != NULL)) {
									struct offsets  *p1 = src_offset_table[GFileIndex[jjj]];
									while (p1 != NULL) {
										n ++;
										p1 = p1->next;
									}
								}
								else n = 1;	/* there is atleast 1 match */
								printf("%d", n);
							    }
							    else {
								printf(" [");
								if (ByteLevelIndex && (src_offset_table != NULL)) {
									struct offsets  *p1 = src_offset_table[GFileIndex[jjj]];
									while (p1 != NULL) {
										printf(" %d", p1->offset);
										p1 = p1->next;
									}
								}
								printf("]");
							    }
							}
							printf("\n");
						}
						if (PRINTAPPXFILEMATCH && Only_first && GPRINTFILENUMBER) {
							printf("END\n");
						}
					}
				}
				RETURN(0);
			} /* end of Only_first */
			if (!OneFilePerBlock) searchpercent = num_blocks*100/GNumpartitions;
			else searchpercent = num_blocks * 100 / OneFilePerBlock;
#if	BG_DEBUG
			fprintf(debug, "searchpercent = %d, num_blocks = %d\n", searchpercent, num_blocks);
#endif	/*BG_DEBUG*/
#if	!ISSERVER
			if (!GNOPROMPT && (searchpercent > MAX_SEARCH_PERCENT)) {
				char	cc[8];
				cc[0] = 'y';
				fprintf(stderr, "Your query may search about %d%% of the total space! Continue? (y/n)", searchpercent); 
				fgets(cc, 4, stdin);
				if (cc[0] != 'y') RETURN(0);
			}
			if (ByteLevelIndex && !RecordLevelIndex && (searchpercent > DEF_MAX_INDEX_PERCENT)) NOBYTELEVEL = 1;	/* with RecordLevelIndex, I don't just want to stop collecting offsets just because searchpercent > .... */
#endif	/*!ISSERVER*/
		} /* end of !MATCHFILE */
		else {	/* set up the right options for -F in index_argv/index_argc itself since they will no longer be used */
			index_argc=0;
			strcpy(index_argv[0], GProgname);

			/* adding the -h option, which is safer for -F */
			index_argc ++;
			index_argv[index_argc][0] =  '-';
			index_argv[index_argc][1] = 'h';
			index_argv[index_argc][2] = '\0';
			index_argc ++;

			/* new code: bgopal, Feb/8/94: deleted udi's code here */
			j = 0;
			while (FileOpt[j] == '-') {
				j++;
				while ((FileOpt[j] != ' ') && (FileOpt[j] != '\0') && (FileOpt[j] != '\n')) {
					if (j >= MAX_ARGS - 1) {
						fprintf(stderr, "%s: too many options after -F: %s\n", GProgname, FileOpt);
						RETURN(usage());
					}
					index_argv[index_argc][0] =  '-';
					index_argv[index_argc][1] = FileOpt[j];
					index_argv[index_argc][2] = '\0';
					index_argc ++;
					j++;
				}
				if ((FileOpt[j] == '\0') || (FileOpt[j] == '\n')) break;
				if ((FileOpt[j] == ' ') && (FileOpt[j-1] == '-')) {
					fprintf(stderr, "%s: illegal option: '-' after -F\n", GProgname);
					RETURN(usage());
				}
				else if (FileOpt[j] == ' ') while(FileOpt[j] == ' ') j++;
			}
			while(FileOpt[j] == ' ') j++;

			fileopt_length = strlen(FileOpt);
			strncpy(index_argv[index_argc],FileOpt+j,fileopt_length-j);
			index_argv[index_argc][fileopt_length-j] = '\0';
			index_argc++;
			my_free(FileOpt, MAXFILEOPT);
			FileOpt = NULL;

#if	BG_DEBUG
			fprintf(debug, "pattern to check with -F = %s\n",index_argv[index_argc-1]);
#endif	/*BG_DEBUG*/
#if	DEBUG
			fprintf(stderr, "-F : ");
			for (jj=0; jj < index_argc; jj++) 
				fprintf(stderr, " %s ",index_argv[jj]);
			fprintf(stderr, "\n");
#endif	/*DEBUG*/
			fflush(stdout);
			get_filenames(src_index_set, index_argc, index_argv, dummylen, dummypat, file_num);

			/* Assume #files per partitions is appx constant */
			if (OneFilePerBlock) num_blocks = GNumfiles;
			else num_blocks = GNumfiles * GNumpartitions / p_table[GNumpartitions - 1];
			if (Only_first) { /* search the index only */
				fprintf(stderr, "There are matches to %d out of %d %s\n", num_blocks, (OneFilePerBlock > 0) ? OneFilePerBlock : GNumpartitions, (OneFilePerBlock > 0) ? "files" : "blocks");
				if (num_blocks > 0) {
					char	cc[8];
					cc[0] = 'y';
#if	!ISSERVER
					if (!GNOPROMPT) {
						fprintf(stderr, "Do you want to see the file names? (y/n)");
						fgets(cc, 4, stdin);
					}
#endif	/*!ISSERVER*/
					if (!SILENT && (cc[0] == 'y')) {
						if (PRINTAPPXFILEMATCH && Only_first && GPRINTFILENUMBER) {
							printf("BEGIN %d %d %d\n", bestmatcherrors, NOBYTELEVEL, OPTIMIZEBYTELEVEL);
						}
						for (jjj=0; jjj<GNumfiles; jjj++) {
							if ((GLIMITOUTPUT > 0) && (jjj >= GLIMITOUTPUT)) break;
							if (ByteLevelIndex && !NOBYTELEVEL && (src_index_set[REAL_PARTITION - 1] != 1) && (src_offset_table[GFileIndex[jjj]] == NULL)) continue;
							if (GPRINTFILENUMBER) printf("%d", GFileIndex[jjj]);
							else printf("%s", GTextfiles[jjj]);
							if (PRINTAPPXFILEMATCH) {
							    if (GCOUNT) {
								int n = 0;
								printf(": ");
								if (ByteLevelIndex && (src_offset_table != NULL)) {
									struct offsets  *p1 = src_offset_table[GFileIndex[jjj]];
									while (p1 != NULL) {
										n ++;
										p1 = p1->next;
									}
								}
								else n = 1;	/* there is atleast 1 match */
								printf("%d", n);
							    }
							    else {
								printf("[");
								if (ByteLevelIndex && (src_offset_table != NULL)) {
									struct offsets  *p1 = src_offset_table[GFileIndex[jjj]];
									while (p1 != NULL) {
										printf(" %d", p1->offset);
										p1 = p1->next;
									}
								}
								printf("]");
							    }
							}
							printf("\n");
						}
						if (PRINTAPPXFILEMATCH && Only_first && GPRINTFILENUMBER) {
							printf("END\n");
						}
					}
				}
				RETURN(0);
			} /* end of Only_first */
			if (OneFilePerBlock) searchpercent = GNumfiles * 100 / OneFilePerBlock;
			else searchpercent = GNumfiles * 100 / p_table[GNumpartitions - 1];
#if	BG_DEBUG
			fprintf(debug, "searchpercent = %d, num_files = %d\n", searchpercent, p_table[GNumpartitions - 1]);
#endif	/*BG_DEBUG*/
#if	!ISSERVER
			if (!GNOPROMPT && (searchpercent > MAX_SEARCH_PERCENT)) {
				char	cc[8];
				cc[0] = 'y';
				fprintf(stderr, "Your query may search about %d%% of the total space! Continue? (y/n)", searchpercent); 
				fgets(cc, 4, stdin);
				if (cc[0] != 'y') RETURN(0);
			}
			if (ByteLevelIndex && !RecordLevelIndex && (searchpercent > DEF_MAX_INDEX_PERCENT)) NOBYTELEVEL = 1;	/* with RecordLevelIndex, I don't just want to stop collecting offsets just because searchpercent > .... */
#endif	/*!ISSERVER*/
		}

		/* At this point, I have the set of files to search */

	search_files:
		/* Replace -B by the number of errors if best-match */
		if (GBESTMATCH && (my_B_index >= 0)) {
			sprintf(&agrep_argv[my_B_index][1], "%d", bestmatcherrors);
#if	BG_DEBUG
			fprintf(debug, "Changing -B to -%d\n", bestmatcherrors);
#endif	/*BG_DEBUG*/
		}
		agrep_argv[my_M_index][1] = 'Z';
		agrep_argv[my_P_index][1] = 'Z';
#if	0
		for (iii=0; iii<agrep_argc; iii++) fprintf(stdout, "'%s' ", agrep_argv[iii]);
		fprintf(stdout, "\n");
#endif
/*
		if (!ComplexBoolean && ((long)GParse & AND_EXP) && (my_l_index != -1) && !WHOLEFILESCOPE) agrep_argv[my_l_index][1] = 'l';
*/
		if ((ComplexBoolean || ((long)GParse & AND_EXP)) && (my_l_index != -1) && !WHOLEFILESCOPE) agrep_argv[my_l_index][1] = 'l';

		if (GNumfiles <= 0) RETURN(0);
		if (glimpse_clientdied) RETURN(0);
		/* must reinitialize since the above agrep calls for index-search ruined the real options: it is required EVEN IF ByteLevelIndex */
		AM = fileagrep_init(agrep_argc, agrep_argv, MAXPAT, APattern);
		/* do actual search with postfiltering if structured query */
		if (WHOLEFILESCOPE <= 0) {
			if (!UseFilters) {
				if (!ByteLevelIndex || RecordLevelIndex || NOBYTELEVEL) {
					for (i=0; i<GNumfiles; i++) {
						/* if (RecordLevelIndex && (src_offset_table[GFileIndex[i]] == NULL)) continue; */
						gprev_num_of_matched = gnum_of_matched;
						SetCurrentFileName = 1;
						if (GPRINTFILENUMBER) sprintf(CurrentFileName, "%d", GFileIndex[i]);
						else strcpy(CurrentFileName, GTextfiles[i]);
						if (GPRINTFILETIME) {
							SetCurrentFileTime = 1;
							CurrentFileTime = get_file_time(timesfp, NULL, GTextfiles[i], GFileIndex[i]);
						}
						if ((ret = fileagrep_search(AM, APattern, 1, &GTextfiles[i], 0, stdout)) > 0) {
							gnum_of_matched += ret;
							gfiles_matched ++;
						}
						SetCurrentFileName = 0;
						if (GPRINTFILETIME) SetCurrentFileTime = 0;
						if (GLIMITOUTPUT > 0) {
							if (GLIMITOUTPUT <= gnum_of_matched) break;
							LIMITOUTPUT = GLIMITOUTPUT - gnum_of_matched;
						}
						if (GLIMITTOTALFILE > 0) {
							if (GLIMITTOTALFILE <= gfiles_matched) break;
							LIMITTOTALFILE = GLIMITTOTALFILE - gfiles_matched;
						}
 						if ((ret < 0) && (errno == AGREP_ERROR)) break;
						if (glimpse_clientdied) break;
						fflush(stdout);
					}
				}
				else {
					for (i=0; i<GNumfiles; i++) {
						gprev_num_of_matched = gnum_of_matched;
						SetCurrentFileName = 1;
						if (GPRINTFILENUMBER) sprintf(CurrentFileName, "%d", GFileIndex[i]);
						else strcpy(CurrentFileName, GTextfiles[i]);
						if (my_stat(GTextfiles[i], &file_stat_buf) == -1) {
							if (GPRINTNONEXISTENTFILE) printf("%s\n", CurrentFileName);
							continue;
						}
						if (GPRINTFILETIME) {
							SetCurrentFileTime = 1;
							CurrentFileTime = get_file_time(timesfp, &file_stat_buf, GTextfiles[i], GFileIndex[i]);
						}
						if (file_stat_buf.st_mtime > index_stat_buf.st_mtime) {
							/* fprintf(stderr, "Warning: file modified after indexing: must SEARCH %s\n", CurrentFileName); */
							free_list(&src_offset_table[GFileIndex[i]]);
							first_search = 1;
							if ((ret = fileagrep_search(AM, APattern, 1, &GTextfiles[i], 0, stdout)) > 0) {
								gnum_of_matched += ret;
								gfiles_matched ++;
							}
						}
						else if ((ret = glimpse_search(AM, APattern, GD_length, GD_pattern, GTextfiles[i], GTextfiles[i], GFileIndex[i], src_offset_table, stdout)) > 0) {
							gnum_of_matched += ret;
							gfiles_matched ++;
						}
						SetCurrentFileName = 0;
						if (GPRINTFILETIME) SetCurrentFileTime = 0;
						if (GLIMITOUTPUT > 0) {
							if (GLIMITOUTPUT <= gnum_of_matched) break;
							LIMITOUTPUT = GLIMITOUTPUT - gnum_of_matched;
						}
						if (GLIMITTOTALFILE > 0) {
							if (GLIMITTOTALFILE <= gfiles_matched) break;
							LIMITTOTALFILE = GLIMITTOTALFILE - gfiles_matched;
						}
 						if ((ret < 0) && (errno == AGREP_ERROR)) break;
						if (glimpse_clientdied) break;
						fflush(stdout);
					}
				}
			} /* end of !UseFilters */
			else {
				sprintf(outname[0], "%s/.glimpse_apply.%d", TEMP_DIR, getpid());
				for (i=0; i<GNumfiles; i++) {
					/* if (RecordLevelIndex && (src_offset_table[GFileIndex[i]] == NULL)) continue; */
					if (apply_filter(GTextfiles[i], outname[0]) == 1) {
						gprev_num_of_matched = gnum_of_matched;
						SetCurrentFileName = 1;
						if (GPRINTFILENUMBER) sprintf(CurrentFileName, "%d", GFileIndex[i]);
						else strcpy(CurrentFileName, GTextfiles[i]);
						if (my_stat(GTextfiles[i], &file_stat_buf) == -1) {
							if (GPRINTNONEXISTENTFILE) printf("%s\n", CurrentFileName);
							continue;
						}
						if (GPRINTFILETIME) {
							SetCurrentFileTime = 1;
							CurrentFileTime = get_file_time(timesfp, &file_stat_buf, GTextfiles[i], GFileIndex[i]);
						}
						if (!ByteLevelIndex || RecordLevelIndex || NOBYTELEVEL || (file_stat_buf.st_mtime > index_stat_buf.st_mtime)) {
							first_search = 1;
							if ((ret = fileagrep_search(AM, APattern, 1, outname, 0, stdout)) > 0) {
								gnum_of_matched += ret;
								gfiles_matched ++;
							}
						}
						else {
							if (file_stat_buf.st_mtime > index_stat_buf.st_mtime) {
								/* fprintf(stderr, "Warning: file modified after indexing: must SEARCH %s\n", CurrentFileName); */
								free_list(&src_offset_table[GFileIndex[i]]);
								first_search = 1;
								if ((ret = fileagrep_search(AM, APattern, 1, outname, 0, stdout)) > 0) {
									gnum_of_matched += ret;
									gfiles_matched ++;
								}
							}
							else if ((ret = glimpse_search(AM, APattern, GD_length, GD_pattern, GTextfiles[i], outname[0], GFileIndex[i], src_offset_table, stdout)) > 0) {
								gfiles_matched ++;
								gnum_of_matched += ret;
							}
						}
						SetCurrentFileName = 0;
						if (GPRINTFILETIME) SetCurrentFileTime = 0;
					}
					else {
						if (!ByteLevelIndex || RecordLevelIndex || NOBYTELEVEL) {
							first_search = 1;
							if ((ret = fileagrep_search(AM, APattern, 1, &GTextfiles[i], 0, stdout)) > 0) {
								gnum_of_matched += ret;
								gfiles_matched ++;
							}
						}
						else {
							SetCurrentFileName = 1;
							if (GPRINTFILENUMBER) sprintf(CurrentFileName, "%d", GFileIndex[i]);
							else strcpy(CurrentFileName, GTextfiles[i]);
							if (my_stat(GTextfiles[i], &file_stat_buf) == -1) {
								if (GPRINTNONEXISTENTFILE) printf("%s\n", CurrentFileName);
								continue;
							}
							if (GPRINTFILETIME) {
								SetCurrentFileTime = 1;
								CurrentFileTime = get_file_time(timesfp, &file_stat_buf, GTextfiles[i], GFileIndex[i]);
							}
							if (file_stat_buf.st_mtime > index_stat_buf.st_mtime) {
								/* fprintf(stderr, "Warning: file modified after indexing: must SEARCH %s\n", CurrentFileName); */
								free_list(&src_offset_table[GFileIndex[i]]);
								first_search = 1;
								if ((ret = fileagrep_search(AM, APattern, 1, &GTextfiles[i], 0, stdout)) > 0) {
									gnum_of_matched += ret;
									gfiles_matched ++;
								}
							}
							else if ((ret = glimpse_search(AM, APattern, GD_length, GD_pattern, GTextfiles[i], GTextfiles[i], GFileIndex[i], src_offset_table, stdout)) > 0) {
								gnum_of_matched += ret;
								gfiles_matched ++;
							}
							SetCurrentFileName = 0;
							if (GPRINTFILETIME) SetCurrentFileTime = 0;
						}
					}
					if (GLIMITOUTPUT > 0) {
						if (GLIMITOUTPUT <= gnum_of_matched) break;
						LIMITOUTPUT = GLIMITOUTPUT - gnum_of_matched;
					}
					if (GLIMITTOTALFILE > 0) {
						if (GLIMITTOTALFILE <= gfiles_matched) break;
						LIMITTOTALFILE = GLIMITTOTALFILE - gfiles_matched;
					}
 					if ((ret < 0) && (errno == AGREP_ERROR)) break;
					if (glimpse_clientdied) break;
					fflush(stdout);
				}
			}
		} /* end of WHOLEFILESCOPE <= 0 */
		else {
			FILE	*tmpfp = NULL;	/* to store structured query-search output */
			int	OLDFILENAMEONLY;/* don't use FILENAMEONLY for agrepping the stuff: handle it in filtering */
			int     OLDLIMITOUTPUT; /* don't use LIMITs for search: only for filtering=identify_region(): agrep NEVER changes these 3 */
			int	OLDLIMITPERFILE;
			int	OLDLIMITTOTALFILE;
			int	OLDPRINTRECORD;	/* don't use PRINTRECORD for search: only after filter_output() recognizes boolean in wholefilescope */
			int	OLDCOUNT;	/* don't use OLDCOUNT for search: only after filter_output() recognizes boolean in wholefilescope */

			if (!UseFilters) {
				for (i=0; i<GNumfiles; i++) {
					/* if (RecordLevelIndex && (src_offset_table[GFileIndex[i]] == NULL)) continue; */
					OLDFILENAMEONLY = FILENAMEONLY;
					FILENAMEONLY = 0;
					OLDLIMITOUTPUT = LIMITOUTPUT;
					LIMITOUTPUT = 0;
					OLDLIMITPERFILE = LIMITPERFILE;
					LIMITPERFILE = 0;
					OLDLIMITTOTALFILE = LIMITTOTALFILE;
					LIMITTOTALFILE = 0;
					OLDPRINTRECORD = PRINTRECORD;
					PRINTRECORD = 1;
					OLDCOUNT = COUNT;
					COUNT = 0;
					gprev_num_of_matched = gnum_of_matched;
					if ((tmpfp = fopen(tempfile, "w")) == NULL) {
						fprintf(stderr, "%s: cannot open for writing: %s, errno=%d\n", GProgname, tempfile, errno);
						RETURN(usage());
					}
					SetCurrentFileName = 1;
					if (GPRINTFILENUMBER) sprintf(CurrentFileName, "%d", GFileIndex[i]);
					else strcpy(CurrentFileName, GTextfiles[i]);
					if (!ByteLevelIndex || RecordLevelIndex || NOBYTELEVEL) {
						if (GPRINTFILETIME) {
							SetCurrentFileTime = 1;
							CurrentFileTime = get_file_time(timesfp, NULL, GTextfiles[i], GFileIndex[i]);
						}
						first_search = 1;
						ret = fileagrep_search(AM, APattern, 1, &GTextfiles[i], 0, tmpfp);
					}
					else {
						if (my_stat(GTextfiles[i], &file_stat_buf) == -1) {
							if (GPRINTNONEXISTENTFILE) printf("%s\n", CurrentFileName);
							fclose(tmpfp);
							continue;
						}
						if (GPRINTFILETIME) {
							SetCurrentFileTime = 1;
							CurrentFileTime = get_file_time(timesfp, &file_stat_buf, GTextfiles[i], GFileIndex[i]);
						}
						if (file_stat_buf.st_mtime > index_stat_buf.st_mtime) {
							/* fprintf(stderr, "Warning: file modified after indexing: must SEARCH %s\n", CurrentFileName); */
							free_list(&src_offset_table[GFileIndex[i]]);
							first_search = 1;
							ret = fileagrep_search(AM, APattern, 1, &GTextfiles[i], 0, tmpfp);
						}
						else ret = glimpse_search(AM, APattern, GD_length, GD_pattern, GTextfiles[i], GTextfiles[i], GFileIndex[i], src_offset_table, tmpfp);
					}
					SetCurrentFileName = 0;
					if (GPRINTFILETIME) SetCurrentFileTime = 0;
					fflush(tmpfp);
					fclose(tmpfp);
					tmpfp = NULL;
					if ((ret < 0) && (errno == AGREP_ERROR)) break;
#if	DEBUG
					printf("done search\n");
					fflush(stdout);
#endif	/*DEBUG*/
					FILENAMEONLY = OLDFILENAMEONLY;
					LIMITOUTPUT = OLDLIMITOUTPUT;
					LIMITPERFILE = OLDLIMITPERFILE;
					LIMITTOTALFILE = OLDLIMITTOTALFILE;
					PRINTRECORD = OLDPRINTRECORD;
					COUNT = OLDCOUNT;
                                        if (!UseFilters) {
                                               ret = filter_output(GTextfiles[i], tempfile, GParse, GD_pattern, GD_length, GOUTTAIL, nullfp, StructuredIndex);
                                        } else {
                                               ret = filter_output(outname[0], tempfile, GParse, GD_pattern, GD_length, GOUTTAIL, nullfp, StructuredIndex);
                                               unlink(outname[0]);
                                        }
					gnum_of_matched += (ret > 0) ? ret : 0;
					gfiles_matched += (ret > 0) ? 1 : 0;
					if (GLIMITOUTPUT > 0) {
						if (GLIMITOUTPUT <= gnum_of_matched) break;
						LIMITOUTPUT = GLIMITOUTPUT - gnum_of_matched;
					}
					if (GLIMITTOTALFILE > 0) {
						if (GLIMITTOTALFILE <= gfiles_matched) break;
						LIMITTOTALFILE = GLIMITTOTALFILE - gfiles_matched;
					}
					if (glimpse_clientdied) break;
					fflush(stdout);
				}
			}
			else {	/* we should try to apply the filter (we come here with -W -z, say) */
				sprintf(outname[0], "%s/.glimpse_apply.%d", TEMP_DIR, getpid());
				for (i=0; i<GNumfiles; i++) {
					/* if (RecordLevelIndex && (src_offset_table[GFileIndex[i]] == NULL)) continue; */
					OLDFILENAMEONLY = FILENAMEONLY;
					FILENAMEONLY = 0;
					OLDLIMITOUTPUT = LIMITOUTPUT;
					LIMITOUTPUT = 0;
					OLDLIMITPERFILE = LIMITPERFILE;
					LIMITPERFILE = 0;
					OLDLIMITTOTALFILE = LIMITTOTALFILE;
					LIMITTOTALFILE = 0;
					OLDPRINTRECORD = PRINTRECORD;
					PRINTRECORD = 1;
					OLDCOUNT = COUNT;
					COUNT = 0;
					gprev_num_of_matched = gnum_of_matched;
					if ((tmpfp = fopen(tempfile, "w")) == NULL) {
						fprintf(stderr, "%s: cannot open for writing: %s, errno=%d\n", GProgname, tempfile, errno);
						RETURN(usage());
					}

					SetCurrentFileName = 1;
					if (GPRINTFILENUMBER) sprintf(CurrentFileName, "%d", GFileIndex[i]);
					else strcpy(CurrentFileName, GTextfiles[i]);
					if (apply_filter(GTextfiles[i], outname[0]) == 1) {
						if (my_stat(GTextfiles[i], &file_stat_buf) == -1) {
							if (GPRINTNONEXISTENTFILE) printf("%s\n", CurrentFileName);
							fclose(tmpfp);
							continue;
						}
						if (GPRINTFILETIME) {
							SetCurrentFileTime = 1;
							CurrentFileTime = get_file_time(timesfp, &file_stat_buf, GTextfiles[i], GFileIndex[i]);
						}
						if (!ByteLevelIndex || RecordLevelIndex || NOBYTELEVEL || (file_stat_buf.st_mtime > index_stat_buf.st_mtime)) {
							first_search = 1;
							ret = fileagrep_search(AM, APattern, 1, outname, 0, tmpfp);
						}
						else {
							if (file_stat_buf.st_mtime > index_stat_buf.st_mtime) {
								/* fprintf(stderr, "Warning: file modified after indexing: must SEARCH %s\n", CurrentFileName); */
								free_list(&src_offset_table[GFileIndex[i]]);
								first_search = 1;
								ret = fileagrep_search(AM, APattern, 1, outname, 0, tmpfp);
							}
							else ret = glimpse_search(AM, APattern, GD_length, GD_pattern, GTextfiles[i], outname[0], GFileIndex[i], src_offset_table, tmpfp);
						}
					}
					else {
						if (!ByteLevelIndex || RecordLevelIndex || NOBYTELEVEL) {
							if (GPRINTFILETIME) {
								SetCurrentFileTime = 1;
								CurrentFileTime = get_file_time(timesfp, NULL, GTextfiles[i], GFileIndex[i]);
							}
							first_search = 1;
							ret = fileagrep_search(AM, APattern, 1, &GTextfiles[i], 0, tmpfp);
						}
						else {
							if (my_stat(GTextfiles[i], &file_stat_buf) == -1) {
								if (GPRINTNONEXISTENTFILE) printf("%s\n", CurrentFileName);
								fclose(tmpfp);
								continue;
							}
							if (GPRINTFILETIME) {
								SetCurrentFileTime = 1;
								CurrentFileTime = get_file_time(timesfp, &file_stat_buf, GTextfiles[i], GFileIndex[i]);
							}
							if (file_stat_buf.st_mtime > index_stat_buf.st_mtime) {
								/* fprintf(stderr, "Warning: file modified after indexing: must SEARCH %s\n", CurrentFileName); */
								free_list(&src_offset_table[GFileIndex[i]]);
								first_search = 1;
								ret = fileagrep_search(AM, APattern, 1, &GTextfiles[i], 0, tmpfp);
							}
							else ret = glimpse_search(AM, APattern, GD_length, GD_pattern, GTextfiles[i], GTextfiles[i], GFileIndex[i], src_offset_table, tmpfp);
						}
					}
					SetCurrentFileName = 0;
					if (GPRINTFILETIME) SetCurrentFileTime = 0;
					fflush(tmpfp);
					fclose(tmpfp);
					tmpfp = NULL;
					if ((ret < 0) && (errno == AGREP_ERROR)) break;
#if	DEBUG
					printf("done search\n");
					fflush(stdout);
#endif	/*DEBUG*/
					FILENAMEONLY = OLDFILENAMEONLY;
					LIMITOUTPUT = OLDLIMITOUTPUT;
					LIMITPERFILE = OLDLIMITPERFILE;
					LIMITTOTALFILE = OLDLIMITTOTALFILE;
					PRINTRECORD = OLDPRINTRECORD;
					COUNT = OLDCOUNT;
					if (!UseFilters) {   /* Added to do structured queries from Webglimpse */
						ret = filter_output(GTextfiles[i], tempfile, GParse, GD_pattern, GD_length, GOUTTAIL, nullfp, StructuredIndex);
					} else {
						ret = filter_output(outname[0], tempfile, GParse, GD_pattern, GD_length, GOUTTAIL, nullfp, StructuredIndex);
					}
					gnum_of_matched += (ret > 0) ? ret : 0;
					gfiles_matched += (ret > 0) ? 1 : 0;
					if (GLIMITOUTPUT > 0) {
						if (GLIMITOUTPUT <= gnum_of_matched) break;
						LIMITOUTPUT = GLIMITOUTPUT - gnum_of_matched;
					}
					if (GLIMITTOTALFILE > 0) {
						if (GLIMITTOTALFILE <= gfiles_matched) break;
						LIMITTOTALFILE = GLIMITTOTALFILE - gfiles_matched;
					}
					if (glimpse_clientdied) break;
					fflush(stdout);
				}
			}
		}
		if (errno == AGREP_ERROR) {
			fprintf(stderr, "%s: error in options or arguments to `agrep'\n", HARVEST_PREFIX);
		}

		RETURN(gnum_of_matched);
	}
	else { /* argc > 0: simply call agrep */
#if	DEBUG
		for (i=0; i<agrep_argc; i++)
			printf("agrep_argv[%d] = %s\n", i, agrep_argv[i]);
#endif	/*DEBUG*/
		i = fileagrep(oldargc, oldargv, 0, stdout);
		RETURN(i);
	}
}
/* end of process_query() */

/*
 * Simple function to remove the non-existent files from the set of
 * files passed onto agrep for search. These are the files which got
 * DELETED after the index was built (but a fresh index was NOT built).
 * Redundant since agrep opens them anyway and stat is as bad as open.
 */
int
purge_filenames(filenames, num)
	CHAR	**filenames;
	int	num;
{
	struct stat	buf;
	int		i, j;
	int		newnum = num;
	int		ret;

	for (i=0; i<newnum; i++) {
		if (-1 == (ret = my_stat(filenames[i], &buf))) {
#if	BG_DEBUG
			fprintf(debug, "stat on %s = %d\n", filenames[i], ret);
#endif	/*BG_DEBUG*/
			my_free(filenames[i], 0);
			for (j=i; j<newnum-1; j++)
				filenames[j] = filenames[j+1];
			filenames[j] = NULL;
			newnum --;
			i--;	/* to counter the ++ on top */
		}
	}

#if	BG_DEBUG
	fprintf(debug, "Old numfiles=%d\tNew numfiles=%d\n", num, newnum);
	for (i=0; i<newnum; i++)
		fprintf(debug, "file %d = %s\n", i, filenames[i]);
#endif	/*BG_DEBUG*/
	return newnum;
}

CHAR filter_buf[BLOCKSIZE + MAXPAT*2];

/* returns #of bytes stripped off */
int getbyteoff(buf, pbyteoff)
	CHAR	*buf;
	int	*pbyteoff;
{
	CHAR	temp[32];
	int	i = 0;

	while (isdigit(*buf) && (i<32)) temp[i++] = *buf++;
	if ((*buf != '=') || (*(buf + 1) != ' ')) return -1;
	temp[i] = '\0';
	*pbyteoff = atoi(temp);
	return i+2;
}

/*
 * Filter the output in infile:
 *
 * -- get the matched line/record-s using GD_pattern, GD_length and GOUTAIL
 * -- call identify regions using matched line/record's byte offset
 * -- collect patterns corr. to that attribute into a new pattern (in split_pat itself)
 * -- see if one of them matches that line/record using memagrep
 * -- if so, output that line/record onto stdout
 */
 /* May have to change name to reflect the fact that it does booleans too */
int
filter_output(infile, outfile, GParse, GD_pattern, GD_length, GOUTTAIL, nullfp, num_attr)
	char	*infile;
	char	*outfile;
	ParseTree *GParse;
	CHAR	GD_pattern[];
	int	GD_length;
	int	GOUTTAIL;
	FILE	*nullfp;
	int	num_attr;
{
	FILE	*outfp;
	FILE	*displayfp = NULL;
	FILE	*storefp = NULL;
	int	num_read, total_read = 0;
	int	residue = 0;
	int	byteoff;
	int	attribute;
	int	i, ii;	/* i is forloop index, ii is booleaneval index */
	CHAR	*final_end;
	CHAR	*current_end;
	CHAR	*current_begin;
	CHAR	*previous_begin;
	int	skiplen;
	char	s[MAX_LINE_LEN];
	CHAR	c1, c2;
	int	printed, numprinted = 0;	/* returns number of printed records if successful in matching the pattern in the object infile */
	char	*attrname;
	int	success = 0;	/* do we print the stored output or not */
	int	count = 0;
	int	OLDLIMITOUTPUT = 0, OLDLIMITPERFILE = 0, OLDLIMITTOTALFILE = 0;

#if	BG_DEBUG
	printf("INFILE=%s\n", infile);
	printf("OUTFILE\n");
	sprintf(s, "exec cat %s\n", outfile);
	system(s);
	printf("OUTFILEDONE\n");
#endif	/*BG_DEBUG*/
	if ((outfp = fopen(outfile, "r")) == NULL) return 0;
	if (StructuredIndex && (-1 == region_create(infile))) {
		fclose(outfp);
		return 0;
	}
	if (ComplexBoolean || ((long)GParse & AND_EXP)) {
		sprintf(s, "%s/.glimpse_storeoutput.%d", TEMP_DIR, getpid());
		if ((displayfp = storefp = fopen(s, "w")) == NULL) {
			if (StructuredIndex) region_destroy();
			fclose(outfp);
			return 0;
		}
	}
	else {
		displayfp = stdout;
		/* cannot come to filter_output in this case unless -a! */
	}
	memset(matched_terminals, '\0', num_terminals);

	while ( ( (num_read = fread(filter_buf + residue, 1, BLOCKSIZE - residue, outfp)) > 0) || (residue > 0)) {
		total_read += num_read;
		if (num_read <= 0) {
			final_end = filter_buf + residue;
			num_read = residue;
			residue = 0;
		}
		else {
			num_read += residue;
			final_end = (CHAR *)backward_delimiter(filter_buf + num_read, filter_buf, GD_pattern, GD_length, GOUTTAIL);
			residue = filter_buf + num_read - final_end;
		}
#if	DEBUG
		fprintf(stderr, "filter_buf=%x final_end=%x residue=%x last_chars=%c%c%c num_read=%x\n",
			filter_buf, final_end, residue, *(final_end-2), *(final_end-1), *(final_end), num_read);
#endif	/*DEBUG*/

		current_begin = previous_begin = filter_buf;
#if	1
		current_end = (CHAR *)forward_delimiter(filter_buf, filter_buf + num_read, GD_pattern, GD_length, GOUTTAIL);	/* skip over prefixes like filename */
		if (!GOUTTAIL) current_end = (CHAR *)forward_delimiter((long)current_end + GD_length, final_end, GD_pattern, GD_length, GOUTTAIL);
#else	/*1*/
		current_end = (CHAR *)forward_delimiter(filter_buf+1, final_end, GD_pattern, GD_length, GOUTTAIL);

#endif	/*1*/

#if	DEBUG
		fprintf(stderr, "current_begin=%x current_end=%x\n", current_begin, current_end);
#endif	/*DEBUG*/

		while (current_end <= final_end) {
			previous_begin = current_begin;
			/* look for %d= */
			byteoff = -1;
			while (current_begin < current_end) {
				if (isdigit(*current_begin)) {
					skiplen = getbyteoff(current_begin, &byteoff);
#if	BG_DEBUG
					fprintf(debug, "byteoff=%d skiplen=%d\n", byteoff, skiplen);
#endif	/*BG_DEBUG*/
					if ((skiplen < 0) || (byteoff < 0)) {
						current_begin ++;
						continue;
					}
					else break;
				}
				else current_begin ++;
			}
#if	DEBUG
			printf("current_begin=%x current_end=%x final_end=%x residue=%x num_read=%x\n", current_begin, current_end, final_end, residue, num_read);
#endif	/*DEBUG*/

#if	DEBUG
			printf("byteoff=%d skiplen=%d\n", byteoff, skiplen);
#endif	/*DEBUG*/
			if ((skiplen < 0) || (byteoff < 0)) {	/* output the whole line as it is: there is nothing to strip (e.g., -l) */
#if	0
				/* This is an error: -l is now handled completely inside filter_output --> agrep won't processes it when -W */
				if (!SILENT) fwrite(previous_begin, 1, current_end-previous_begin, displayfp);
				numprinted ++;
#endif
			}
			else if ( (num_attr <= 0) || (((attribute = region_identify(byteoff, 0)) < num_attr) && (attribute >= 0)) ) {
				/* prefix is from previous_begin to current_begin. Skip skiplen from current_begin. Rest until current_end is valid output */
				if (num_attr <= 0) attribute = 0;
#if	BG_DEBUG
				fprintf(debug, "region@%d=%d\n", byteoff, attribute);
#endif	/*BG_DEBUG*/
				c1 = *(current_begin + skiplen - 1);
				c2 = *(current_end + 1);
				printed = 0;

				for (i=0; i<num_terminals; i++) {
#if	0
					printf("--> already_matched[%d] = %d, going to look at '%s'\n", i, matched_terminals[i], terminals[i].data.leaf.value);
#endif
					if (matched_terminals[i] && (GFILENAMEONLY || FILEOUT || printed || ((LIMITOUTPUT > 0) && (numprinted >= LIMITOUTPUT)) || ((LIMITPERFILE > 0) && (numprinted >= LIMITPERFILE)))) continue;
					if ((terminals[i].data.leaf.attribute == 0)  || ((int)(terminals[i].data.leaf.attribute) == attribute)) {
						*(current_begin + skiplen - 1) = '\n';
						*(current_end + 1) = '\n';
						OLDLIMITOUTPUT = LIMITOUTPUT;
						LIMITOUTPUT = 0;
						OLDLIMITPERFILE = LIMITPERFILE;
						LIMITPERFILE = 0;
						OLDLIMITTOTALFILE = LIMITTOTALFILE;
						LIMITTOTALFILE = 0;
						if (memagrep_search(	strlen(terminals[i].data.leaf.value), terminals[i].data.leaf.value,
									current_end - current_begin - skiplen + 1, current_begin + skiplen - 1,
									0, nullfp) > 0) {
							LIMITOUTPUT = OLDLIMITOUTPUT;
							LIMITPERFILE = OLDLIMITPERFILE;
							LIMITTOTALFILE = OLDLIMITTOTALFILE;

#if	0
							*(current_end + 1) = '\0';
							printf("--> search succeeded for %s in %s\n", terminals[i].data.leaf.value, previous_begin);
#endif	/*0*/
							*(current_begin + skiplen - 1) = c1;
							*(current_end + 1) = c2;
							matched_terminals[i] = 1;	/* must reevaluate/set since don't know if it should be printed */

							if (!(((LIMITOUTPUT > 0) && (numprinted >= LIMITOUTPUT)) ||
							      ((LIMITPERFILE > 0) && (numprinted >= LIMITPERFILE))) && !printed) {	/* see if it was useful later */
								if (!COUNT && !FILEOUT && !SILENT) {
								fwrite(previous_begin, 1, current_begin - previous_begin, displayfp);
								if (PRINTATTR) fprintf(displayfp, "%s# ",
									(attrname = attr_id_to_name(attribute)) == NULL ? "(null)" : attrname);
								if (GBYTECOUNT) fprintf(displayfp, "%d= ", byteoff);
								if (PRINTRECORD) {
								fwrite(current_begin + skiplen, 1, current_end - current_begin - skiplen, displayfp);
								}
								else {
									if (*(current_begin + skiplen) == '@') {
										int	iii = 0;
										while (current_begin[skiplen + iii] != '}')
											fputc(current_begin[skiplen + iii++], displayfp);
										fputc('}', displayfp);
									}
									fputc('\n', displayfp);
								}
								}
								printed = 1;
								numprinted ++;
							}
						}
						else {
							LIMITOUTPUT = OLDLIMITOUTPUT;
							LIMITPERFILE = OLDLIMITPERFILE;
							LIMITTOTALFILE = OLDLIMITTOTALFILE;
#if	0
							*(current_end + 1) = '\0';
							printf("--> search failed for %s in %s\n", terminals[i].data.leaf.value, previous_begin);
#endif	/*0*/
							*(current_begin + skiplen - 1) = c1;
							*(current_end + 1) = c2;
						}
					}
				}

				if (!success) {
					if (ComplexBoolean) {
						success = eval_tree(GParse, matched_terminals);
					}
					else {
						if ((long)GParse & AND_EXP) {
							success = 0;
							for (ii=0; ii<num_terminals; ii++) {
								if (!matched_terminals[ii]) break;
							}
							if (ii >= num_terminals) success = 1;
						}
						else {
							success = 0;
							/* cannot come to filter_output in this case unless -a! */
						}
					}
				}

				/* optimize options that do not need all the matched lines */
				if (success) {
					if (GFILENAMEONLY) {
						if (GPRINTFILETIME) {	/* from bug fix message by  Dr Jaime Prilusky lsprilus@weizmann.weizmann.ac.il jaimep@terminator.pdb.bnl.gov */
							if (!SILENT) fprintf(stdout, "%s%s\n", CurrentFileName, aprint_file_time(CurrentFileTime));
						}
						else {
							if (!SILENT) fprintf(stdout, "%s\n", CurrentFileName);
						}
						if (storefp != NULL) fclose(storefp); /* don't bother to flush! */
						storefp = NULL;
						goto unlink_and_quit;
					}
					else if (FILEOUT) {
						/* file_out(infile); */
						if (storefp != NULL) fclose(storefp); /* don't bother to flush! */
						storefp = NULL;
						goto unlink_and_quit;
					}
				}
			}
			if (success && (((LIMITOUTPUT > 0) && (numprinted >= LIMITOUTPUT)) || ((LIMITPERFILE > 0) && (numprinted >= LIMITPERFILE)))) goto double_break;
			if (glimpse_clientdied) goto double_break;
			if (current_end >= final_end) break;
			current_begin = current_end;
			if (!GOUTTAIL) current_end = (CHAR *)forward_delimiter((long)current_end + GD_length, final_end, GD_pattern, GD_length, GOUTTAIL);
			else current_end = (CHAR *)forward_delimiter(current_end, final_end, GD_pattern, GD_length, GOUTTAIL);
#if	DEBUG
		fprintf(stderr, "current_begin=%x current_end=%x\n", current_begin, current_end);
#endif	/*DEBUG*/
		}
		if (residue > 0) {
			memcpy(filter_buf, final_end, residue);
			memcpy(filter_buf+residue, GD_pattern, GD_length);
		}
	}

double_break:
	/* Come here on normal exit or when the current agrep-output is no longer of any use */
	if (!success && (total_read > 0)) {
		if (ComplexBoolean) {
			success = eval_tree(GParse, matched_terminals);
		}
		else {
			if ((long)GParse & AND_EXP) {
				success = 0;
				for (ii=0; ii<num_terminals; ii++) {
					if (!matched_terminals[ii]) break;
				}
				if (ii >= num_terminals) success = 1;
			}
			else {
				success = 0;
				/* cannot come to filter_output in this case unless -a! */
			}
		}
	}

	/* Print the temporary output onto stdout if search was successful; unlink the temprorary file */
	if (success) {
		if (GFILENAMEONLY) {	/* all other output options are useless since they all deal with the MATCHED line */
			if (GPRINTFILETIME) {	/* from bug fix message by  Dr Jaime Prilusky lsprilus@weizmann.weizmann.ac.il jaimep@terminator.pdb.bnl.gov */
				if (!SILENT) fprintf(stdout, "%s%s\n", CurrentFileName, aprint_file_time(CurrentFileTime));
			}
			else {
				if (!SILENT) fprintf(stdout, "%s\n", CurrentFileName);
			}
			if (!SILENT) fprintf(stdout, "%s\n", CurrentFileName);
			if (storefp != NULL) fclose(storefp); /* don't bother to flush! */
			storefp = NULL;
		}
		else if (COUNT && !FILEOUT) {
			if (!SILENT) {
				if(!NOFILENAME) fprintf(stdout, "%s: %d\n", CurrentFileName, numprinted);
				else fprintf(stdout, "%d\n", numprinted);
			}
			if (storefp != NULL) fclose(storefp); /* don't bother to flush! */
			storefp = NULL;
		}
		else if (FILEOUT) {
			/* file_out(infile); */
			if (storefp != NULL) fclose(storefp); /* don't bother to flush! */
			storefp = NULL;
		}
		else if (storefp != NULL) {
			fflush(storefp);
			fclose(storefp);
#if	DEBUG
			printf("STOREOUTPUT\n");
			sprintf(s, "exec cat %s/.glimpse_storeoutput.%d\n", TEMP_DIR, getpid());
			system(s);
#endif	/*DEBUG*/
			sprintf(s, "%s/.glimpse_storeoutput.%d", TEMP_DIR, getpid());
			if ((storefp = fopen(s, "r")) != NULL) {
				if (!SILENT) while (fgets(s, MAX_LINE_LEN, storefp) != NULL) fputs(s, stdout);
				fclose(storefp);
			}
			storefp = NULL;
		}
	}
	else {
		if (storefp != NULL) fclose(storefp); /* else don't bother to flush */
	}

unlink_and_quit:
	sprintf(s, "%s/.glimpse_storeoutput.%d", TEMP_DIR, getpid());
	unlink(s);

	if (StructuredIndex) region_destroy();
	fclose(outfp);

	if (GFILENAMEONLY) {
		if (numprinted > 0) return 1;
		else return 0;
	}
	else if (ComplexBoolean || ((long)GParse & AND_EXP)) {
		if (success) return numprinted;
		else return 0;
	} else {	/* must be -a */
		return numprinted;
	}
}

usage()
{
	fprintf(stderr, "\nThis is glimpse version %s, %s.\n\n", GLIMPSE_VERSION, GLIMPSE_DATE);
	fprintf(stderr, "usage:  %s [-#abceghijklnprstwxyBCDEGIMNPQSVWZ] [-d DEL] [-f FILE] [-F PAT] [-H DIR] [-J HOST] [-K PORT] [-L X[:Y:Z]] [-R X] [-T DIR] [-Y D] pattern [files]", GProgname);
	fprintf(stderr, "\n");
	fprintf(stderr, "List of options (see %s for more details):\n", GLIMPSE_URL);
	fprintf(stderr, "\n");

	fprintf(stderr, "-#: find matches with at most # errors\n");
	fprintf(stderr, "-a: print attribute names (useful only for Harvest SOIF format)\n");
	fprintf(stderr, "-b: print the byte offset of the record from the beginning of the file\n");
	fprintf(stderr, "-B: best match mode: find the closest matches to the pattern\n");
	fprintf(stderr, "-c: output the number of matched records\n");
	fprintf(stderr, "-C: send queries to glimpseserver\n");
	fprintf(stderr, "-d DEL: define record delimiter DEL\n");
	fprintf(stderr, "-D x: adjust the cost of deletions to x\n");
	fprintf(stderr, "-e: for patterns starting with -\n");
	fprintf(stderr, "-E: print matching lines as they appear in the index (useful in -EQNg)\n");
	fprintf(stderr, "-f FILE: restrict the search to files whose names appear in FILE\n");
	fprintf(stderr, "-F PAT: restrict the search to files matching PAT\n");
	fprintf(stderr, "-g: print the file number (in the index)\n");
	fprintf(stderr, "-G: output the (whole) files that contain a match\n");
	fprintf(stderr, "-h: do not output file names before matched record\n");
	fprintf(stderr, "-H DIR: the glimpse index is located in directory DIR\n");
	fprintf(stderr, "-i: case-insensitive search, e.g., 'a' = 'A'\n");
	fprintf(stderr, "-I x: adjust the cost of insertions to x\n");
	fprintf(stderr, "-j: output file modification dates (if -t was used for the indexing)\n");
	fprintf(stderr, "-J HOST: send queries to glimpseserver at HOST\n");
	fprintf(stderr, "-k: use the pattern as is (no meta characters)\n");
	fprintf(stderr, "-K PORT: send queries to glimpseserver at TCP port number PORT\n");
	fprintf(stderr, "-l: output only the names of files that contain a match\n");
	fprintf(stderr, "-L X[:Y:Z]: limit the output to X records [Y files, Z matches per file]\n");
	fprintf(stderr, "-n: output record prefixed by record number\n");
	fprintf(stderr, "-N: search only the index (may not be precise for some queries) \n");
	fprintf(stderr, "-o: delimiter is output at the beginning of the matched record\n");
	fprintf(stderr, "-O: file names are printed only once per file\n");
	fprintf(stderr, "-p FILE:off:len:endian: restrict the search to the files whose names\n\tare specified as a bit-field OR sparse-set in FILE\n");
	fprintf(stderr, "-P: print the pattern that matched before the matched record\n");
	fprintf(stderr, "-q: print the offsets of the beginning and end of each matched record\n");
	fprintf(stderr, "-Q: (with -N) print offsets of matches from (only the large) index\n");
	fprintf(stderr, "-r: (used only for agrep) - recursive search\n");
	fprintf(stderr, "-R X: set the maximum size of a record to X\n");
	fprintf(stderr, "-s: display nothing except error messages\n");
	fprintf(stderr, "-S x: adjust the cost of substitutions to x\n");
	fprintf(stderr, "-t: use in combination with -d DEL so that the delimiter DEL appears\n\tat the end of each output record instead of the beginning\n");
	fprintf(stderr, "-T DIR: temporary files are put in directory DIR (instead of /tmp)\n");
	fprintf(stderr, "-u: do not output matched records (useful in -qbug)\n");
	fprintf(stderr, "-U: interpret .glimpse_filenames when -U / -X option is used in glimpseindex\n");
	fprintf(stderr, "-v: (works ONLY for agrep) - output all records that do not contain a match\n");
	fprintf(stderr, "-V,--version: print the current version of glimpse\n");
	fprintf(stderr, "-w: pattern has to match as a word, e.g., 'win' will not match 'wind'\n");
	fprintf(stderr, "-W: the scope of Booleans is the whole file (except for structured queries)\n");
	fprintf(stderr, "-x: the pattern must match the whole line\n");
	fprintf(stderr, "-X: if an indexed file that matches 'pattern' doesn't exist, print its name\n");
	fprintf(stderr, "-y: no prompt\n");
	fprintf(stderr, "-Y D: output only matches in files that were updated in the last D days\n");
	fprintf(stderr, "-z: customizable filtering using the .glimpse_filters file\n");
	fprintf(stderr, "-Z: no op\n");
	fprintf(stderr, "--help: this message\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "For questions about glimpse, please contact: `%s'\n", GLIMPSE_EMAIL);

	return -1;	/* useful if we make glimpse into a library */

/*
 * Undocumented Option Combinations for SFS (like RPC calls)
 *	print file number of match instead of file name: -g
 *	print enclosing offsets of matched record: -q
 *	NOT print matched record: -u
 * E.G. USAGE: -qbug (b prints offset of pattern: can also use -lg or -Ng)
 *	look only at index: -E
 *	look at matched offsets in files as seen in index (w/o searching): -QN
 * E.G. USAGE: -EQNgy
 *	read the -EQNg or just -QNg output from stdin and perform actual search w/o
 *	searching the index (take hints from user): -U
 * NOTE: can't use U unless QNg are all used together (e.g., BEGIN/END won't be printed)
 */
}

usageS()
{
	fprintf(stderr, "\nThis is glimpse server version %s, %s.\n\n", GLIMPSE_VERSION, GLIMPSE_DATE);
	fprintf(stderr, "usage:  %s [-H DIR] [-J HOST] [-K PORT]", GProgname);
	fprintf(stderr, "\n");
	fprintf(stderr, "-H DIR: the glimpse index is located in directory DIR\n");
	fprintf(stderr, "-J HOST: the host name (string) clients must use / server runs on\n");
	fprintf(stderr, "-K PORT: the port (short integer) clients must use / server runs on\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "For questions about glimpse, please contact `%s'\n", GLIMPSE_EMAIL);

	return -1;	/* useful if we make glimpse into a library */
}
 
#if	CLIENTSERVER
/*
 *  do_select() - based on select_loop() from the Harvest Broker.
 *  -- Courtesy: Darren Hardy, hardy@cs.colorado.edu
 */
int do_select(sock, sec)
int sock;		/* the socket to wait for */
int sec;		/* the number of seconds to wait */
{
	struct timeval to;
	fd_set qready;
	int err;

	if (sock < 0 || sec < 0)
		return 0;

	FD_ZERO(&qready);
	FD_SET(sock, &qready);
	to.tv_sec = sec;
	to.tv_usec = 0;
	if ((err = select(sock + 1, &qready, NULL, NULL, &to)) < 0) {
		if (errno == EINTR)
			return 0;
		perror("select");
		return -1;
	}
	if (err == 0)
		return 0;

	/* If there's someone waiting to get it, let them through */
	return (FD_ISSET(sock, &qready) ? 1 : 0);
}
#endif	/* CLIENTSERVER */
