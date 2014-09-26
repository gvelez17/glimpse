/* Copyright (c) 1994 Sun Wu, Udi Manber, Burra Gopal.  All Rights Reserved. */
/* ./glimpse/index/glimpse.c */
#include "glimpse.h"
#include <stdlib.h>
#include <sys/time.h>
#if	ISO_CHAR_SET
#include <locale.h>	/* support for 8bit character set:ew@senate.be */
#endif
#include <errno.h>

extern char **environ;
extern int errno;
extern FILE *TIMEFILE;	/* file descriptor for sorting .glimpse_filenames by time */
#if	BG_DEBUG
extern FILE  *LOGFILE; 	/* file descriptor for LOG output */
#endif	/*BG_DEBUG*/
extern FILE  *STATFILE;	/* file descriptor for statistical data about indexed files */
extern FILE  *MESSAGEFILE;	/* file descriptor for important messages meant for the user */
extern char  INDEX_DIR[MAX_LINE_LEN];
extern struct stat istbuf;

#ifdef BUILDCAST
/* TEMP_DIR is normally defined in ../main.c; if we're building
 * buildcast, that's not linked in, so we need to define one here. */
/* char * TEMP_DIR = NULL; */
static char * TEMP_DIR = "/tmp";
#else
extern char	*TEMP_DIR; /* directory to store glimpse temporary files, usually /tmp unless -T is specified */
#endif /* BUILDCAST */
extern int indexable_char[256];
extern int GenerateHash;
extern int KeepFilenames;
extern int OneFilePerBlock;
extern int IndexNumber;
extern int CountWords;
extern int StructuredIndex;
extern int attr_num;
extern int MAXWORDSPERFILE;
extern int NUMERICWORDPERCENT;
extern int AddToIndex;
extern int DeleteFromIndex;
extern int PurgeIndex;
extern int FastIndex;
extern int BuildDictionary;
extern int BuildDictionaryExisting;
extern int CompressAfterBuild;
extern int IncludeHigherPriority;
extern int FilenamesOnStdin;
extern int ExtractInfo;
extern int InfoAfterFilename;
extern int FirstWordOfInfoIsKey;
extern int UseFilters;
extern int ByteLevelIndex;
extern int RecordLevelIndex;
extern int StoreByteOffset;
extern char rdelim[MAX_LINE_LEN];
extern char old_rdelim[MAX_LINE_LEN];
extern int rdelim_len;
/* extern int IndexUnderscore; */
extern int IndexableFile;
extern int MAX_PER_MB, MAX_INDEX_PERCENT;
extern int I_THRESHOLD;
extern int BigHashTable;
extern int BigFilenameHashTable;
extern int IndexEverything;
extern int BuildTurbo;
extern int SortByTime;

extern int AddedMaxWordsMessage;
extern int AddedMixedWordsMessage;

extern int file_num;
extern int old_file_num;
extern int new_file_num;
extern int file_id;
extern int part_num;
extern char **name_list[MAXNUM_INDIRECT];
extern int p_table[MAX_PARTITION];
extern int  *size_list[MAXNUM_INDIRECT];
extern int p_size_list[];
extern unsigned int *disable_list;
extern int memory_usage;
extern int mask_int[];
extern int REAL_PARTITION, REAL_INDEX_BUF, MAX_ALL_INDEX, FILEMASK_SIZE;
extern struct indices	*deletedlist;
extern char sync_path[MAX_LINE_LEN];
extern int ATLEASTONEFILE;

extern set_usemalloc();	/* compress/misc.c */

char IProgname[MAX_LINE_LEN];
int ModifyFilenamesIndex = 0;

/*
 * Has newnum crossed the boundary of an encoding? This is so rare that we
 * needn't optimize it by changing the format of the old index and reusing it.
 */
cross_boundary(oldnum, newnum)
	int	oldnum, newnum;
{
	int	ret;

	if (oldnum <= 0) return 0;
	ret =  ( ((oldnum <= MaxNum8bPartition) && (newnum > MaxNum8bPartition)) ||
		 ((oldnum <= MaxNum12bPartition) && (newnum > MaxNum12bPartition)) ||
		 ((oldnum <= MaxNum16bPartition) && (newnum > MaxNum16bPartition)) );
	if (ret) fprintf(MESSAGEFILE, "Must change index format. Commencing fresh indexing...\n");
	return ret;
}

determine_sync()
{
	char	S[1024], s1[256], s2[256];
	FILE	*fp;
	int	i, ret;

	strcpy(sync_path, "sync");
	sprintf(S, "exec whereis sync > %s/zz.%d", TEMP_DIR,getpid());
	/* Change it to use which: not urgent. */
	system(S);
	sprintf(S, "%s/zz.%d", TEMP_DIR,getpid());
	if ((fp = fopen(S, "r")) == NULL) {
		/* printf("11111\n"); */
		return 0;
	}
	if ((ret = fread(S, 1, sizeof(S)-1, fp)) <= 0) {
		sprintf(S, "%s/zz.%d", TEMP_DIR,getpid());
		unlink(S);
		fclose(fp);
		/* printf("22222\n"); */
		return 0;
	}
        S [ret] = 0; /* terminate string */
	sprintf(s1, "%s/zz.%d", TEMP_DIR,getpid());
	unlink(s1);
	fclose(fp);
	/* printf("read: %s\n", S); */

	sscanf(S, "%s%s", s1, s2);
	/* printf("s1=%s s2=%s\n", s1, s2); */
	if (strncmp(s1, "sync", 4)) {
		/* printf("33333\n"); */
		return 0;
	}
	if (!strcmp(s2, "") || !strcmp(s2, " ")) {
		/* printf("44444\n"); */
		return 0;
	}
	if (strstr(s2, "sync") == NULL) {
		/* printf("55555\n"); */
		return 0;
	}
	strcpy(sync_path, s2);
	/* printf("Using sync in: %s\n", sync_path); */
	return 1;
}

