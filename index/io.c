/* Copyright (c) 1994 Sun Wu, Udi Manber, Burra Gopal.  All Rights Reserved. */
/* ./glimpse/index/io.c */
#include "glimpse.h"
#include <autoconf.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <dlfcn.h>
extern char INDEX_DIR[MAX_LINE_LEN];
extern int memory_usage;

#include "utils.c"

int	REAL_INDEX_BUF = DEF_REAL_INDEX_BUF,
	MAX_ALL_INDEX = DEF_MAX_ALL_INDEX,
	FILEMASK_SIZE = DEF_FILEMASK_SIZE,
	REAL_PARTITION = DEF_REAL_PARTITION;

/* Escapes single quotes in "original" string with backquote (\) s.t. it can be passed on to the shell as a file name: returns its second argument for printf */
/* Called before passing any argument to the system() routine in glimpse or glimspeindex source code */
/* Works only if the new name is going to be passed as argument to the shell within two ''s */
char *
escapesinglequote(original, new)
	char	*original, *new;
{
	char	*oldnew = new;
	while (*original != '\0') {
		if (*original == '\'') {
			*new ++ = '\'';	/* close existing ' : this guy will be a part of a file name starting from a ' */
			*new ++ = '\\';	/* add escape character */
			*new ++ = '\'';	/* add single quote from original here */
		}
		*new ++ = *original ++; /* start the real single quote to continute existing file name if *original was ' */
	}
	*new = *original;
	return oldnew;
}

/* --------------------------------------------------------------------
get_array_of_lines()
input: an input filename, address of the table, maximum number of entries
of the table, and a overflow handling flag.
output: a set of strings in the table.
when overflow is ON, the function returns after the table is filled.
otherwise the function will exit if overflow occurs.
In normal return, the function returns the number of entries read.
----------------------------------------------------------------------*/
get_array_of_lines(inputfile, table, max_entry, overflow_ok)
char *inputfile;
char **table[];
int  max_entry;  /* max number of entries in the table */
int  overflow_ok;   /* flag for handling overflow */
{
	int  tx=0;    /* index for table */
	FILE *file_in;
	unsigned char buffer[MAX_NAME_BUF];
	char *np; 	
	int  line_length;
	int  num_lines;

	if((file_in = fopen(inputfile, "r")) == NULL) {
		if (overflow_ok) return 0;
		fprintf(stderr, "can't open for reading: %s\n", inputfile);
		exit(2);
	}
	fgets(buffer, MAX_NAME_BUF, file_in);
	sscanf(buffer, "%d", &num_lines);
	if ((num_lines < 0) || (num_lines > MaxNum24bPartition)) {
		fclose(file_in);
		if (overflow_ok) return 0;
		fprintf(stderr, "Error in reading: %s\n", inputfile);
		exit(2);
	}

	while(fgets(buffer, MAX_NAME_BUF, file_in)) {
		line_length = strlen(buffer);
		if (line_length == 1) continue;
		buffer[line_length-1] = '\0';  /* discard the '\n' */
#if	BG_DEBUG
		np = (char *) my_malloc(sizeof(char) * (line_length + 2));
#else	/*BG_DEBUG*/
		np = (char *) my_malloc(sizeof(char) * (line_length + 2));
#endif	/*BG_DEBUG*/
		if(np == NULL) {
		    int	i=0;

		    fclose(file_in);
		    for (i=0; i<tx; i++) {
#if	BG_DEBUG
			memory_usage -= (strlen(LIST_GET(table, i)) + 2);
#endif	/*BG_DEBUG*/
			if (LIST_GET(table, i) != NULL) {
				my_free(LIST_GET(table, i), 0);
				LIST_SUREGET(table, i) = NULL;
			}
		    }
		    if (overflow_ok) {
			fclose(file_in);
			return 0;
		    }
		    fprintf(stderr, "malloc failure in get_array_of_lines\n");
		    exit(2);
		}
		LIST_ADD(table, tx, np, char*);
		tx ++;
		/* table[tx++] = (unsigned char *)np; */
		strcpy(np, buffer);
		if(tx > max_entry) {
		    fclose(file_in);
		    if(overflow_ok) {
			fclose(file_in);
			return(tx);
		    }
		    fprintf(stderr, "overflow in get_array_of_lines()\n");
		    exit(2);
		}
	}
	fclose(file_in);
	return(tx);   /* return number of lines read */
}

/* --------------------------------------------------------------------
get_table():
input: an input filename, address of the table, maximum number of entries
of the table, and a overflow handling flag.
output: a set of integers in the table.
when overflow_ok is ON, the function returns after the table is filled.
otherwise the function will exit if overflow occurs.
In normal return, the function returns the number of entries read.
----------------------------------------------------------------------*/
int get_table(inputfile, table, max_entry, overflow_ok)
char *inputfile;
int  table[];
int  max_entry;
int  overflow_ok;
{
	int  val = 0;
	int  c = 0;
	FILE *file_in;
	int  tx=0;           /* number of entries read */

	if((file_in = fopen(inputfile, "r")) == NULL) {
		if (overflow_ok) return 0;
		fprintf(stderr, "can't open %s for reading\n", inputfile);
		exit(2);
	}
	while((c = getc(file_in)) != EOF) {
		val = c << 24;
		if ((c = getc(file_in)) == EOF) break;
		val |= c << 16;
		if ((c = getc(file_in)) == EOF) break;
		val |= c << 8;
		if ((c = getc(file_in)) == EOF) break;
		val |= c;

		if(tx > max_entry) {
			if(!overflow_ok) {
			    fprintf(stderr, "in get_table: table overflow\n");
			    exit(2);
			}
			break;
		}
		table[tx++] = val;
	}
	fclose(file_in);
	return(tx);
}

get_index_type(s, dashn, num, attr, delim)
char s[];
int *dashn, *num, *attr;
char delim[];
{
	FILE *fp = fopen(s, "r");
	char buf[MAX_LINE_LEN];

	*dashn = *num = *attr = 0;
	*delim = '\0';
	if (fp == NULL) return 0;
	fscanf(fp, "%s\n%%%d\n%%%d%s\n", buf, num, attr, delim);
	/* printf("get_index_type(): %s %d %d %s\n", buf, num, attr, delim); */
	fclose(fp);
	if (strstr(buf, "1234567890")) *dashn = ON;
	return *num;
}

/* Read offset from srcbuf first so that you can use it with srcbuf=destbuf */
get_block_numbers(srcbuf, destbuf, partfp)
	unsigned char *srcbuf, *destbuf;
	FILE *partfp;
{
	int	offset, pat_size;
	static int printederror = 0;

	/* Does not do caching of blocks seen so far: done in OS hopefully */
	offset = (srcbuf[0] << 24) |
		(srcbuf[1] << 16) |
		(srcbuf[2] << 8) |
		(srcbuf[3]);
	pat_size = decode32b(offset);

	if (-1 == fseek(partfp, pat_size, 0)) {
		if (!printederror) {
			fprintf(stderr, "Warning! Error in the format of the index!\n");
			printederror = 1;
		}
	}
	destbuf[0] = '\n';
	destbuf[1] = '\0';
	destbuf[2] = '\0';
	destbuf[3] = '\0';
	if (fgets(destbuf, REAL_INDEX_BUF - MAX_WORD_BUF - 1, partfp) == NULL) {
		destbuf[0] = '\n';
		destbuf[1] = '\0';
		destbuf[2] = '\0';
		destbuf[3] = '\0';
	}
}