main(argc, argv)
int argc;
char **argv;
{ 
    int pid = getpid();
    int	i, m = 0;
    char *indexdir, es1[MAX_LINE_LEN], es2[MAX_LINE_LEN];
    char s[MAX_LINE_LEN], s1[MAX_LINE_LEN];
    char working_dir[MAX_LINE_LEN];
    FILE *tmpfp;
    char hash_file[MAX_LINE_LEN], string_file[MAX_LINE_LEN], freq_file[MAX_LINE_LEN];
    char tmpbuf[1024];
    struct stat stbuf;
    char name[MAX_LINE_LEN];
    char outname[MAX_LINE_LEN];
    int specialwords, threshold;
    int backup;
    struct indices *get_removed_indices();
    struct timeval tv;

#if	ISO_CHAR_SET
    setlocale(LC_ALL,""); /* support for 8bit character set: ew@senate.be, Henrik.Martin@eua.ericsson.se */
#endif
    BuildDictionary = ON;
    set_usemalloc();
    srand(pid);
    umask(077);
    determine_sync();

    INDEX_DIR[0] = '\0';
    specialwords = threshold = -1;	/* so that compute_dictionary can use defaults not visible here */
    strncpy(IProgname, argv[0], MAX_LINE_LEN);
    memset(size_list, '\0', sizeof(int *) * MAXNUM_INDIRECT);	/* free it once partition successfully calculates p_size_list */
    memset(name_list, '\0', sizeof(char **) * MAXNUM_INDIRECT);
    memset(p_size_list, '\0', sizeof(int) * MAX_PARTITION);
    build_filename_hashtable((char *)NULL, 0);

    /*
     * Process options.
     */

    while (argc > 1) {
	if (strcmp(argv[1], "-help") == 0) {
	    return usage(1);
	}
#if	!BUILDCAST
	else if (strcmp(argv[1], "-R") == 0) {
	    ModifyFilenamesIndex = 1;
	    argc --; argv ++;
	}
	else if (strcmp(argv[1], "-V") == 0) {
	    printf("\nThis is glimpseindex version %s, %s.\n\n", GLIMPSE_VERSION, GLIMPSE_DATE);
	    return(0);
	}
	else if (strcmp(argv[1], "-T") == 0) {
	    BuildTurbo = ON;
	    argc --; argv ++;
	}
	else if (strcmp(argv[1], "-I") == 0) {
	    IndexableFile = ON;
	    argc --; argv ++;
	}
	else if(strcmp(argv[1], "-a") == 0) {
	    AddToIndex = ON;
	    argc--; argv++;
	}
	else if(strcmp(argv[1], "-b") == 0) {
	    ByteLevelIndex = ON;
	    argc--; argv++;
	}
	else if(strcmp(argv[1], "-O") == 0) {
	    StoreByteOffset = ON;
	    argc--; argv++;
	}
	else if(strcmp(argv[1], "-r") == 0) {
	    ByteLevelIndex = ON;
	    RecordLevelIndex = ON;
	    if (argc <= 2) {
		fprintf(stderr, "The -r option must be followed by a delimiter\n");
		return usage(1);
	    }
	    else {
		strncpy(rdelim, argv[2], MAX_LINE_LEN);
		rdelim[MAX_LINE_LEN-1] = '\0';
		rdelim_len = strlen(rdelim);
		strcpy(old_rdelim, rdelim);
		argc -= 2; argv += 2;
	    }
	}
	else if(strcmp(argv[1], "-c") == 0) {
	    CountWords = ON;
	    argc--; argv++;
	}
	else if(strcmp(argv[1], "-d") == 0) {
	    DeleteFromIndex = ON;
	    argc --; argv ++;
	}
	else if(strcmp(argv[1], "-D") == 0) {
	    PurgeIndex = OFF;
	    argc --; argv ++;
	}
	else if(strcmp(argv[1], "-f") == 0) {
	    FastIndex = ON;
	    argc--; argv++;
	}
	else if (strcmp(argv[1], "-o") == 0) {
	    OneFilePerBlock = ON;
	    argc --; argv ++;
	}
	else if (strcmp(argv[1], "-s") == 0) {
	    StructuredIndex = ON;
	    argc --; argv ++;
	}
	else if(strcmp(argv[1], "-z") == 0) {
	    UseFilters = ON;
	    argc--; argv++;
	}
	else if(strcmp(argv[1], "-t") == 0) {
	    SortByTime = ON;
	    argc--; argv++;
	}
	else if (strcmp(argv[1], "-C") == 0) {
		BigFilenameHashTable = 1;
		argc --; argv ++;
	}
#else	/*!BUILDCAST*/
	else if (strcmp(argv[1], "-V") == 0) {
	    printf("\nThis is buildcast version %s, %s.\n\n", GLIMPSE_VERSION, GLIMPSE_DATE);
	    return(0);
	}
	else if(strcmp(argv[1], "-C") == 0) {
	    CompressAfterBuild = ON;
	    argc --; argv ++;
	}
	else if(strcmp(argv[1], "-E") == 0) {
	    BuildDictionaryExisting = ON;
	    argc --; argv ++;
	}
	else if (strcmp(argv[1], "-t") == 0) {
	    if ((argc <= 2) || !(isdigit(argv[2][0]))) {
		return usage(1);
	    }
	    else {
		threshold = atoi(argv[2]);
		argc -= 2; argv += 2;
	    }
	}
	else if (strcmp(argv[1], "-l") == 0) {
	    if ((argc <= 2) || !(isdigit(argv[2][0]))) {
		return usage(1);
	    }
	    else {
		specialwords = atoi(argv[2]);
		argc -= 2; argv += 2;
	    }
	}
#endif	/*!BUILDCAST*/
	else if (strcmp(argv[1], "-M") == 0) {
	    if (argc == 2) {
		fprintf(stderr, "-M should be followed by the amount of memory in MB for indexing words\n");
		return usage(1);
	    }
	    m = atoi(argv[2]);
	    if (m < 1) {
		fprintf(stderr, "Ignoring -M %d (< 1 MB). Using default value of about 2 MB\n", m);
		return usage(1);
	    }
	    else {
		/*
		 * Calculate I_THRESHOLD approximately. Note: 2*1024*1024*2 / (2*24 + 32 + 12) = 47662, DEF_I_THRESHOLD = 40000, so OK
		 * N * sizeofindices + N*(avgwordlen + sizeoftoken)/indicespertoken <= mem
		 * elemsperset = occurrences/indicespertoken
		 * N <= mem * occurrences / (sizeofindices*indicespertoken + avgwordlen + sizeoftoken)
		 */
		I_THRESHOLD = m * 1024 * 1024 * (INDICES_PER_TOKEN) /
				(INDICES_PER_TOKEN * sizeof(struct indices) + sizeof(struct token) + AVG_WORD_LEN);
		fprintf(stderr, "Using %d words as threshold before merge\n", I_THRESHOLD/INDICES_PER_TOKEN);
	    }
	    argc -= 2; argv += 2;
	}
	else if (strcmp(argv[1], "-w") == 0) {
	    if (argc == 2) {
		fprintf(stderr, "-w should be followed by the number of words\n");
		return usage(1);
	    }
	    MAXWORDSPERFILE = atoi(argv[2]);
	    argc -= 2; argv += 2;
	}
	else if (strcmp(argv[1], "-S") == 0) {
	    if (argc == 2) {
		fprintf(stderr, "-S should be followed by the stop list limit\n");
		return usage(1);
	    }
	    MAX_PER_MB = MAX_INDEX_PERCENT = atoi(argv[2]);
	    argc -= 2; argv += 2;
	}
	else if(strcmp(argv[1], "-n") == 0) {
	    IndexNumber = ON;
	    if ((argc <= 2) || !(isdigit(argv[2][0]))) {	/* -n has no arg */
		argc --; argv ++;
	    }
	    else {
		NUMERICWORDPERCENT = atoi(argv[2]);
		if ((NUMERICWORDPERCENT > 100) || (NUMERICWORDPERCENT < 0)) {
		    fprintf(stderr, "The percentage of numeric words must be in [0..100]\n");
		    return usage(1);
		}
		argc-=2; argv+=2;
	    }
	}
	else if(strcmp(argv[1], "-h") == 0) {
	    /* I want to generate .glimpse_filehash and .glimpse_filehash_index */
	    GenerateHash = ON;
	    argc --; argv ++;
	}
	else if(strcmp(argv[1], "-i") == 0) {
	    IncludeHigherPriority = ON;
	    argc --; argv ++;
	}
	else if(strcmp(argv[1], "-k") == 0) {
	    /* I want to know what files were there before: used in SFS to compute new sets from old ones */
	    KeepFilenames = ON;
	    argc --; argv ++;
	}
	else if (strcmp(argv[1], "-B") == 0) {
		BigHashTable = 1;
		argc --; argv ++;
	}
	else if (strcmp(argv[1], "-E") == 0) {
		IndexEverything = 1;	/* without doing stat tests, etc. */
		argc --; argv ++;
	}
	else if(strcmp(argv[1], "-F") == 0) {
	    FilenamesOnStdin = ON;
	    argc--; argv++;
	}
	else if(strcmp(argv[1], "-X") == 0) {	/* extract some info to append after a ' ' to filename in filename-buffer */
	    ExtractInfo = ON;
	    argc--; argv++;
	}
	else if(strcmp(argv[1], "-U") == 0) {	/* some information is there after blank after filename on same line as filename-buffer: makes sense only with -F */
	    InfoAfterFilename = ON;
	    argc--; argv++;
	}
	else if(strcmp(argv[1], "-K") == 0) {	/* first word of above info/or the extracted info is the key */
	    FirstWordOfInfoIsKey = ON;
	    argc--; argv++;
	}
	/*
	else if(strcmp(argv[1], "-u") == 0) {
	    IndexUnderscore = ON;
	    argc--; argv++;
	}
	*/
	else if (strcmp(argv[1], "-H") == 0) {
	    if (argc == 2) {
		fprintf(stderr, "-H should be followed by a directory name\n");
		return usage(1);
	    }
	    strncpy(INDEX_DIR, argv[2], MAX_LINE_LEN);
	    argc -= 2; argv += 2;
	}
	else break;	/* rest are directory names */
    }

    if (RecordLevelIndex && StructuredIndex) {
	fprintf(stderr, "-r and -s are not compatible!\n");
	return usage(1);
    }

    if (StoreByteOffset && !RecordLevelIndex) {
	fprintf(stderr, "ignoring -O since -r was not specified\n");
	StoreByteOffset = OFF;
    }

    if (InfoAfterFilename && !FilenamesOnStdin) {
	fprintf(stderr, "-U works only when -F is specified!\n");
	return usage(1);
    }

    if (FirstWordOfInfoIsKey && !(InfoAfterFilename || ExtractInfo)) {
	fprintf(stderr, "-K works only when one of -X or -U are specified!\n");
	return usage(1);
    }

    if (RecordLevelIndex) {
	/* printf("old_rdelim = %s rdelim = %s rdelim_len = %d\n", old_rdelim, rdelim, rdelim_len); */
	preprocess_delimiter(rdelim, rdelim_len, rdelim, &rdelim_len);
	/* printf("processed rdelim = %s rdelim_len = %d\n", rdelim, rdelim_len); */
    }

    if (ModifyFilenamesIndex) {
	int	offset = 0;
	char	buffer[1024];
	FILE	*filefp, *indexfp;

	sprintf(buffer, "%s/%s", INDEX_DIR, NAME_LIST);
	if ((filefp = fopen(buffer, "r")) == NULL) {
	    fprintf(stderr, "Cannot open %s for reading\n", buffer);
	    exit(2);
	}
	sprintf(buffer, "%s/%s.tmp", INDEX_DIR, NAME_LIST_INDEX);
	if ((indexfp = fopen(buffer, "w")) == NULL) {
	    fprintf(stderr, "Cannot open %s for writing\n", buffer);
	    exit(2);
	}
	fgets(buffer, 1024, filefp);	/* skip over num. of file names */
	offset += strlen(buffer);
	while (fgets(buffer, 1024, filefp) != NULL) {
		putc((offset & 0xff000000) >> 24, indexfp);
		putc((offset & 0xff0000) >> 16, indexfp);
		putc((offset & 0xff00) >> 8, indexfp);
		putc((offset & 0xff), indexfp);
		offset += strlen(buffer);
	}
	fflush(filefp);
	fclose(filefp);
	fflush(indexfp);
	fclose(indexfp);
#if	SFS_COMPAT
	sprintf(s, "%s/%s.tmp", INDEX_DIR, NAME_LIST_INDEX);
	sprintf(s1, "%s/%s", INDEX_DIR, NAME_LIST_INDEX);
	return rename(s, s1);
#else
	sprintf(buffer, "mv %s/%s.tmp %s/%s", INDEX_DIR, NAME_LIST_INDEX, INDEX_DIR, NAME_LIST_INDEX);
	return system(buffer);
#endif
    }

    BuildTurbo = ON;	/* always ON: user can remove .glimpse_turbo if not needed */
    /*
     * Look for invalid option combos.
     */

    if ((argc<=1) && (!FilenamesOnStdin) && !FastIndex) {
	return usage(1);
    }

    if (DeleteFromIndex && (AddToIndex || CountWords || IndexableFile)) {
	/* With -f, it is automatic for files not found in OS but present in index; without it, an explicit set of files is required as argument on cmdline */
	fprintf(stderr, "-d cannot be used with -I, -a or -c (see man pages)\n");
	exit(1);
    }

    if (ByteLevelIndex) {
	if (MAX_PER_MB <= 0) {
	    fprintf(stderr, "Stop list limit (#of occurrences per MB) '%d' must be > 0\n", MAX_PER_MB);
	    exit(1);
	}
    }
    else if (OneFilePerBlock) {
	if ((MAX_INDEX_PERCENT <= 0) || (MAX_INDEX_PERCENT > 100)) {
	    fprintf(stderr, "Stop list limit (%% of occurrences in files) '%d' must be in (0, 100]\n", MAX_INDEX_PERCENT);
	    exit(1);
	}
    }

    /*
     * Find the index directory since it is used in all options.
     */

    if (INDEX_DIR[0] == '\0') {
	if ((indexdir = getenv("HOME")) == NULL) {
	    getcwd(INDEX_DIR, MAX_LINE_LEN-1);
	    fprintf(stderr, "Using working-directory '%s' to store index\n\n", INDEX_DIR);
	}
	else strncpy(INDEX_DIR, indexdir, MAX_LINE_LEN);
    }
    getcwd(working_dir, MAX_LINE_LEN - 1);
    if (-1 == chdir(INDEX_DIR)) {
	fprintf(stderr, "Cannot change directory to %s\n", INDEX_DIR);
	return usage(0);
    }
    getcwd(INDEX_DIR, MAX_LINE_LEN - 1);	/* must be absolute path name */
    chdir(working_dir);	/* get back to where you were */

    if (IndexableFile) {	/* traverse the given directories and output names of files that are indexable on stdout */
	SortByTime = OFF;
	partition(argc, argv);
	return 0;
    }
    else {
#if	BUILDCAST
	printf("\nThis is buildcast version %s, %s.\n\n", GLIMPSE_VERSION, GLIMPSE_DATE);
#else	/*BUILDCAST*/
	printf("\nThis is glimpseindex version %s, %s.\n\n", GLIMPSE_VERSION, GLIMPSE_DATE);
#endif	/*BUILDCAST*/
    }

    if (ByteLevelIndex) {
#if	0
	/* We'll worry about these things later */
	if (AddToIndex || DeleteFromIndex || FastIndex) {
	    fprintf(stderr, "Fresh indexing recommended: -a, -d and -f are not supported with -b as yet\n");
	    exit(1);
	}
	AddToIndex = FastIndex = OFF;
#endif
	CountWords = OFF;
	OneFilePerBlock = ON;
    }

    if (SortByTime) {
	if (DeleteFromIndex || AddToIndex) {
	    fprintf(stderr, "Fresh indexing recommended: -a and -d are not supported with -t as yet\n");
	    exit(1);
	}
	FastIndex = OFF;	/* automatically shuts it off as of now: we shall optimize -t with -f later */
    }

    /*
     * CONVENTION: all the relevant output is on stdout; warnings/errors are on stderr.
     * Initialize / open important files.
     */

    read_filters(INDEX_DIR, UseFilters);

    freq_file[0] = hash_file[0] = string_file[0] = '\0';
    strcpy(freq_file, INDEX_DIR);
    strcat(freq_file, "/");
    strcat(freq_file, DEF_FREQ_FILE);
    strcpy(hash_file, INDEX_DIR);
    strcat(hash_file, "/");
    strcat(hash_file, DEF_HASH_FILE);
    strcpy(string_file, INDEX_DIR);
    strcat(string_file, "/");
    strcat(string_file, DEF_STRING_FILE);
    initialize_tuncompress(string_file, freq_file, 0);

    sprintf(s, "%s/%s", INDEX_DIR, DEF_TIME_FILE);
    if((TIMEFILE = fopen(s, "w")) == 0) {
	fprintf(stderr, "can't open %s for writing\n", s);
	exit(2);
    }

#if	BG_DEBUG
    sprintf(s, "%s/%s", INDEX_DIR, DEF_LOG_FILE);
    if((LOGFILE = fopen(s, "w")) == 0) {
	fprintf(stderr, "can't open %s for writing\n", s);
	LOGFILE = stderr;
    }
#endif	/*BG_DEBUG*/

    sprintf(s, "%s/%s", INDEX_DIR, DEF_MESSAGE_FILE);
    if((MESSAGEFILE = fopen(s, "w")) == 0) {
	fprintf(stderr, "can't open %s for writing\n", s);
	MESSAGEFILE = stderr;
    }

    sprintf(s, "%s/%s", INDEX_DIR, DEF_STAT_FILE);
    if((STATFILE = fopen(s, "a")) == 0) {
	fprintf(stderr, "can't open %s for appending\n", s);
	STATFILE = stderr;
    }
    gettimeofday(&tv, NULL);
#if	BUILDCAST
    fprintf(STATFILE, "\nThis is buildcast version %s, %s. %s", GLIMPSE_VERSION, GLIMPSE_DATE, ctime(&tv.tv_sec));
#else
    fprintf(STATFILE, "\nThis is glimpseindex version %s, %s. %s", GLIMPSE_VERSION, GLIMPSE_DATE, ctime(&tv.tv_sec));
#endif

#if	BG_DEBUG
    fprintf(LOGFILE, "Index Directory = %s\n\n", INDEX_DIR);
#endif	/*BG_DEBUG*/
    if (MAXWORDSPERFILE != 0) fprintf(MESSAGEFILE, "Index: maximum number of indexed words per file = %d\n", MAXWORDSPERFILE);
    else fprintf(MESSAGEFILE, "Index: maximum number of indexed words per file = infinity\n");
    fprintf(MESSAGEFILE, "Index: maximum percentage of numeric words per file = %d\n", NUMERICWORDPERCENT);

    set_indexable_char(indexable_char);

#if	BUILDCAST

    CountWords = ON;
    AddToIndex = OFF;
    FastIndex = OFF;

    /* Save old search-dictionaries */

    sprintf(s, "%s/.glimpse_index", INDEX_DIR);
    if (!access(s, R_OK)) {
	sprintf(s, "%s/.glimpse_tempdir.%d", INDEX_DIR, pid);
	if (-1 == mkdir(s, 0700)) {
	    fprintf(stderr, "cannot create temporary directory %s\n", s);
	    return -1;
	}
#if	SFS_COMPAT
	sprintf(s, "%s/%s", INDEX_DIR, INDEX_FILE);
	sprintf(s1, "%s/.glimpse_tempdir.%d", INDEX_DIR, pid);
	rename(s, s1);
#else
	sprintf(s, "exec %s -f '%s/%s' '%s/.glimpse_tempdir.%d'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), INDEX_FILE, escapesinglequote(INDEX_DIR, es2), pid);
	system(s);
#endif
#if	SFS_COMPAT
	sprintf(s, "%s/%s", INDEX_DIR, P_TABLE);
	sprintf(s1, "%s/.glimpse_tempdir.%d", INDEX_DIR, pid);
	rename(s, s1);
#else
	sprintf(s, "exec %s -f '%s/%s' '%s/.glimpse_tempdir.%d'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), P_TABLE, escapesinglequote(INDEX_DIR, es2), pid);
	system(s);
#endif
#if	SFS_COMPAT
	sprintf(s, "%s/%s", INDEX_DIR, NAME_LIST);
	sprintf(s1, "%s/.glimpse_tempdir.%d", INDEX_DIR, pid);
	rename(s, s1);
#else
	sprintf(s, "exec %s -f '%s/%s' '%s/.glimpse_tempdir.%d'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), NAME_LIST, escapesinglequote(INDEX_DIR, es2), pid);
	system(s);
#endif
#if	SFS_COMPAT
	sprintf(s, "%s/%s", INDEX_DIR, NAME_LIST_INDEX);
	sprintf(s1, "%s/.glimpse_tempdir.%d", INDEX_DIR, pid);
	rename(s, s1);
#else
	sprintf(s, "exec %s -f '%s/%s' '%s/.glimpse_tempdir.%d'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), NAME_LIST_INDEX, escapesinglequote(INDEX_DIR, es1), pid);
	system(s);
#endif
#if	SFS_COMPAT
	sprintf(s, "%s/%s", INDEX_DIR, NAME_HASH);
	sprintf(s1, "%s/.glimpse_tempdir.%d", INDEX_DIR, pid);
	rename(s, s1);
#else
	sprintf(s, "exec %s -f '%s/%s' '%s/.glimpse_tempdir.%d'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), NAME_HASH, escapesinglequote(INDEX_DIR, es2), pid);
	system(s);
#endif
#if	SFS_COMPAT
	sprintf(s, "%s/%s", INDEX_DIR, NAME_HASH_INDEX);
	sprintf(s1, "%s/.glimpse_tempdir.%d", INDEX_DIR, pid);
	rename(s, s1);
#else
	sprintf(s, "exec %s -f '%s/%s' '%s/.glimpse_tempdir.%d'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), NAME_HASH_INDEX, escapesinglequote(INDEX_DIR, es2), pid);
	system(s);
#endif
#if	SFS_COMPAT
	sprintf(s, "%s/%s", INDEX_DIR, MINI_FILE);
	sprintf(s1, "%s/.glimpse_tempdir.%d", INDEX_DIR, pid);
	rename(s, s1);
#else
	sprintf(s, "exec %s -f '%s/%s' '%s/.glimpse_tempdir.%d'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), MINI_FILE, escapesinglequote(INDEX_DIR, es2), pid);
	system(s);
#endif
#if	SFS_COMPAT
	sprintf(s, "%s/%s", INDEX_DIR, DEF_STAT_FILE);
	sprintf(s1, "%s/.glimpse_tempdir.%d", INDEX_DIR, pid);
	rename(s, s1);
#else
	sprintf(s, "exec %s -f '%s/%s' '%s/.glimpse_tempdir.%d'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), DEF_STAT_FILE, escapesinglequote(INDEX_DIR, es2), pid);
	system(s);
#endif
	/* Don't save messages, log, debug, etc. */
	sprintf(s, "%s/.glimpse_attributes", INDEX_DIR);
	if (!access(s, R_OK)) {
#if	SFS_COMPAT
	    sprintf(s, "%s/%s", INDEX_DIR, ATTRIBUTE_FILE);
	    sprintf(s1, "%s/.glimpse_tempdir.%d", INDEX_DIR, pid);
	    rename(s, s1);
#else
	    sprintf(s, "exec %s -f '%s/%s' '%s/.glimpse_tempdir.%d'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), ATTRIBUTE_FILE, escapesinglequote(INDEX_DIR, es2), pid);
	    system(s);
#endif
	}
    }

    /* Backup old cast-dictionaries: don't use move since indexing might want to use them */
    sprintf(s, "%s/.glimpse_quick", INDEX_DIR);
    if (!access(s, R_OK)) {	/* there are previous cast dictionaries */
	backup = rand();
	sprintf(s, "%s/.glimpse_backup.%x", INDEX_DIR, backup);
	if (-1 == mkdir(s, 0700)) {
	    fprintf(stderr, "cannot create backup directory %s\n", s);
	    return -1;
	}
	sprintf(s, "exec %s -f '%s/.glimpse_quick' '%s/.glimpse_backup.%x'\n", SYSTEM_CP, escapesinglequote(INDEX_DIR, es1), escapesinglequote(INDEX_DIR, es2), backup);
	system(s);
	sprintf(s, "exec %s -f '%s/.glimpse_compress' '%s/.glimpse_backup.%x'\n", SYSTEM_CP, escapesinglequote(INDEX_DIR, es1), escapesinglequote(INDEX_DIR, es2), backup);
	system(s);
	sprintf(s, "exec %s -f '%s/.glimpse_compress.index' '%s/.glimpse_backup.%x'\n", SYSTEM_CP, escapesinglequote(INDEX_DIR, es1), escapesinglequote(INDEX_DIR, es2), backup);
	system(s);
	sprintf(s, "exec %s -f '%s/.glimpse_uncompress' '%s/.glimpse_backup.%x'\n", SYSTEM_CP, escapesinglequote(INDEX_DIR, es1), escapesinglequote(INDEX_DIR, es2), backup);
	system(s);
	sprintf(s, "exec %s -f '%s/.glimpse_uncompress.index' '%s/.glimpse_backup.%x'\n", SYSTEM_CP, escapesinglequote(INDEX_DIR, es1), escapesinglequote(INDEX_DIR, es2), backup);
	system(s);
	printf("Saved previous cast-dictionary in %s/.glimpse_backup.%x\n", INDEX_DIR, backup);
    }

    /* Now index these files, and build new dictionaries */
    partition(argc, argv);
    initialize_data_structures(file_num);
    old_file_num = file_num;
    build_index();

    cleanup();
    save_data_structures();
    destroy_filename_hashtable();
    uninitialize_common();
    uninitialize_tcompress();
    uninitialize_tuncompress();
    compute_dictionary(threshold, DISKBLOCKSIZE, specialwords, INDEX_DIR);

    if (CompressAfterBuild) {
	/* For the new compression */
	if (!initialize_tcompress(hash_file, freq_file, TC_ERRORMSGS)) goto docleanup;
	printf("Compressing files with new dictionary...\n");
	/* Use the set of file-names collected during partition() / modified during build_hash */
	for(i=0; i<file_num; i++) {
	    if ((disable_list != NULL) && (disable_list[block2index(i)] & mask_int[i%(8*sizeof(int))])) continue;	/* nop since disable_list IS NULL */
	    strcpy(name, LIST_GET(name_list, i));
	    tcompress_file(name, outname, TC_REMOVE | TC_EASYSEARCH | TC_OVERWRITE | TC_NOPROMPT);
	}
    }

docleanup:
    /* Restore old search-dictionaries */
    sprintf(s, "%s/.glimpse_tempdir.%d/.glimpse_index", INDEX_DIR, pid);
    if (!access(s, R_OK)) {
#if	SFS_COMPAT
	sprintf(s1, "%s/%s", INDEX_DIR, INDEX_FILE);
	sprintf(s, "%s/.glimpse_tempdir.%d/%s", INDEX_DIR, pid, INDEX_FILE);
	rename(s, s1);
	sprintf(s1, "%s/%s", INDEX_DIR, P_TABLE);
	sprintf(s, "%s/.glimpse_tempdir.%d/%s", INDEX_DIR, pid, P_TABLE);
	rename(s, s1);
	sprintf(s1, "%s/%s", INDEX_DIR, NAME_LIST);
	sprintf(s, "%s/.glimpse_tempdir.%d/%s", INDEX_DIR, pid, NAME_LIST);
	rename(s, s1);
	sprintf(s1, "%s/%s", INDEX_DIR, NAME_LIST_INDEX);
	sprintf(s, "%s/.glimpse_tempdir.%d/%s", INDEX_DIR, pid, NAME_LIST_INDEX);
	rename(s, s1);
	sprintf(s1, "%s/%s", INDEX_DIR, NAME_HASH);
	sprintf(s, "%s/.glimpse_tempdir.%d/%s", INDEX_DIR, pid, NAME_HASH);
	rename(s, s1);
	sprintf(s1, "%s/%s", INDEX_DIR, NAME_HASH_INDEX);
	sprintf(s, "%s/.glimpse_tempdir.%d/%s", INDEX_DIR, pid, NAME_HASH_INDEX);
	rename(s, s1);
	sprintf(s1, "%s/%s", INDEX_DIR, MINI_FILE);
	sprintf(s, "%s/.glimpse_tempdir.%d/%s", INDEX_DIR, pid, MINI_FILE);
	rename(s, s1);
	sprintf(s1, "%s/%s", INDEX_DIR, DEF_STAT_FILE);
	sprintf(s, "%s/.glimpse_tempdir.%d/%s", INDEX_DIR, pid, DEF_STAT_FILE);
	rename(s, s1);
	sprintf(s1, "%s/%s", INDEX_DIR, ATTRIBUTE_FILE);
	sprintf(s, "%s/.glimpse_tempdir.%d/%s", INDEX_DIR, pid, ATTRIBUTE_FILE);
	rename(s, s1);
#else
	/* sprintf(s, "exec %s -f %s/.glimpse_tempdir.%d/.glimpse_* %s\n", SYSTEM_MV, INDEX_DIR, pid, INDEX_DIR); */
	sprintf(s, "exec %s -f '%s/.glimpse_tempdir.%d/%s' '%s'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), pid, escapesinglequote(INDEX_FILE, es2), INDEX_DIR);
	system(s);
	sprintf(s, "exec %s -f '%s/.glimpse_tempdir.%d/%s' '%s'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), pid, P_TABLE, escapesinglequote(INDEX_DIR, es2));
	system(s);
	sprintf(s, "exec %s -f '%s/.glimpse_tempdir.%d/%s' '%s'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), pid, NAME_LIST, escapesinglequote(INDEX_DIR, es2));
	system(s);
	sprintf(s, "exec %s -f '%s/.glimpse_tempdir.%d/%s' '%s'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), pid, NAME_LIST_INDEX, escapesinglequote(INDEX_DIR, es2));
	system(s);
	sprintf(s, "exec %s -f '%s/.glimpse_tempdir.%d/%s' '%s'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), pid, NAME_HASH, escapesinglequote(INDEX_DIR, es2));
	system(s);
	sprintf(s, "exec %s -f '%s/.glimpse_tempdir.%d/%s' '%s'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), pid, NAME_HASH_INDEX, escapesinglequote(INDEX_DIR, es2));
	system(s);
	sprintf(s, "exec %s -f '%s/.glimpse_tempdir.%d/%s' '%s'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), pid, MINI_FILE, escapesinglequote(INDEX_DIR, es2));
	system(s);
	sprintf(s, "exec %s -f '%s/.glimpse_tempdir.%d/%s' '%s'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), pid, DEF_STAT_FILE, escapesinglequote(INDEX_DIR, es2));
	system(s);
	sprintf(s, "exec %s -f '%s/.glimpse_tempdir.%d/%s' '%s'\n", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), pid, ATTRIBUTE_FILE, escapesinglequote(INDEX_DIR, es2));
	system(s);
#endif
	sprintf(s, "%s/.glimpse_tempdir.%d", INDEX_DIR, pid);
	rmdir(s);
    }
    printf("\nBuilt new cast-dictionary in %s\n", INDEX_DIR);

#else	/*BUILDCAST*/

    if (AddToIndex || DeleteFromIndex || FastIndex) {
	/* Not handling byte level indices here for now */
	int	indextype = 0, indexnumber = OFF, structuredindex = OFF, recordlevelindex = OFF, temp_attr_num = 0, bytelevelindex = OFF;
	char	temp_rdelim[MAX_LINE_LEN];

	sprintf(s, "%s/%s", INDEX_DIR, INDEX_FILE);
	if (-1 == stat(s, &istbuf)) {
	    if (AddToIndex || DeleteFromIndex) {
		fprintf(stderr, "Cannot find previous index %s! Fresh indexing recommended\n", s);
		return usage(0);
	    }
	    file_num = 0;
	    file_id = 0;
	    part_num = 1;
	    goto fresh_indexing;
	}

	/* Find out existing index of words and partitions/filenumbers */
	if ((indextype = get_index_type(s, &indexnumber, &indextype, &structuredindex, temp_rdelim)) < 0) {
#if	0
	    fprintf(stderr, "Fresh indexing recommended: -a and -f are not supported with -b as yet\n");
	    exit(1);
	    /* we support it now */
#endif
	}
	if (structuredindex == -2) {
	    recordlevelindex = 1;
	    bytelevelindex = 1;
	}
	if (structuredindex <= 0) structuredindex = 0;
	else {
	    temp_attr_num = structuredindex;
	    structuredindex = 1;
	}
	file_num = part_num = 0;
	sprintf(s, "%s/%s", INDEX_DIR, NAME_LIST);
	file_num = get_array_of_lines(s, name_list, MaxNum24bPartition, 1);
	initialize_disable_list(file_num);
	initialize_data_structures(file_num);

	if (!indextype) {
		sprintf(s, "%s/%s", INDEX_DIR, P_TABLE);
		part_num = get_table(s, p_table, MAX_PARTITION, 1) - 1;	/* part_num INCLUDES last partition */
	}
	else merge_splits();

	/* Check for errors, Set OneFilePerBlock */
	if ( (file_num <= 0) || (!indextype && (part_num <= 0)) ) {
	    if (AddToIndex || DeleteFromIndex) {
		fprintf(stderr, "Cannot find previous glimpseindex files! Fresh indexing recommended\n");
		return usage(0);
	    }
	    file_num = 0;
	    file_id = 0;
	    part_num = 1;
	    my_free(disable_list);
	    disable_list = NULL;
	    goto fresh_indexing;
	}

	if (OneFilePerBlock && !indextype) {
	    fprintf(stderr, "Warning: ignoring option -o: using format of existing index\n");
	    OneFilePerBlock = 0;
	    ByteLevelIndex = 0;
	}
	else {
	    OneFilePerBlock = abs(indextype);
	    if (indextype < 0) ByteLevelIndex = ON;
	}
	if (StructuredIndex && !structuredindex) {
	    fprintf(stderr, "Warning: ignoring option -s: using format of existing index\n");
	    StructuredIndex = 0;
	    attr_num = 0;
	}
	else {
	    StructuredIndex = structuredindex;
	    attr_num = temp_attr_num;
	}
	if (RecordLevelIndex && !recordlevelindex) {
	    fprintf(stderr, "Warning: ignoring option -r: using format of existing index\n");
	    RecordLevelIndex = 0;
	    ByteLevelIndex = 0;
	    rdelim[0] = '\0';
	    old_rdelim[0] = '\0';
	    rdelim_len = 0;
	}
	else {
	    RecordLevelIndex = recordlevelindex;
	    strcpy(old_rdelim, temp_rdelim);
	    strcpy(rdelim, old_rdelim);
	    rdelim_len = strlen(rdelim);
	    preprocess_delimiter(rdelim, rdelim_len, rdelim, &rdelim_len);
	}

	/* Used in FastIndex for all existing files, used in AddToIndex/DeleteFromIndex if we are trying to add/remove an existing file */
	build_filename_hashtable(name_list, file_num);

#if	0
	/* Test if these are inverses of each other */
	save_data_structures();
	merge_splits();
#endif	/*0*/

	/*
	 * FastIndex: set disable-flag for unchanged files: remove AND
	 * disable non-existent files. Let hole remain in file-names/partitions.
	 */
	if (FastIndex) {
	    for (i=0; i<file_num; i++)
		if (-1 == my_stat(LIST_GET(name_list, i), &stbuf)) {
			remove_filename(i, -1);
		}
		else if (((stbuf.st_mode & S_IFMT) == S_IFREG) && (stbuf.st_ctime <= istbuf.st_ctime)) {
		    /* This is just used as a cache since exclude/include processing is not done here: see dir.c */
		    disable_list[block2index(i)] |= mask_int[i % (8*sizeof(int))];
		}
		else {
		    /* Can't do it for directories since files in it can be modified w/o date reflected in the directory. Same for symlinks. */
		    LIST_ADD(size_list, i, stbuf.st_size, int);
		    disable_list[block2index(i)] &= ~(mask_int[i % (8*sizeof(int))]);
		}
	}
	/*
	 * AddToIndex without FastIndex: disable all existing files, remove those that don't exist now.
	 * Out of old ones, only ADDED FILES are re-enabled: dir.c
	 */
	else if (AddToIndex) {
	    for (i=0; i<file_num; i++) {
		if (-1 == my_stat(LIST_GET(name_list, i), &stbuf)) {
		    remove_filename(i, -1);
		}
		else {
		    LIST_ADD(size_list, i, stbuf.st_size, int);	/* ONLY for proper statistics in save_data_structures() */
		    disable_list[block2index(i)] |= mask_int[i % (8*sizeof(int))];
		}
	    }
	}
	/* else: DeleteFromIndex without FastIndex: don't touch other files */

	old_file_num = file_num;
	destroy_data_structures();

	/* Put old/new files into partitions/filenumbers */
	if (-1 == oldpartition(argc, argv)) {
	    for(i=0;i<file_num;i++) {
#if	BG_DEBUG
		memory_usage -= (strlen(LIST_GET(name_list, i)) + 2);
#endif	/*BG_DEBUG*/
		if (LIST_GET(name_list, i) != NULL) {
			my_free(LIST_GET(name_list, i), 0);
			LIST_SUREGET(name_list, i) = NULL;
		}
	    }
	    file_num = 0;
	    file_id = 0;
	    for (i=0;i<part_num; i++) {
		p_table[i] = 0;
	    }
	    part_num = 1;
	    my_free(disable_list);
	    disable_list = NULL;
	    goto fresh_indexing;
	}

	/* Reindex all the files but use the file-names obtained with oldpartition() */
	if (cross_boundary(OneFilePerBlock, file_num)) {
	    my_free(disable_list);
	    disable_list = NULL;
	}

	initialize_data_structures(file_num);
	if (!DeleteFromIndex || FastIndex) build_index();
	if ((deletedlist = get_removed_indices()) == NULL) new_file_num = file_num;
	else if (PurgeIndex) new_file_num = purge_index();

#if	BG_DEBUG
	fprintf(LOGFILE, "Built indices in %s/%s\n", INDEX_DIR, INDEX_FILE);
#endif	/*BG_DEBUG*/
	goto docleanup;
    }

fresh_indexing:
    /* remove it to create space since it can be large: don't need for fresh indexing */
    sprintf(s, "%s/%s", INDEX_DIR, P_TABLE);
    unlink(s);
    /* These should be zeroed since they can confuse fsize and fsize_directory() */
    AddToIndex = 0;
    FastIndex = 0;
#if	BG_DEBUG
    fprintf(LOGFILE, "Commencing fresh indexing\n");
#endif	/*BG_DEBUG*/
    partition(argc, argv);
    destroy_filename_hashtable();
    initialize_data_structures(file_num);
    old_file_num = file_num;
    build_index();
#if	BG_DEBUG
    fprintf(LOGFILE, "\nBuilt indices in %s/%s\n", INDEX_DIR, INDEX_FILE);
#endif	/*BG_DEBUG*/

docleanup:
    cleanup();
    save_data_structures();
    destroy_filename_hashtable();
#if	BG_DEBUG
    fflush(LOGFILE);
    fclose(LOGFILE);
#endif	/*BG_DEBUG*/
    fflush(MESSAGEFILE);
    fclose(MESSAGEFILE);
    fflush(STATFILE);
    fclose(STATFILE);
    if (AddedMaxWordsMessage) printf("\nSome files contributed > %d words to the index: check %s\n", MAXWORDSPERFILE, DEF_MESSAGE_FILE);
    if (AddedMixedWordsMessage) printf("Some files had numerals in > %d%% of the indexed words: check %s\n", NUMERICWORDPERCENT, DEF_MESSAGE_FILE);

    printf("\nIndex-directory: \"%s\"\nGlimpse-files created here:\n", INDEX_DIR);
    chdir(INDEX_DIR);
    sprintf(s, "exec %s -l .glimpse_* > %s/%d\n", SYSTEM_LS, TEMP_DIR,pid);
    system(s);
    sprintf(s, "%s/%d", TEMP_DIR,pid);
    if ((tmpfp = fopen(s, "r")) != NULL) {
	memset(tmpbuf, '\0', 1024);
	while(fgets(tmpbuf, 1024, tmpfp) != NULL) fputs(tmpbuf, stdout);
	fflush(tmpfp);
	fclose(tmpfp);
	unlink(s);
    }
    else fprintf(stderr, "cannot open %s to `cat': check %s for .glimpse - files\n", s, INDEX_DIR);
#endif	/*BUILDCAST*/

    if (!ATLEASTONEFILE) exit(1);
    return 0;
}

cleanup()
{
    char s[MAX_LINE_LEN];

    sprintf(s, "%s/%s", INDEX_DIR, I1);
    unlink(s);
    sprintf(s, "%s/%s", INDEX_DIR, I2);
    unlink(s);
    sprintf(s, "%s/%s", INDEX_DIR, I3);
    unlink(s);
    sprintf(s, "%s/%s", INDEX_DIR, O1);
    unlink(s);
    sprintf(s, "%s/%s", INDEX_DIR, O2);
    unlink(s);
    sprintf(s, "%s/%s", INDEX_DIR, O3);
    unlink(s);
    sprintf(s, "%s/.glimpse_apply.%d", INDEX_DIR, getpid());
    unlink(s);
    return(1); /* We return 1, because function expect int as return type */
}

#if	!BUILDCAST
usage(flag)
int	flag;
{
	if (flag) fprintf(stderr, "\nThis is glimpseindex version %s, %s.\n\n", GLIMPSE_VERSION, GLIMPSE_DATE);
	fprintf(stderr, "usage: %s [-help] [-a] [-d] [-f] [-i] [-n [X]] [-o] [-r delim] [-s] [-t] [-w X] [-B] [-F] [-H DIR] [-I] [-M X] [-R] [-S X] [-T] [-V] NAMES\n", IProgname);
	fprintf(stderr, "List of options (see %s for more details):\n", GLIMPSE_URL);

	fprintf(stderr, "-help: outputs this menu\n");
	fprintf(stderr, "-a: add given files/directories to an existing index\n");
	fprintf(stderr, "-b: build a (large) byte-level index \n");
	fprintf(stderr, "-B: use a hash table that is 4 times bigger (256k entries instead of 64K) \n");
	fprintf(stderr, "-d NAMES: delete (file or directory) NAMES from an existing index\n");
	fprintf(stderr, "-D NAMES: delete NAMES from the list of files (but not from the index!)\n");
	fprintf(stderr, "-E: do not run a check on file types\n");
	fprintf(stderr, "-f: incremental indexing (add all newly modified files)\n");
	fprintf(stderr, "-F: the list of files to index is obtained from standard input\n");
	fprintf(stderr, "-h: generates some hash-tables for WebGlimpse\n");
	fprintf(stderr, "-H DIR: the index is put in directory DIR\n");
	fprintf(stderr, "-i: make .glimpse_include take precedence over .glimpse_exclude\n");
	fprintf(stderr, "-I: output the list of files that would be indexed (but don't index)\n");
	fprintf(stderr, "-M X: use X MBytes of memory for temporary tables\n");
        fprintf(stderr, "-n [X]: index numbers as well as words; warn (into .glimpse_messages)\n\tif file adds > X%% numeric words: default is %d\n", DEF_NUMERIC_WORD_PERCENT);
	fprintf(stderr, "-o: build a small (rather than tiny) size index (the recommended option!)\n");
	/*fprintf(stderr, "-O: when using -r option, store byte offset of each record,\n\tinstead of the record number, for faster access\n");*/
	fprintf(stderr, "-r delim: build an index at the granularity of delimiter `delim'\n\tto do booleans by reading ONLY the index\n");
	fprintf(stderr, "-R: recompute .glimpse_filenames_index from .glimpse_filenames if it changes\n");
	fprintf(stderr, "-s: build index to support structured (Harvest SOIF type) queries\n");
	fprintf(stderr, "-S X: adjust the size of the stop list\n");
	fprintf(stderr, "-t: sort the indexed files by date and time (most recent first)\n");
	fprintf(stderr, "-T: build .glimpse_turbo for very fast search with -i -w in glimpse\n");
	fprintf(stderr, "-U: there is extra information after filenames: works only with -F\n");
	fprintf(stderr, "-w X: warn (into .glimpse_messages) if a file adds >= X words to the index\n");
	fprintf(stderr, "-X: extract titles of all documents with .html, .htm, .shtm, .shtml suffix\n");
	fprintf(stderr, "-z: customizable filtering using .glimpse_filters \n");

	fprintf(stderr, "\n");
	fprintf(stderr, "For questions about glimpse, please contact: `%s'\n", GLIMPSE_EMAIL);
	exit(1);
}
#else	/*!BUILDCAST*/
usage(flag)
int	flag;
{
	if (flag) fprintf(stderr, "\nThis is buildcast version %s, %s.\n\n", GLIMPSE_VERSION, GLIMPSE_DATE);
	fprintf(stderr, "usage: %s [-help] [-t] [-i] [-l] [-n [X]] [-w X] [-C] [-E] [-F] [-H DIR] [-V] NAMES\n", IProgname);
        fprintf(stderr, "summary of frequently used options\n(for a more detailed listing see 'man cast'):\n");
	fprintf(stderr, "-help: output this menu\n");
        fprintf(stderr, "-n [X]: index numbers as well as words; warn (into .glimpse_messages)\n\tif file adds > X%% numeric words: default is %d\n", DEF_NUMERIC_WORD_PERCENT);
        fprintf(stderr, "-w X: warn if a file adds > X words to the index\n");
	fprintf(stderr, "-C: compress files with the new dictionary after building it\n");
	fprintf(stderr, "-E: build cast dictionary using existing compressed files only\n");
	fprintf(stderr, "-F: expect filenames on stdin (useful for pipelining)\n");
        fprintf(stderr, "-H DIR: .glimpse-files should be in directory DIR: default is '~'\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "For questions about glimpse, please contact: `%s'\n", GLIMPSE_EMAIL);
	exit(1);
	return(1); /* We type return 1, because function expect int as return type */
}
#endif	/*!BUILDCAST*/