int num_filter=0;
int filter_len[MAX_FILTER];
CHAR *filter[MAX_FILTER];
CHAR *filter_command[MAX_FILTER];
struct stat filstbuf;



/* Prototype for filter entry point in a shared library -- CV 9/14/99 */
typedef int (*FILTER_FUNC)(FILE *, FILE *);

/* Holds addresses of entry points -- CV 9/14/99 */
static FILTER_FUNC filter_func[MAX_FILTER];

/* Holds shared library handles, one per filter and shared library 
   -- CV 9/14/99 */
static void *filter_handle[MAX_FILTER];


/* Loads shared filter libraries. The criterion, upon which this
   function decides whether a filter is an external program or a shared
   library is as follows: If the name of the filter command ends in ".so",
   it assumes that the filter is a shared library; otherwise it assumes
   an external program.
 
   apply_filters() finds out what kind the ith filter is used by
   looking at filter_func[i]. If it is non-null, it is a pointer to
   the entry point in a shared library. Otherwise, the filter is an
   external program. -- CV 9/14/99
*/

#ifndef	RTLD_NOW
/* dummy function for old system such as SunOS-4.1.3 */
static void load_dyn_filters(void) {}
#else
static void load_dyn_filters(void)
{
    int i, len, success;
    char *so_pos;
    char *error;

    memset(filter_func, '\0', sizeof (filter_func));
    memset(filter_handle, '\0', sizeof (filter_handle));

    for (i = 0; i < num_filter; i++) {
	success = 1;
	len = strlen(filter_command[i]);
	if (len > 4) {
	    /* find location in string where .so suffix should be */
	    so_pos = (char *)(filter_command[i] + len - 3);
	    if (strcmp(so_pos, ".so") == 0) {
		/* fprintf(stderr, "Loading %s\n", filter_command[i]); */
		if ((filter_handle[i] = dlopen(filter_command[i],
					       RTLD_NOW)) == NULL) {
		    success = 0;
		    error = dlerror();
		}
		if (success) {
		    filter_func[i] = dlsym(filter_handle[i], "filter_func");
		    if ((error = dlerror()) != NULL)
			success = 0;
		}
		if (! success) {
		    fputs("Warning: Error while loading filter\n", stderr);
		    fputs(error, stderr); 
		    if (filter_handle[i] != NULL) {
                       /* lib was already loaded  when error happened */
			dlclose(filter_handle[i]);
		    } 
		    /* Since an error occurred, should we disable the
                       filter entirely for this command?  If yes, HOW? -- CV */
		}
	    }
	}
    }
}
#endif	/* RTLD_NOW */


/* Applies a filter to a single file, based on whether the filter is a
   command or in a shared library.

   filter_number is the filter's index in the filter table.
   Returns 0 on success, passes on error code from the filter otherwise
   -- CV 9/14/99
 */

static int apply_one_filter(int filter_number, const char *in_name,
		     const char *out_name)
{
    int ret = 0;

    if (filter_func[filter_number] != NULL) { /* in shared library */
	FILE *in = NULL, *out = NULL;
	in = fopen(in_name, "r");
	if (in != NULL) out = fopen(out_name, "w"); 

	if (in == NULL || out == NULL) ret = errno;
	else ret = (*filter_func[filter_number])(in, out);

	if (out != NULL) fclose(out);
	if (in != NULL) fclose(in);
    } 
    else { /* in external program */
	char escaped_in[MAX_LINE_LEN], escaped_out[MAX_LINE_LEN];	
	char command[2 * MAX_LINE_LEN];
	escapesinglequote(in_name, escaped_in);
	escapesinglequote(out_name, escaped_out);
	sprintf(command, "exec %s '%s' > '%s'", filter_command[filter_number],
		escaped_in, escaped_out);
	/* FIXME: use snprintf() where available to avoid stupid
	   buffer overruns. But security risk should be low, because
	   no user-supplied data goes in here. -- CV 9/14/99. */

	ret = system(command);
    }
    return ret;
}


/* Copies a file verbatim. It could be a little better optimized by
   using fread() and fwrite(), but speed is not critical here.
   Returns 0 on success, error code otherwise.
   -- CV 9/14/99
*/

static int copy_file(const char *source, const char *destination) {
    FILE *src = NULL, *dest = NULL;
    int c, ret = 0;

    src = fopen(source, "r");
    if (src != NULL) dest = fopen(destination, "w");
    if (src == NULL || dest == NULL) 
	ret = errno;
    else {
	/* FIXME: Better error checking here. But it's not really critical,
	   because these are only temporary files.  -- CV 9/14/99 */
	while ((c = fgetc(src)) != EOF)
	    fputc(c, dest);
    }

    if (dest != NULL) fclose(dest);
    if (src != NULL) fclose(src);
    return ret;
}




read_filters(index_dir, dofilter)
char	*index_dir;
int	dofilter;
{
    int len;
    int patlen;
    int patpos;
    int commandpos;
    FILE *filterfile;
    char filterbuf[MAX_LINE_LEN];
    char tempbuf[MAX_LINE_LEN];
    char s[MAX_LINE_LEN];

    num_filter = 0;
    memset(filter, '\0', sizeof(CHAR *) * MAX_FILTER);
    memset(filter_command, '\0', sizeof(CHAR *) * MAX_FILTER);
    memset(filter_len, '\0', sizeof(int) * MAX_FILTER);

    if (!dofilter) return;
    sprintf(s, "%s/%s", index_dir, FILTER_FILE);
    filterfile = fopen(s, "r");
    if(filterfile == NULL) {
	/* fprintf(stderr, "can't open filter file %s\n", s); -- no need */
	num_filter = 0;
    }
    else if (fstat(fileno(filterfile), &filstbuf) == -1) {
	num_filter = 0;
    }
    else {
	while((num_filter < MAX_FILTER) && fgets(filterbuf, MAX_LINE_LEN, filterfile)) {
		if ((len = strlen(filterbuf)) < 1) continue;
		filterbuf[len-1] = '\0';
		commandpos = 0;

		while ((commandpos < len) && ((filterbuf[commandpos] == ' ') || (filterbuf[commandpos] == '\t'))) commandpos ++;	/* leading spaces */
		if (commandpos >= len) continue;
		if (filterbuf[commandpos] == '\'') {
			commandpos ++;
			patpos = commandpos;
			patlen = 0;
			while (commandpos < len) {
				if (filterbuf[commandpos] == '\\') {
					commandpos += 2;
					patlen += 2;
				}
				else if (filterbuf[commandpos] != '\'') {
					commandpos ++;
					patlen ++;
				}
				else break;
			}
			if ((commandpos >= len) || (patlen <= 0)) continue;
			commandpos ++;
		}
		else {
			patpos = commandpos;
			patlen = 0;
			while ((commandpos < len) && (filterbuf[commandpos] != ' ') && (filterbuf[commandpos] != '\t')) {
				commandpos ++;
				patlen ++;
			}
			while ((commandpos < len) && ((filterbuf[commandpos] == ' ') || (filterbuf[commandpos] == '\t'))) commandpos ++;
			if (commandpos >= len) continue;
		}

		memcpy(tempbuf, &filterbuf[patpos], patlen);
		tempbuf[patlen] = '\0';
		if ((filter_len[num_filter] = convert2agrepregexp(tempbuf, patlen)) == 0) continue;	/* inplace conversion */
		filter[num_filter] = (unsigned char *) strdup(tempbuf);
		filter_command[num_filter] = (unsigned char *)strdup(&filterbuf[commandpos]);
		num_filter ++;
	}
	fclose(filterfile);
    }

    load_dyn_filters(); /* load filters in shared libraries -- CV 9/14/99 */
}

/* 1 if filter application was successful and the output (>1B) is in outname, 2 if some pattern matched but there is no output, 0 otherwise: sep 15-18 '94 */
/* memagrep is initialized in partition.c for calls from dir.c, and it is already done by the time we call this function from main.c */
apply_filter(inname, outname)
	char	*inname, *outname;	/* outname is in-out, inname is in */
{
	int	i;
	char	name[MAX_LINE_LEN], es1[MAX_LINE_LEN], es2[MAX_LINE_LEN];
	int	name_len = strlen(inname);
	char	s[MAX_LINE_LEN];
	FILE	*dummyout;
	FILE	*dummyin;
	char	dummybuf[4];
	char	prevoutname[MAX_LINE_LEN];
	char	newoutname[MAX_LINE_LEN];
	char	tempoutname[MAX_LINE_LEN];
	char	tempinname[MAX_LINE_LEN];
	int	ret = 0;
	int	unlink_prevoutname = 0;

	if (num_filter <= 0) return 0;
	if ((dummyout = fopen("/dev/null", "w")) == NULL) return 0;
	/* ready for memgrep */
	name[0] = '\n';
	special_get_name(inname, name_len, tempinname);
	name_len = strlen(tempinname);
	strcpy(name+1, tempinname);
	strcpy(prevoutname, tempinname);
	strcpy(newoutname, outname);

	/* Current properly filtered output is always in prevoutname */
	for(i=0; i<num_filter; i++) {
		if (filter_len[i] > 0) {
			char *suffix;
			name[name_len + 1] = '\0';
			if ((suffix = strstr(name+1, filter[i])) != NULL) {	/* Chris Dalton */
				if (ret == 0) ret = 2;
				/* yes, it matched: now apply the command and get the output */
				/* printf("filtering %s\n", name); */
				/* new filter function -- CV 9/14/99 */
				apply_one_filter(i, prevoutname, newoutname);

				if (((dummyin = my_fopen(newoutname, "r")) == NULL) || (fread(dummybuf, 1, 1, dummyin) <= 0)) {
					if (dummyin != NULL) fclose(dummyin);
					unlink(newoutname);
					continue;
				}
				/* Filter was successful: output exists and has atleast 1 byte in it */
				fclose(dummyin);
				if (unlink_prevoutname) {
					unlink(prevoutname);
					strcpy(tempoutname, prevoutname);
					strcpy(prevoutname, newoutname);
					strcpy(newoutname, tempoutname);
				}
				else {
					strcpy(prevoutname, newoutname);
					sprintf(newoutname, "%s.o", prevoutname);
				}
				ret = 1;
				unlink_prevoutname = 1;
#if	1
				/* if the matched text was a proper suffix of the name, */
				/* remove the suffix just processed before examining the */
				/* name again. Chris Dalton */
				/* And I don't know what the equivalent thing is with */
				/* memagrep_search: since it doesn't return a pointer to */
				/* the place where the match occured. Burra Gopal */
				if (strcmp(filter[i], suffix) == 0) {
					name_len -= strlen(suffix);
					*suffix= '\0';
				}
#endif	/*1*/
				if (strlen(newoutname) >= MAX_LINE_LEN - 1) break;
			}
		}
		else {	/* must call memagrep */
			name[name_len + 1] = '\n';	/* memagrep wants names to end with '\n': '\0' is not necessary */
			/* printf("i=%d filterlen=%d filter=%s inlen=%d input=%s\n", i, -filter_len[i], filter[i], len_current_dir_buf, current_dir_buf); */
			if (((filter_len[i] == -2) && (filter[i][0] == '.') && (filter[i][1] == '*')) ||
			    (memagrep_search(-filter_len[i], filter[i], name_len + 2, name, 0, dummyout) > 0)) {
				if (ret == 0) ret = 2;
				/* yes, it matched: now apply the command and get the output */
				/* printf("filtering %s\n", name); */
				/* new filter function -- CV 9/14/99 */
 				apply_one_filter(i, prevoutname, newoutname);

				if (((dummyin = my_fopen(newoutname, "r")) == NULL) || (fread(dummybuf, 1, 1, dummyin) <= 0)) {
					if (dummyin != NULL) fclose(dummyin);
					unlink(newoutname);
					continue;
				}
				/* Filter was successful: output exists and has atleast 1 byte in it */
				fclose(dummyin);
				if (unlink_prevoutname) {
					unlink(prevoutname);
					strcpy(tempoutname, prevoutname);
					strcpy(prevoutname, newoutname);
					strcpy(newoutname, tempoutname);
				}
				else {
					strcpy(prevoutname, newoutname);
					sprintf(newoutname, "%s.o", prevoutname);
				}
		  		ret = 1;
				unlink_prevoutname = 1;
				if (strlen(newoutname) >= MAX_LINE_LEN - 1) break;
			}
		}
	}
	if (ret == 1) strcpy(outname, prevoutname);
	else {	/* dummy filter that copies input to output: caller can use tempinname but this has easy interface */
	    /* replaced system() call with a simple copy function. -- CV 9/14/99 */
	    copy_file(tempinname, outname);
	}
	fclose(dummyout);
	return ret;
}

/* Use a modified wais stoplist to do this with simple strcmp's in a for loop */
static_stop_list(word)
	char	*word;
{
	return 0;
}

/* This is the stuff that used to be present in the old build_in.c */

/* Some variables used throughout */
FILE *TIMEFILE;		/* file descriptor for sorting .glimpse_filenames by time */
#if	BG_DEBUG
FILE  *LOGFILE; 	/* file descriptor for LOG output */
#endif	/*BG_DEBUG*/
FILE  *STATFILE;	/* file descriptor for statistical data about indexed files */
FILE  *MESSAGEFILE;	/* file descriptor for important messages meant for the user */
char  INDEX_DIR[MAX_LINE_LEN];
char  sync_path[MAX_LINE_LEN];
struct stat istbuf;
struct stat excstbuf;
struct stat incstbuf;

int ICurrentFileOffset;
int NextICurrentFileOffset;

/* Some options used throughout */
int GenerateHash = OFF;
int KeepFilenames = OFF;
int OneFilePerBlock = OFF;
int total_size = 0;
int total_deleted = 0;
int MAXWORDSPERFILE = 0;
int NUMERICWORDPERCENT = DEF_NUMERIC_WORD_PERCENT;
int AddToIndex = OFF;
int DeleteFromIndex = OFF;
int PurgeIndex = ON;
int FastIndex = OFF;
int BuildDictionary = OFF;
int BuildDictionaryExisting = OFF;
int CompressAfterBuild = OFF;
int IncludeHigherPriority = OFF;
int FilenamesOnStdin = OFF;
int ExtractInfo = OFF;
int InfoAfterFilename = OFF;
int FirstWordOfInfoIsKey = OFF;
int UseFilters = OFF;
int ByteLevelIndex = OFF;
int RecordLevelIndex = OFF;	/* When we want a -o like index but want to do booleans on a per-record basis directly from index: robint@zedcor.com */
				/* This type of index doesn't make sense with attributes since they span > 1 record; hence StructuredIndex == -2 => this = ON */
int StoreByteOffset = OFF;	/* In RecordLevelIndex, store record # for each word or byte offset of the record: record # is the default (12/12/96) */
char rdelim[MAX_LINE_LEN];
char old_rdelim[MAX_LINE_LEN];
int rdelim_len = 0;
/* int IndexUnderscore = OFF; */
int IndexableFile = OFF;
int MAX_INDEX_PERCENT = DEF_MAX_INDEX_PERCENT;
int MAX_PER_MB = DEF_MAX_PER_MB;
int I_THRESHOLD = DEF_I_THRESHOLD;
int BigHashTable = OFF;
int IndexEverything = OFF;
int HashTableSize = MAX_64K_HASH;
int BuildTurbo = OFF;
int SortByTime = OFF;

int AddedMaxWordsMessage = OFF;
int AddedMixedWordsMessage = OFF;

int  icount=0; /* count the number of my_malloc for indices structure */
int  hash_icount=0; /* to see how much was added to the current hash table */
int  save_icount=0; /* to see how much was added to the index by the current file */
int  numeric_icount=0; /* to see how many numeric words were there in the current file */

int mask_int[32] = MASK_INT;
int p_table[MAX_PARTITION];
int memory_usage = 0;

char *
my_malloc(len)
    int len;
{
    char *s;
    static int i=100;

    if ((s = malloc(len)) != NULL) memory_usage += len;
    else fprintf(stderr, "malloc failed after memory_usage = %x Bytes\n", memory_usage);
    /* Don't exit since might do traverse here: exit in glimpse though */
#if	BG_DEBUG
    printf("m:%x ", memory_usage);
    i--;
    if (i==0) {
	printf("\n");
	i = 100;
    }
#endif	/*BG_DEBUG*/
    return s;
}

my_free(ptr, size)
	void *ptr;
	int size;
{
    if (ptr) free(ptr);
    memory_usage -= size;
#if	BG_DEBUG
    printf("f:%x ", memory_usage);
#endif	/*BG_DEBUG*/
}

int file_num = 0;
int old_file_num = 0;	/* upto what file number should disable list be accessed: < file_num if incremental indexing */
int new_file_num = -1;	/* after purging index, how many files are left: for save_data_structures() */

int  bp=0;                          /* buffer pointer */
unsigned char word[MAX_WORD_BUF];
int FirstTraverse1 = ON;

struct  indices *ip;

/* Globals used in merge, and also in glimpse's get_index.c */
unsigned int *src_index_set = NULL;
unsigned int *dest_index_set = NULL;
unsigned char *src_index_buf = NULL;
unsigned char *dest_index_buf = NULL;
unsigned char *merge_index_buf = NULL;

/*
 * Routines for zonal memory allocation for glimpseindex and very fast search in glimpse.
 */

int next_free_token = 0;
struct token *free_token = NULL; /*[I_THRESHOLD/AVG_OCCURRENCES]; */

int next_free_indices = 0;
struct indices *free_indices = NULL; /*[I_THRESHOLD]; */

int next_free_word = 0;
char *free_word = NULL; /*[I_THRESHOLD/AVG_OCCURRENCES * AVG_WORD_LEN]; */

extern int usemalloc;

/*
 * The beauty of this allocation scheme is that "free" does not need to be implemented!
 */

tokenallfree()
{
	next_free_token = 0;
}

tokenfree(e, len)
struct token *e;
int len;
{
	if (usemalloc) my_free(e, sizeof(struct token));
}

struct token *
tokenalloc(len)
int	len;
{
	struct token *e;

	if (usemalloc) (e) = (struct token *)my_malloc(sizeof(struct token));
	else {
		if (free_token == NULL) free_token = (struct token *)my_malloc(sizeof(struct token) * I_THRESHOLD / INDICES_PER_TOKEN);
		if (free_token == NULL) {fprintf(stderr, "malloc failure in tokenalloc()\n"); exit(2);}
		else (e) = ((next_free_token >= I_THRESHOLD / INDICES_PER_TOKEN) ? (NULL) : (&(free_token[next_free_token ++])));
	}
	return e;
}

indicesallfree()
{
	next_free_indices = 0;
}

indicesfree(e, len)
struct indices *e;
int len;
{
	if (usemalloc) my_free(e, sizeof(struct indices));
}

struct indices *
indicesalloc(len)
int	len;
{
	struct indices *e;

	if (usemalloc) (e) = (struct indices *)my_malloc(sizeof(struct indices));
	else {
		if (free_indices == NULL) free_indices = (struct indices *)my_malloc(sizeof(struct indices) * I_THRESHOLD);
		if (free_indices == NULL) {fprintf(stderr, "malloc failure in indicesalloc()\n"); exit(2);}
		else (e) = ((next_free_indices >= I_THRESHOLD) ? (NULL) : (&(free_indices[next_free_indices ++])));
	}
	return e;
}

/* For words in a token structure */
wordallfree()
{
	next_free_word = 0;
}

wordfree(s, len)
char *s;
int len;
{
	if (usemalloc) my_free(s, len);
}

char *
wordalloc(len)
int	len;
{
	char *s;

	if (usemalloc) (s) = (char *)my_malloc(len);
	else {
		if (free_word == NULL) free_word = (char *)my_malloc(AVG_WORD_LEN * I_THRESHOLD/INDICES_PER_TOKEN);
		if (free_word == NULL) {fprintf(stderr, "malloc failure in wordalloc()\n"); exit(2); }
		else (s) = ((next_free_word + len + 2 >= AVG_WORD_LEN * I_THRESHOLD/INDICES_PER_TOKEN) ? (NULL) : (&(free_word[next_free_word])));
		if (s != NULL) next_free_word += (len);
		/* 2 for 1 char word with '\0' */
	}
	return s;
}

struct mini *mini_array = NULL;
int mini_array_len = 0;

#if	WORD_SORTED
/*
 * Routines that operate on the index using the mini-index.
 *
 * The index is a list of words+delim+attr+offset+\n sorted
 * by the word (using strcmp).
 *
 * The mini-index keeps track of the offsets in the index
 * where every WORDS_PER_REGION-th word in the index occurs.
 * There is no direct way for glimpse to seek into the mini
 * file for the exact offset of this word since unlike hash
 * values words are of variable length.
 *
 * This is small enough to be kept in memory and searched
 * directly with full word case insensitive string compares
 * with binary search. For 256000 words in index there will be
 * 256000/128 = 2000 words in mini-index that will occupy
 * 2000*32 (avgword + off + delim/attr + sizeof(struct mini)),
 * which is less than 16 pages (can always be resident in mem).
 *
 * We just need to string search log_2(2000) + 128 words of
 * length 12B each in the worst case ===> VERY FAST. This is
 * not the best possible but space is the limit. If we hash the
 * whole index/regions in the index, we need TOO MUCH memory.
 */

/*
 * Binary search mini_array[beginindex..endindex); return 1 if success, 0 if failure.
 * Sets begin and end offsets for direct search; initially beginindex=0, endindex=mini_array_len
 */
int
get_mini(word, len, beginoffset, endoffset, beginindex, endindex, minifp)
	unsigned char *word;
	int	len;
	long	*beginoffset, *endoffset;
	int	beginindex, endindex;
	FILE	*minifp;
{
	int	cmp, midindex;

	if ((mini_array == NULL) || (mini_array_len <= 0)) return 0;

	midindex = beginindex + (endindex - beginindex)/2;
	cmp = strcmp(word, mini_array[midindex].word);
	if (cmp < 0) {	/* word DEFINITELY BEFORE midindex (but still at or after beginindex) */
		if (beginindex >= midindex) {	/* range of search is just ONE element in array */
			*beginoffset = mini_array[midindex].offset;
			if (midindex + 1 < mini_array_len) {
				*endoffset = mini_array[midindex + 1].offset;
			}
			else *endoffset = -1;	/* go till end of file */
			return 1;
		}
		else return get_mini(word, len, beginoffset, endoffset, beginindex, midindex);
	}
	else {	/* word DEFINITELY AT OR AFTER midindex (but still before endindex) */
		if ((cmp == 0) || (endindex <= midindex + 1)) {	/* range of search is just ONE element in array */
			*beginoffset = mini_array[midindex].offset;
			if (midindex + 1 < mini_array_len) {
				*endoffset = mini_array[midindex + 1].offset;
			}
			else *endoffset = -1;	/* go till end of file */
			return 1;
		}
		else return get_mini(word, len, beginoffset, endoffset, midindex, endindex);
	}
}

/* Returns: #of words in mini_array if success or already read, -1 if failure */
int
read_mini(indexfp, minifp)
	FILE	*indexfp, *minifp;	/* indexfp pointing right to first line of word+... */
{
	unsigned char	s[MAX_LINE_LEN], word[MAX_NAME_LEN];
	int	wordnum = 0, wordlen;
	long	offset;
	struct stat st;

	if ((mini_array != NULL) && (mini_array_len > 0)) return mini_array_len;
	if (minifp == NULL) return 0;
	if (fstat(fileno(minifp), &st) == -1) {
		fprintf(stderr, "Can't stat: %s\n", s);
		return -1;
	}
	rewind(minifp);
	fscanf(minifp, "%d\n", &mini_array_len);
	if ((mini_array_len <= 0) || (mini_array_len > (st.st_size / 4 /* \n, space, 1char offset, 1char word */))) {
		fprintf(stderr, "Error in format of: %s\n", s);
		return -1;
	}

	mini_array = (struct mini *)my_malloc(sizeof(struct mini) * mini_array_len);
	memset(mini_array, '\0', sizeof(struct mini) * mini_array_len);

	while ((wordnum < mini_array_len) && (fscanf(minifp, "%s %ld\n", word, &offset) != EOF)) {
		wordlen = strlen((char *)word);
		mini_array[wordnum].word = (char *)my_malloc(wordlen + 2);
		strcpy((char *)mini_array[wordnum].word, (char *)word);
		mini_array[wordnum].offset = offset;
		wordnum ++;
	}

	return mini_array_len;
}

dump_mini(indexfile)
	char	*indexfile;
{
	unsigned char	s[MAX_LINE_LEN], word[MAX_NAME_LEN];
	FILE	*indexfp;
	FILE	*minifp;
	int	wordnum = 0, j, attr_num;
	long	offset;	/* offset if offset of beginning of word */
	char	temp_rdelim[MAX_LINE_LEN];

	temp_rdelim[0] = '\0';  /* Initialize just in case. 10/25/99 --GV */

	if ((indexfp = fopen(indexfile, "r")) == NULL) {
		fprintf(stderr, "Can't open for reading: %s\n", indexfile);
		return;
	}

	sprintf(s, "%s/%s.tmp", INDEX_DIR, MINI_FILE);
	if ((minifp = fopen(s, "w")) == NULL) {
		fprintf(stderr, "Can't open for writing: %s\n", s);
		fclose(indexfp);
		return;
	}

	fgets(s, 256, indexfp);	/* indexnumbers */
	fgets(s, 256, indexfp);	/* onefileperblock */
	fscanf(indexfp, "%%%d%s\n", &attr_num, temp_delim);	/* structured index */

	offset = ftell(indexfp);
	while (fgets(s, MAX_LINE_LEN, indexfp) != NULL) {
		if ((wordnum % WORDS_PER_REGION) == 0) {
			j = 0;
			while ((j < MAX_LINE_LEN) && (s[j] != WORD_END_MARK) && (s[j] != ALL_INDEX_MARK) && (s[j] != '\n')) j++;
			if ((j >= MAX_LINE_LEN) || (s[j] == '\n')) {
				wordnum ++;
				offset = ftell(indexfp);
				continue;
			}
			/* else it is WORD_END_MARK or ALL_INDEX_MARK */
			s[j] = '\0';
			strcpy((char *)word, (char *)s);
			if (fprintf(minifp, "%s %ld\n", word, offset) == EOF) {
				fprintf(stderr, "Error: write failed at %s:%d\n", __FILE__, __LINE__);
				break;
			}
			mini_array_len ++;
		}
		wordnum ++;
		offset = ftell(indexfp);
	}
	fclose(indexfp);
	fflush(minifp);
	fclose(minifp);

	/*
	 * Add amount of space needed for mini_array at the beginning
	 */

	sprintf(s, "%s/%s", INDEX_DIR, MINI_FILE);
	if ((minifp = fopen(s, "w")) == NULL) {
		fprintf(stderr, "Can't open for writing: %s\n", s);
		goto end;
	}
	sprintf(s, "%s/%s.tmp", INDEX_DIR, MINI_FILE);
	if ((indexfp = fopen(s, "r")) == NULL) {
		fprintf(stderr, "Can't open for reading: %s\n", s);
		fclose(minifp);
		goto end;
	}

	fprintf(minifp, "%d\n", mini_array_len);
	while (fgets(s, MAX_LINE_LEN, indexfp) != NULL) {
		fputs(s, minifp);
	}
	fflush(minifp);
	fclose(minifp);
end:
	sprintf(s, "%s/%s.tmp", INDEX_DIR, MINI_FILE);
	unlink(s);
	return;
}
#else	/* WORD_SORTED */
int
get_mini(word, len, beginoffset, endoffset, beginindex, endindex, minifp)
	unsigned char *word;
	int	len;
	long	*beginoffset, *endoffset;
	int	beginindex, endindex;
	FILE	*minifp;
{
	int	index;
	unsigned char array[sizeof(int)];
	extern int	glimpse_isserver;	/* in agrep/agrep.c */

	index = hash64k(word, len);
	if ((mini_array == NULL) || (mini_array_len <= 0) || !glimpse_isserver) {
		if (minifp == NULL) return 0;
		fseek(minifp, (long)(index*sizeof(int)), 0);
		if (fread((void *)array, sizeof(int), 1, minifp) != 1) return 0;
		*beginoffset = decode32b((array[0] << 24) | (array[1] << 16) | (array[2] << 8) | array[3]);
		if (fread((void *)array, sizeof(int), 1, minifp) != 1)
			*endoffset = -1;
		else *endoffset = decode32b((array[0] << 24) | (array[1] << 16) | (array[2] << 8) | array[3]);
		return 1;
	}
	*beginoffset = mini_array[index].offset;
	if (index + 1 < endindex)
		*endoffset = mini_array[index + 1].offset;
	else *endoffset = -1;
	return 1;
}

/* Returns: #of words in mini_array if success or already read, -1 if failure */
int
read_mini(indexfp, minifp)
	FILE	*indexfp, *minifp;	/* indexfp pointing right to first line of word+... */
{
	unsigned char	s[MAX_LINE_LEN], array[sizeof(int)];
	int	offset, hash_value;

	if ((mini_array != NULL) && (mini_array_len > 0)) return mini_array_len;
	if (minifp == NULL) return 0;
	rewind(minifp);
	mini_array_len = MINI_ARRAY_LEN;
	mini_array = (struct mini *)my_malloc(sizeof(struct mini) * mini_array_len);
	memset(mini_array, '\0', sizeof(struct mini) * mini_array_len);

	hash_value = 0;	/* line# I am going to scan */
	offset = 0;
	while ((hash_value < MINI_ARRAY_LEN) && (fread((void *)array, sizeof(int), 1, minifp) == 1)) {
		offset = (array[0] << 24) | (array[1] << 16) | (array[2] << 8) | array[3];
		mini_array[hash_value++].offset = decode32b(offset);
	}
	for (; hash_value<MINI_ARRAY_LEN; hash_value++)
		mini_array[hash_value].offset = -1;	/* end of index file */
	return mini_array_len;
}

/*
 * 1. Find hash64k values of each word. Then fprintf it before the word and put it
 *    in another file. Sort it and put that as the real index.
 * 2. Then in the new index, dump offsets after stripping the hash value out, and
 *    dump the offset at the hash_value-th line into the mini file.
 * 3. The only problem is that the offsets obtained from the index into the parti-
 *    tions won't be in increasing order, but who cares? get_block_numbers() works!
 * 4. In merge_splits(), we have to re-sort everything by word for add-to-index
 *    and fast-index to work properly.
 */
dump_mini(indexfile)
	char	*indexfile;
{
	unsigned char	s[MAX_LINE_LEN], *t, word[MAX_NAME_LEN], c;
	unsigned char	indexnumber[MAX_LINE_LEN], onefileperblock[MAX_LINE_LEN];
	int	attr_num, linelen;
	FILE	*indexfp;
	FILE	*newindexfp;
	FILE	*minifp;
	long	offset;	/* offset if offset of beginning of word */
	int	eoffset, j, hash_value, prev_hash_value;	/* NOT shorts!! */
	int	rc;	/* return code from system(3) */
	char	es1[MAX_LINE_LEN], es2[MAX_LINE_LEN], es3[MAX_LINE_LEN], temp_rdelim[MAX_LINE_LEN];

	temp_rdelim[0] = '\0';  /* Initialize in case not read. 10/25/99 --GV */

	/*
	 * First change the sorting order of the index file.
	 */
	if ((indexfp = fopen(indexfile, "r")) == NULL) {
		fprintf(stderr, "Can't open for reading: %s\n", indexfile);
		exit(2);
	}

	sprintf(s, "%s.tmp", indexfile);
	if ((newindexfp = fopen(s, "w")) == NULL) {
		fprintf(stderr, "Can't open for writing: %s\n", s);
		fclose(indexfp);
		exit(2);
	}

	/* Must store since sort -n can screw it up */
	fgets(indexnumber, 256, indexfp);
	fgets(onefileperblock, 256, indexfp);
	if ( !fscanf(indexfp, "%%%d%s\n", &attr_num, temp_rdelim)) 
		fscanf(indexfp, "%%%d\n", &attr_num);

	while (fgets(s, MAX_LINE_LEN, indexfp) != NULL) {
		j = 0;
		linelen = strlen(s);
		while ((j < linelen) && (s[j] != WORD_END_MARK) && (s[j] != ALL_INDEX_MARK) && (s[j] != '\n') && (s[j] != '\0')) j++;
		if ((j >= linelen) || (s[j] == '\n') || (s[j] == '\0')) {
			continue;
		}
		/* else it is WORD_END_MARK or ALL_INDEX_MARK */
		c = s[j];
		s[j] = '\0';
		hash_value = hash64k(s, j);
		s[j] = c;
		fprintf(newindexfp, "%d ", hash_value);
		if (fputs(s, newindexfp) == EOF) {
			fprintf(stderr, "Error: write failed at %s:%d\n", __FILE__, __LINE__);
			exit(2);
		}
	}
	fclose(indexfp);
	fflush(newindexfp);
	fclose(newindexfp);
#if	SFS_COMPAT
	unlink(indexfile);
#else
	sprintf(s, "exec %s '%s'", SYSTEM_RM, escapesinglequote(indexfile, es1));
	system(s);
#endif

#if	DONTUSESORT_T_OPTION || SFS_COMPAT
	sprintf(s, "exec %s -n '%s.tmp' > '%s'\n", SYSTEM_SORT, escapesinglequote(indexfile, es1), escapesinglequote(indexfile, es2));
#else
	sprintf(s, "exec %s -n -T '%s' '%s.tmp' > '%s'\n", SYSTEM_SORT, escapesinglequote(INDEX_DIR, es1), escapesinglequote(indexfile, es2), escapesinglequote(indexfile, es3));
#endif
	rc = system(s);
	if (rc >> 8) {
		fprintf (stderr, "'sort' command:\n");
		fprintf (stderr, "    %s\n", s);
		fprintf (stderr, "failed with exit status %d\n", rc>>8);
		exit(2);
	}
#if	SFS_COMPAT
	sprintf(s, "%s.tmp", indexfile);
	unlink(s);
#else
	sprintf(s, "exec %s '%s.tmp'", SYSTEM_RM, escapesinglequote(indexfile, es1));
	system(s);
#endif
	system(sync_path);	/* sync() has a BUG */

	/*
	 * Now dump the mini-file's offsets and create the stripped index file
	 */

	if ((indexfp = fopen(indexfile, "r")) == NULL) {
		fprintf(stderr, "Can't open for reading: %s\n", indexfile);
		exit(2);
	}

	sprintf(s, "%s.tmp", indexfile);
	if ((newindexfp = fopen(s, "w")) == NULL) {
		fprintf(stderr, "Can't open for writing: %s\n", s);
		fclose(indexfp);
		exit(2);
	}

	sprintf(s, "%s/%s", INDEX_DIR, MINI_FILE);
	if ((minifp = fopen(s, "w")) == NULL) {
		fprintf(stderr, "Can't open for writing: %s\n", s);
		fclose(indexfp);
		fclose(newindexfp);
		exit(2);
	}

	fputs(indexnumber, newindexfp);
	fputs(onefileperblock, newindexfp);
	if (attr_num != -2) fprintf(newindexfp, "%%%d\n", attr_num);
	else fprintf(newindexfp, "%%%d %s\n", attr_num, temp_rdelim);

	prev_hash_value = -1;
	hash_value = 0;
	offset = ftell(newindexfp);
	while (fgets(s, MAX_LINE_LEN, indexfp) != NULL) {
		linelen = strlen(s);
		t = s;
		while ((*t != ' ') && (t < s + linelen)) t++;
		if (t >= s + linelen) continue;
		*t = '\0';
		sscanf(s, "%d", &hash_value);
		t ++;	/* points to first character of the beginning of s */
		fputs(t, newindexfp);
		if (hash_value != prev_hash_value) {
			for (j=prev_hash_value + 1; j<=hash_value; j++) {
				eoffset = encode32b((int)offset);
				putc((eoffset & 0xff000000) >> 24, minifp);
				putc((eoffset & 0xff0000) >> 16, minifp);
				putc((eoffset & 0xff00) >> 8, minifp);
				if (putc((eoffset & 0xff), minifp) == EOF) {
					fprintf(stderr, "Error: write failed at %s:%d\n", __FILE__, __LINE__);
					exit(2);
				}
			}
			prev_hash_value = hash_value;
		}
		offset = ftell(newindexfp);
	}
	for (hash_value = prev_hash_value + 1; hash_value<MINI_ARRAY_LEN; hash_value++) {
		eoffset = encode32b((int)offset);	/* end of index file */
		putc((eoffset & 0xff000000) >> 24, minifp);
		putc((eoffset & 0xff0000) >> 16, minifp);
		putc((eoffset & 0xff00) >> 8, minifp);
		if (putc((eoffset & 0xff), minifp) == EOF) {
			fprintf(stderr, "Error: write failed at %s:%d\n", __FILE__, __LINE__);
			exit(2);
		}
	}
	fclose(indexfp);
	fflush(newindexfp);
	fclose(newindexfp);
	fflush(minifp);
	fclose(minifp);

#if	SFS_COMPAT
	unlink(indexfile);
#else
	sprintf(s, "exec %s '%s'", SYSTEM_RM, escapesinglequote(indexfile, es1));
	system(s);
#endif
#if	SFS_COMPAT
	sprintf(s, "%s.tmp", indexfile);
	rename(s, indexfile);
#else
	sprintf(s, "exec %s '%s.tmp' '%s'\n", SYSTEM_MV, escapesinglequote(indexfile, es1), escapesinglequote(indexfile, es2));
	system(s);
#endif
	system(sync_path);	/* sync() has a BUG */
}
#endif	/* WORD_SORTED */

/* Creates data structures that are related to the number of files present in
 * ".glimpse_filenames". These data structures are:
 * 1. index sets	-- use my_malloc
 * 2. index bufs	-- use my_malloc
 * Once this is done, this function can be called directly from glimpse/get_filenames()
 * and that can use all sets/bufs data structures directly.
 * This doesn't care how name_list() is created to be an array of arrays to be able to
 * add/delete dynamically from it: this uses malloc completely.
 * But:
 *	disable_list (which is used only inside glimpse_index) must be malloced separately.
 *	multi_dest_index_set (which is used only inside glimpse) must be malloced separately.
 */
initialize_data_structures(files)
int	files;
{
	FILEMASK_SIZE = ((files + 1)/(8*sizeof(int)) + 4);
	REAL_PARTITION = (FILEMASK_SIZE + 4);
	if (REAL_PARTITION < MAX_PARTITION + 2) REAL_PARTITION = MAX_PARTITION + 2;
	REAL_INDEX_BUF = ((files + 1)*3 + 3*8*sizeof(int)  + 6*MAX_WORD_BUF + 2);	/* index line length with OneFilePerBlock (and/or ByteLevelIndex) */
	/* increased by *3 + 24*sizeof(int) to avoid segfaults --GV */
	if (REAL_INDEX_BUF < MAX_SORTLINE_LEN) REAL_INDEX_BUF = MAX_SORTLINE_LEN;
	MAX_ALL_INDEX = (REAL_INDEX_BUF / 2);
	if (src_index_set == NULL) src_index_set = (unsigned int *)my_malloc(sizeof(int)*REAL_PARTITION);
	memset(src_index_set, '\0', sizeof(int) * REAL_PARTITION);
	if (dest_index_set == NULL) dest_index_set = (unsigned int *)my_malloc(sizeof(int)*REAL_PARTITION);
	memset(dest_index_set, '\0', sizeof(int) * REAL_PARTITION);

/* malloc a few extra bytes above REAL_INDEX_BUF */
/* the value of REAL_INDEX_BUF is used to terminate loops, but in the loop up to 4 chars may be assigned */
	if (src_index_buf == NULL) src_index_buf = (unsigned char *)my_malloc(sizeof(char)*(REAL_INDEX_BUF+4));
	memset(src_index_buf, '\0', sizeof(char)*(REAL_INDEX_BUF+4));
	if (dest_index_buf == NULL) dest_index_buf = (unsigned char *)my_malloc(sizeof(char)*(REAL_INDEX_BUF + 4));
	memset(dest_index_buf, '\0', sizeof(char)*(REAL_INDEX_BUF+4));
	if (merge_index_buf == NULL) merge_index_buf = (unsigned char *)my_malloc(sizeof(char)*(REAL_INDEX_BUF + 4));
	memset(merge_index_buf, '\0', sizeof(char)*(REAL_INDEX_BUF+4));
}

destroy_data_structures()
{
	if (src_index_set != NULL) free(src_index_set);
	src_index_set = NULL;
	if (dest_index_set != NULL) free(dest_index_set);
	dest_index_set = NULL;
	if (src_index_buf != NULL) free(src_index_buf);
	src_index_buf = NULL;
	if (dest_index_buf != NULL) free(dest_index_buf);
	dest_index_buf = NULL;
	if (merge_index_buf != NULL) free(merge_index_buf);
	merge_index_buf = NULL;
}

/* We MUST be able to parse name as: "goodoldunixfilename firstwordofotherinfo restofotherinfo_whichifNULL_willnotbeprecededbyblanklikeitdoeshere\n" */
/* len is strlen(name), being points to                   ^ and end points to ^: the firstwordofotherinfo can be used to create .glimpse_filehash when -U ON */
/* Restriction: the 3 strings above cannot contain '\n' or '\0' or ' ' */
/* returns 0 if parsing was successful, -1 if error */
/* begin/end values are NOT stored for each file (painful!), so this function may be called multiple times for the same name: caller MUST save if reqd. */
int
special_parse_name(name, len, begin, end)
	char	*name;
	int	len;
	int	*begin, *end;
{
	int	i;
	int	index;

	*begin = -1;
	*end = -1;
	if (InfoAfterFilename || ExtractInfo) {	/* Glimpse will ALWAYS terminate filename at first blank (no ' ', '\n', '\0' in filename) */
		/* Trying to use FILE_END_MARK instead of blank! --GB 6/7/99 */
		for (i=0; i<len; i++) {
			if (name[i] == '\n') break;
			if (name[i] == FILE_END_MARK)  {
				if (*begin == -1) {
					*begin = i+1;
					if (!InfoAfterFilename) break;	/* don't care about URL since it doesn't exist as far as I know */
				}
				else {
					*end = i;
					break;
				}
			}
		}
		if (*begin == -1) {
			*begin = 0; *end = len;
			return 0;
		}
		else {
			if (*end == -1) *end = len;
			if (*begin >= *end) {
				*end = *begin - 1; *begin = 0;
				/* was returning -1 before, but if can't find any "firstwordofinfo", then just use the first word in buffer for indexing... */
			}
			return 0;
		}
	}
	else {
		*begin = 0; *end = len;
		return 0;
	}
}

/* Puts the actual name of the file in the file-system into temp (caller must pass buffer that is large enough to hold it...) */
int
special_get_name(name, len, temp)
	char	*name;
	int	len;
	char	*temp;
{
	int	begin=-1, end=-1;
	if (name == NULL) return -1;
	if (len < 0) len = strlen(name);
	if (len <= 0) {
		errno = EINVAL;
		return -1;
	}
	if (special_parse_name(name, len, &begin, &end) == -1) return -1;
	if ((begin >= MAX_LINE_LEN) || (len >= MAX_LINE_LEN)) {
		errno = ENAMETOOLONG;
		return -1;
	}
	if (begin > 0) {	/* points to first element of the information (like URL) stored after filename */
		memcpy(temp, name, begin-1);
		temp[begin-1] = '\0';
	}
	else {	/* no other information stored with filename */
		memcpy(temp, name, len);
		temp[len] = '\0';
	}
	return 0;
}

/* Must NOT write into name or flag since they may be passed as "const" char* on some systems */
FILE *
my_fopen(name, flag)
	char	*name;
	char	*flag;
{
	int	len;
	char	temp[MAX_LINE_LEN];

	if (name == NULL) return NULL;
	len = strlen(name);
	if (special_get_name(name, len, temp) == -1) return NULL;
	return fopen(temp, flag);
}

int
my_open(name, flag, mode)
	char	*name;
	int	flag, mode;
{
	int	len;
	char	temp[MAX_LINE_LEN];

	if (name == NULL) return -1;
	len = strlen(name);
	if (special_get_name(name, len, temp) == -1) return -1;
	return open(temp, flag, mode);
}

int
my_stat(name, buf)
	char	*name;
	struct stat *buf;
{
	int	len;
	char	temp[MAX_LINE_LEN];

	if (name == NULL) return -1;
	len = strlen(name);
	if (special_get_name(name, len, temp) == -1) return -1;
	return stat(temp, buf);
}

int
my_lstat(name, buf)
	char	*name;
	struct stat *buf;
{
	int	len;
	char	temp[MAX_LINE_LEN];

	if (name == NULL) return -1;
	len = strlen(name);
	if (special_get_name(name, len, temp) == -1) return -1;
	return lstat(temp, buf);
}

/* Changed hash-routines to look at exactly that portion of the filename that occurs before the first blank character, */
/* and use that to compare names: Oct/96 --- But lose efficiency since must parse name everytime: at least 1 string copy */
/* Using FILE_END_MARK instead of blank. --GB 6/7/99 */
name_hashelement *name_hashtable[MAX_64K_HASH];	/* if (!BigFilenameHashTable) then only the first 4K entries in it are used */

/*
 * Returns the index of the name if the it is found amongst the set
 * of files in name_array; -1 otherwise.
 */
int
get_filename_index(name)
	char	*name;
{
	int	index;
	int	len;
	int	i, begin=-1, end=-1;
	/* int	skips=0; */
	name_hashelement	*e;
	char	*temp;
	int	temp_len;

	if (name == NULL) return -1;
	len = strlen(name);
	if (special_parse_name(name, len, &begin, &end) == -1) return -1;
	if ((begin >= MAX_LINE_LEN) || (len >= MAX_LINE_LEN)) {
		errno = ENAMETOOLONG;
		return -1;
	}
	temp = name;
	if (begin > 0) {	/* points to first element of the information (like URL) stored after filename */
		temp_len = begin - 1;
	}
	else {	/* no other information stored with filename */
		temp_len = len;
	}

	if (FirstWordOfInfoIsKey) index = hashNk(name, end-begin);
	else {	/* hash on filename */
		if (begin <= 0) index = hashNk(name, len);
		else index = hashNk(name, begin-1);
	}
	e = name_hashtable[index];
	while((e != NULL) && (strncmp(temp, e->name, temp_len))) {
		/* skips ++; */
		e = e->next;
	}
	/* fprintf(STATFILE, "skips = %d\n", skips); */
	if (e == NULL) return -1;
	return e->index;
}

insert_filename(name, name_index)
	char	*name;
	int	name_index;
{
	int	len;
	int	index;
	int	i, begin=-1, end=-1;
	name_hashelement **pe;
	char	*temp;
	int	temp_len;

	if (name == NULL) return;
	len = strlen(name);
	if (special_parse_name(name, len, &begin, &end) == -1) return;
	if ((begin >= MAX_LINE_LEN) || (len >= MAX_LINE_LEN)) {
		errno = ENAMETOOLONG;
		return;
	}
	temp = name;
	if (begin > 0) {	/* points to first element of the information (like URL) stored after filename */
		temp_len = begin - 1;
	}
	else {	/* no other information stored with filename */
		temp_len = len;
	}

	if (FirstWordOfInfoIsKey) index = hashNk(name, end-begin);
	else {	/* hash on filename */
		if (begin <= 0) index = hashNk(name, len);
		else index = hashNk(name, begin-1);
	}
	pe = &name_hashtable[index];
	while((*pe != NULL) && (strncmp((*pe)->name, temp, temp_len))) pe = &(*pe)->next;
	if ((*pe) != NULL) return;
	if ((*pe = (name_hashelement *)my_malloc(sizeof(name_hashelement))) == NULL) {
		fprintf(stderr, "malloc failure in insert_filename %s:%d\n", __FILE__, __LINE__);
		exit(2);
	}
	(*pe)->next = NULL;
#if	0
	if (((*pe)->name = (char *)my_malloc(len + 2)) == NULL) {
		fprintf(stderr, "malloc failure in insert_filename %s:%d\n", __FILE__, __LINE__);
		exit(2);
	}
	strcpy((*pe)->name, name);
#else
	(*pe)->name = name;
#endif
	(*pe)->name_len = strlen(name);
	(*pe)->index = name_index;
}

change_filename(name, len, index, newname)
	char	*name;
	int	len;
	int	index;
	char	*newname;
{
	name_hashelement **pe, *t;
	char	temp[MAX_LINE_LEN];
	int	temp_len;

	if (special_get_name(name, len, temp) == -1) return;
	temp_len = strlen(temp);
	pe = &name_hashtable[index];
	while((*pe != NULL) && (strncmp((*pe)->name, temp, temp_len))) pe = &(*pe)->next;
	if ((*pe) == NULL) return;
#if	0
	my_free((*pe)->name);
#endif
	(*pe)->name = newname;
	return;
}

delete_filename(name, name_index)
	char	*name;
	int	name_index;
{
	int	len;
	int	index;
	int	i, begin=-1, end=-1;
	name_hashelement **pe, *t;
	char	*temp;
	int	temp_len;

	if (name == NULL) return;
	len = strlen(name);
	if (special_parse_name(name, len, &begin, &end) == -1) return;
	if ((begin >= MAX_LINE_LEN) || (len >= MAX_LINE_LEN)) {
		errno = ENAMETOOLONG;
		return;
	}
	temp = name;
	if (begin > 0) {	/* points to first element of the information (like URL) stored after filename */
		temp_len = begin - 1;
	}
	else {	/* no other information stored with filename */
		temp_len = len;
	}

	if (FirstWordOfInfoIsKey) index = hashNk(name, end-begin);
	else {	/* hash on filename */
		if (begin <= 0) index = hashNk(name, len);
		else index = hashNk(name, begin-1);
	}
	pe = &name_hashtable[index];
	while((*pe != NULL) && (strncmp((*pe)->name, temp, temp_len))) pe = &(*pe)->next;
	if ((*pe) == NULL) return;
	t = *pe;
	*pe = (*pe)->next;
#if	0
	my_free(t->name);
#endif
	my_free(t, sizeof(name_hashelement));
	return;
}

init_filename_hashtable()
{
	int	i;
	for (i=0; i<MAX_64K_HASH; i++) name_hashtable[i] = NULL;
}

int	built_filename_hashtable = 0;
build_filename_hashtable(names, num)
	char	**names[];
	int	num;
{
	int	i;

	init_filename_hashtable();
	for (i=0; i<num; i++) insert_filename(LIST_GET(names, i), i);
	built_filename_hashtable = 1;
}

destroy_filename_hashtable()
{
	int	i;
	name_hashelement **pe, *t;

	for (i=0; i<MAX_64K_HASH; i++) {
		pe = &name_hashtable[i];
		while(*pe!=NULL) {
			t = *pe;
			*pe = (*pe)->next;
#if	0
			my_free(t->name);
#endif
			my_free(t, sizeof(name_hashelement));
		}
		*pe = NULL;
	}
	built_filename_hashtable = 0;
}

long
get_file_time(fp, stbuf, name, i)
	FILE	*fp;
	struct stat *stbuf;
	char	*name;
	int	i;
{
	CHAR	array[sizeof(long)];
	int	xx;
	long	ret = 0;
	struct stat mystbuf;
	if (fp != NULL) {
		fseek(fp, i*sizeof(long), 0);
		fread(array, sizeof(long), 1, fp);
		for (xx=0; xx<sizeof(long); xx++) ret |= array[xx] << (8*(sizeof(long) - xx - 1));
	}
	else if (stbuf != NULL) {
		ret = stbuf->st_mtime;
	}
	else {
		if (my_stat(name, &mystbuf) == -1) ret = 0;
		else ret = mystbuf.st_mtime;
	}
	return ret;
}
