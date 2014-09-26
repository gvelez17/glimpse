/* Copyright (c) 1994 Sun Wu, Udi Manber, Burra Gopal.  All Rights Reserved. */
/* ./glimpse/index/partition.c */
#include "glimpse.h"
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>

extern int BigFilenameHashTable;
extern int DeleteFromIndex;
extern int FastIndex;
extern int FilenamesOnStdin;
extern char INDEX_DIR[MAX_LINE_LEN];
extern char sync_path[MAX_LINE_LEN];
extern int file_num;	/* the number of files */
extern int new_file_num; /* the new number of files after purging some from index */
extern char **name_list[MAXNUM_INDIRECT]; /* to store the file names */
extern int  *size_list[MAXNUM_INDIRECT]; /* store size of each file */
extern int  p_table[MAX_PARTITION];	/* partition table, the i-th partition begins at p_table[i] and ends at p_tables[i+1] */
extern int  p_size_list[MAX_PARTITION];	/* sum of the sizes of the files in each partition */
extern int  part_num;	/* number of partitions, 1 initially since partition # 0 is not accessed */

extern int built_filename_hashtable;
extern name_hashelement *name_hashtable[MAX_64K_HASH];
extern int total_size; /* total size of the directory */
extern int total_deleted; /* number of files being deleted */
int  part_size=DEFAULT_PART_SIZE;	/* partition size */
int  new_partition;
int  files_per_partition;
int  files_in_partition;
int  ATLEASTONEFILE = 0;
extern int errno;

char patbuf[MAX_PAT];
extern unsigned char *src_index_buf;
extern unsigned char *dest_index_buf;
extern int REAL_PARTITION, REAL_INDEX_BUF, MAX_ALL_INDEX, FILEMASK_SIZE;
extern int memory_usage;
extern struct indices	*deletedlist;

extern  FILE	*TIMEFILE;
extern  FILE	*STATFILE;
extern  FILE	*MESSAGEFILE;

extern struct stat excstbuf;
extern struct stat incstbuf;

extern int GenerateHash;
extern int KeepFilenames;
extern int OneFilePerBlock;
extern int ByteLevelIndex;
extern int RecordLevelIndex;
extern int rdelim_len;
extern char rdelim[MAX_LINE_LEN];
extern char old_rdelim[MAX_LINE_LEN];
extern int StructuredIndex;
extern int attr_num;
extern char INDEX_DIR[MAX_LINE_LEN];
extern int AddToIndex;
extern int IndexableFile;
extern int BuildTurbo;
extern int SortByTime;

char *exin_argv[8];
int exin_argc;
char current_dir_buf[2*MAX_LINE_LEN + 4];	/* must have space to store pattern after directory name */
unsigned char dummypat[MAX_PAT];
int dummylen;
FILE *dummyout;

partition(dir_num, dir_name)
char **dir_name;
int  dir_num;
{
    int num_pat=0;
    int num_inc=0;
    int len;
    long thetime;
    long prevtime;
    int theindex;
    int firsttime = 1;
    int xx;
    struct timeval tv;
    FILE *tmp_TIMEFILE;
    FILE *index_TIMEFILE;
    int ret;
    char **temp_name_list;
    int *temp_size_list;
    int temp_file_num;
    char S[MAX_LINE_LEN], S1[MAX_LINE_LEN], es1[MAX_LINE_LEN], es2[MAX_LINE_LEN], es3[MAX_LINE_LEN];
    int  pat_len[MAX_EXCLUSIVE];
    int  inc_len[MAX_EXCLUSIVE];
    CHAR *inc[MAX_INCLUSIVE];	/* store the patterns used to mask in files */
    CHAR *pat[MAX_EXCLUSIVE];     /* store the patterns that are used to
				     mask out those files that are not to
				     be indexed  */
    int MinPartNum; 		/* minimum number of partitions */
    int i=0, j;
    int subtotal=0;
    int pdx = 0; 			/* index pointer for p_table */
    FILE *patfile; 	/* file descriptor for prohibit pattern file */
    FILE *incfile;	/* file descriptor for include pattern file */
    char *current_dir;	/* must have '\n' before directory name */
    char s[MAX_LINE_LEN];
    char working_dir[MAX_LINE_LEN];
    struct stat sbuf;

    current_dir_buf[0] = '\n';
    current_dir_buf[1] = '\0';
    current_dir = &current_dir_buf[1];
    /* if (IndexableFile) goto directlytofsize; */

    if ((dummyout = fopen("/dev/null", "w")) == NULL) return -1;
    exin_argv[0] = "glimpseindex";
    exin_argv[1] = "dummypat";
    exin_argc = 2;
    if ((dummylen = memagrep_init(exin_argc, exin_argv, MAX_PAT, dummypat)) <= 0) return -1;	/* exclude/include pattern search */

    sprintf(s, "%s/%s", INDEX_DIR, PROHIBIT_LIST);
    patfile = fopen(s, "r");
    if(patfile == NULL) {
	/* fprintf(stderr, "can't open exclude-pattern file\n"); -- no need! */
	num_pat = 0;
    }
    else {
	while((num_pat < MAX_EXCLUSIVE) && fgets(patbuf, MAX_PAT, patfile)) {
		if ((len = strlen(patbuf)) < 1) continue;
		patbuf[len-1] = '\0';
		if ((pat_len[num_pat] = convert2agrepregexp(patbuf, len-1)) == 0) continue;
		pat[num_pat++] = (unsigned char *) strdup(patbuf);
	}
	fclose(patfile);
    }
#if	0
    printf("num_pat %d\n", num_pat);
    for(i=0; i<num_pat; i++) printf("len=%d pat=%s\n", pat_len[i], pat[i]);
    printf("memagrep=%d\n", memagrep_search(-pat_len[0], pat[0], 17, "\n.glimpse_index\nasdfk", 0, stdout));
#endif

    sprintf(s, "%s/%s", INDEX_DIR, INCLUDE_LIST);
    incfile = fopen(s, "r");
    if(incfile == NULL) {
	/* fprintf(stderr, "can't open include-pattern file\n"); -- no need! */
	num_inc = 0;
    }
    else {
	while((num_inc < MAX_INCLUSIVE) && fgets(patbuf, MAX_PAT, incfile)) {
		if ((len = strlen(patbuf)) < 1) continue;
		patbuf[len-1] = '\0';
		if ((inc_len[num_inc] = convert2agrepregexp(patbuf, len-1)) == 0) continue;
		inc[num_inc++] = (unsigned char *) strdup(patbuf);
	}
	fclose(incfile);
    }
#if	0
    printf("num_inc %d\n", num_inc);
    for(i=0; i<num_inc; i++) printf("len=%d inc=%s\n", inc_len[i], inc[i]);
#endif

#ifdef	SW_DEBUG
    printf("dir_num = %d", dir_num-1);
#endif
directlytofsize:
    if ((dir_num <= 1) && (FilenamesOnStdin)) while (fgets(current_dir, MAX_LINE_LEN, stdin) == current_dir) {
	current_dir[strlen(current_dir)-1] = '\0';	/* overwrite \n with \0 */

	/* Get absolute path name of the directory or file being indexed */
	if (-1 == my_stat(current_dir, &sbuf)) {
		fprintf(stderr, "permission denied or non-existent: %s\n", current_dir);
		continue;
	}
	if ((S_ISDIR(sbuf.st_mode)) && (current_dir[0] != '/')) {
	    getcwd(working_dir, MAX_LINE_LEN - 1);
	    if (-1 == chdir(current_dir)) {
		fprintf(stderr, "Cannot chdir to %s\n", current_dir);
		continue;
	    }
	    getcwd(current_dir, MAX_LINE_LEN - 1);
	    chdir(working_dir);
	}

	if (!IndexableFile) printf("Indexing \"%s\" ...\n", current_dir);
    	fsize(current_dir, pat, pat_len, num_pat, inc, inc_len, num_inc, 0); /* the file names will be in name_list[]: NOT TOP LEVEL!!! Mar/11/96 */
    }
    else for(i=1; i<dir_num; i++) {
    	strcpy(current_dir, dir_name[i]);

	/* Get absolute path name of the directory or file being indexed */
	if (-1 == my_stat(current_dir, &sbuf)) {
		fprintf(stderr, "permission denied or non-existent: %s\n", current_dir);
		continue;
	}
	if ((S_ISDIR(sbuf.st_mode)) && (current_dir[0] != '/')) {
	    getcwd(working_dir, MAX_LINE_LEN - 1);
	    if (-1 == chdir(current_dir)) {
		fprintf(stderr, "Cannot chdir to %s\n", current_dir);
		continue;
	    }
	    getcwd(current_dir, MAX_LINE_LEN - 1);
	    chdir(working_dir);
	}

	if (!IndexableFile) {
		if (!DeleteFromIndex) printf("Indexing \"%s\" ...\n", current_dir);
	}
    	fsize(current_dir, pat, pat_len, num_pat, inc, inc_len, num_inc, 1); /* the file names will be in name_list[] */
    }
    if (IndexableFile) return 0;

    /*
     * If -t option is set, we must sort .glimpse_filenames (i.e., name_list and size_list) according to the most recent modification date.
     * The file-number VS its modification date are already available in .glimpse_filetimes which is created in main() and filled in fsize().
     */
    if (SortByTime && (file_num > 0)) {
	fflush(TIMEFILE);
	fclose(TIMEFILE);

#if	USESORT_Z_OPTION
#if	DONTUSESORT_T_OPTION || SFS_COMPAT
	sprintf(S, "exec %s -n -r -z %d '%s/%s' > '%s/%s.tmp'\n", SYSTEM_SORT, maxsortlinelen, escapesinglequote(INDEX_DIR, es1), DEF_TIME_FILE, escapesinglequote(INDEX_DIR, es2), DEF_TIME_FILE);
#else
	sprintf(S, "exec %s -n -r -T '%s' -z %d '%s/%s' > '%s/%s.tmp'\n", SYSTEM_SORT, escapesinglequote(INDEX_DIR, es1), maxsortlinelen, escapesinglequote(INDEX_DIR, es2), DEF_TIME_FILE, escapesinglequote(INDEX_DIR, es3), DEF_TIME_FILE);
#endif
#else
#if	DONTUSESORT_T_OPTION || SFS_COMPAT
	sprintf(S, "exec %s -n -r '%s/%s' > '%s/%s.tmp'\n", SYSTEM_SORT, escapesinglequote(INDEX_DIR, es1), DEF_TIME_FILE, escapesinglequote(INDEX_DIR, es2), DEF_TIME_FILE);
#else
	sprintf(S, "exec %s -n -r -T '%s' '%s/%s' > '%s/%s.tmp'\n", SYSTEM_SORT, escapesinglequote(INDEX_DIR, es1), escapesinglequote(INDEX_DIR, es2), DEF_TIME_FILE, escapesinglequote(INDEX_DIR, es3), DEF_TIME_FILE);
#endif
#endif

#ifdef BG_DEBUG
	printf("%s", S);
#endif
	if((ret=system(S)) != 0) {
	    sprintf(S1, "system('%s') failed at:\n\t File=%s, Line=%d, Errno=%d", S, __FILE__, __LINE__, errno);
	    perror(S1);
	    fprintf(stderr, "Please try to run the program again\n(If there's no memory, increase the swap area / don't use -M and -B options)\n");
	    sprintf(S, "%s/%s", INDEX_DIR, DEF_TIME_FILE);
	    unlink(S);
	    exit(2);
	}

	sprintf(S, "%s/%s.tmp", INDEX_DIR, DEF_TIME_FILE);
	if ((tmp_TIMEFILE = fopen(S, "r")) == NULL) {
	    fprintf(stderr, "can't open %s for reading\n", S);
	    unlink(S);
	    exit(2);
	}

	sprintf(S, "%s/%s", INDEX_DIR, DEF_TIME_FILE);
	if ((TIMEFILE = fopen(S, "w")) == NULL) {
	    fprintf(stderr, "can't open %s for writing\n", S);
	    unlink(S);
	    exit(2);
	}

	sprintf(S, "%s/%s.index", INDEX_DIR, DEF_TIME_FILE);
	if ((index_TIMEFILE = fopen(S, "w")) == NULL) {
	    fprintf(stderr, "can't open %s for writing\n", S);
	    unlink(S);
	    exit(2);
	}

	/* Get the sorted times from .glimpse_times.tmp; dump the exact times for each file in .glimpse_times; dump per-day file# in .glimpse_times.index */
	gettimeofday(&tv, NULL);
	temp_file_num = 0;
	temp_name_list = (char **)my_malloc(sizeof(char *) * file_num);
	memset(temp_name_list, '\0', sizeof(char *) * file_num);
	temp_size_list = (int *)my_malloc(sizeof(int) * file_num);
	memset(temp_size_list, '\0', sizeof(int) * file_num);
	prevtime = tv.tv_sec;
	while (fscanf(tmp_TIMEFILE, "%ld %d", &thetime, &theindex) == 2) {
	    temp_name_list[temp_file_num] = LIST_GET(name_list, theindex);
	    temp_size_list[temp_file_num] = LIST_GET(size_list, theindex);
	    for (xx=0; xx<sizeof(long); xx++) fputc((thetime & ((0xff) << (8*(sizeof(long) - xx - 1))))>>(8*(sizeof(long) - xx - 1)), TIMEFILE);
	    /* fprintf(TIMEFILE, "%d %d\n", thetime, (prevtime - thetime)/86400); */
	    if (firsttime) {
		for (i=0; i<(prevtime - thetime + 86399)/86400; i++) {
		    for (xx=0; xx<sizeof(int); xx++)
			fputc((temp_file_num & ((0xff) << (8*(sizeof(int) - xx - 1))))>>(8*(sizeof(int) - xx - 1)), index_TIMEFILE);
		    /* fprintf(index_TIMEFILE, "%d\n", temp_file_num); */
		}
	    }
	    else {
		for (i=0; i<(prevtime - thetime)/86400; i++) {
		    for (xx=0; xx<sizeof(int); xx++)
			fputc((temp_file_num & ((0xff) << (8*(sizeof(int) - xx - 1))))>>(8*(sizeof(int) - xx - 1)), index_TIMEFILE);
		    /* fprintf(index_TIMEFILE, "%d\n", temp_file_num); */
		}
	    }
	    temp_file_num ++;
	    if (!firsttime) prevtime -= i*86400;
	    else if (i>0) prevtime -= (i-1)*86400;
	    firsttime = 0;
	}
	if (temp_file_num != file_num) {
	    fprintf(stderr, "error in sort: File=%s, Line=%d\n", __FILE__, __LINE__);
	    exit(2);
	}

	/* Change the lists to be sorted now; free temporary lists */
	for (i=0; i<temp_file_num; i++) {
	    LIST_SUREGET(name_list, i) = temp_name_list[i];
	    LIST_SUREGET(size_list, i) = temp_size_list[i];
	}
	my_free(temp_name_list, sizeof(char *) * file_num);
	my_free(temp_size_list, sizeof(int) * file_num);

	fclose(tmp_TIMEFILE);
	fflush(TIMEFILE);
	fclose(TIMEFILE);
	fflush(index_TIMEFILE);
	fclose(index_TIMEFILE);
	sprintf(S, "%s/%s.tmp", INDEX_DIR, DEF_TIME_FILE);
	unlink(S);
    }

    for(i=0; i<file_num; i++) total_size += LIST_GET(size_list, i);
    for(i=0; i<file_num; i++) if (LIST_GET(name_list, i) == NULL) total_deleted ++;
    if (DeleteFromIndex) {
	if (total_size <= 0) {
	    fprintf(STATFILE, "#of files being deleted = %d, Total #of files = %d\n", total_deleted, file_num - total_deleted);
	    printf("\n#of files being deleted = %d, Total #of files = %d\n", total_deleted, file_num - total_deleted);	/* the only output the user sees */
	}
	else {
	    fprintf(STATFILE, "Size of files being indexed = %d B, #of files being deleted = %d, Total #of files = %d\n", total_size, total_deleted, file_num - total_deleted);
	    printf("\nSize of files being indexed = %d B, #of files being deleted = %d, Total #of files = %d\n", total_size, total_deleted, file_num - total_deleted);	/* the only output the user sees */
	}
    }
    else {
	fprintf(STATFILE, "Size of files being indexed = %d B, Total #of files = %d\n", total_size, file_num);
	printf("\nSize of files being indexed = %d B, Total #of files = %d\n", total_size, file_num);	/* the only output the user sees */
    }

#ifdef	SW_DEBUG
    for (i=0; i<file_num; i++)
	printf("name_list[%d] = %s, size=%d\n", i, LIST_GET(name_list, i), LIST_GET(size_list, i));
#endif	/*SW_DEBUG*/

    for (i=0; i<num_inc; i++) {
#if	BG_DEBUG
	memory_usage -= strlen(inc) + 2;
#endif	/*BG_DEBUG*/
	my_free(inc[i], 0);
    }
    for (i=0; i<num_pat; i++) {
#if	BG_DEBUG
	memory_usage -= strlen(pat) + 2;
#endif	/*BG_DEBUG*/
	my_free(pat[i], 0);
    }

    /* Life (algorithm) is much simpler, but encode/decode (I/O) is more complex: the p_table is irrelevant */
    if (OneFilePerBlock)
	return 0;

    /* Now put the files into partitions */
    i=0;
    part_size = total_size / MaxNumPartition;
    if (part_size <= 0) part_size = total_size;
    LIST_ADD(size_list, file_num, part_size, int);
    if (file_num / 2 <= 1) {
	p_table[0] = 0;
	p_table[1] = 0;
	p_table[2] = file_num;
	part_num = 2;
	return 0;
    }

    MinPartNum = (200 < file_num/2)? 200 : file_num/2;
    while(part_num < MinPartNum) {
        pdx = 0;
	i = 0;
        subtotal = 0;
        while ((i < file_num) && (pdx < MAX_PARTITION)) {
	    if((pdx == 0) || (pdx == '\n')) {
		/*
		 * So that there cannot be a partition #'\n' and a '\n' can indicate
		 * the end of the list of partition#s after the WORD_END_MARK.
		 * Also, partition#0 is not accessed so that sort does not
		 * ignore the partitions after partition# 0!
		 */
		p_table[pdx++] = i;
		continue;
	    }
	    p_table[pdx++] = i;
            while(subtotal < part_size) {
                subtotal += LIST_GET(size_list, i);
		i++;
            }
#ifdef	SW_DEBUG
	    printf("pdx=%d part_num=%d i=%d subtotal=%d\n", pdx, part_num, i, subtotal);
#endif
            subtotal = 0;
        }
	part_num = pdx;
#if	0
	printf("part_num = %d part_size = %d\n", part_num, part_size);
#endif
	part_size = part_size * 0.9;
        LIST_ADD(size_list, file_num, part_size, int);
    }
    p_table[pdx] = file_num;

    /* Calculate partition sizes for later output into statistics */
    for (i=0; i<= part_num; i++)
	for (j = p_table[i]; j<p_table[i+1]; j++)
	    p_size_list[i] += LIST_GET(size_list, j);

    return 0;
}

int printed_warning = 0;

/*
 * Difference from above: does not build a new partition table:
 * adds to the existing one (see glimpse.c, options -a and -f).
 * -- added on dec 7th '93
 */
oldpartition(dir_num, dir_name)
char **dir_name;
int  dir_num;
{
    int num_pat=0;
    int num_inc=0;
    int len;
    int  pat_len[MAX_EXCLUSIVE];
    int  inc_len[MAX_EXCLUSIVE];
    CHAR *inc[MAX_INCLUSIVE];	/* store the patterns used to mask in files */
    CHAR *pat[MAX_EXCLUSIVE];     /* store the patterns that are used to
				     mask out those files that are not to
				     be indexed  */
    int i=0;
    FILE *patfile; 	/* file descriptor for prohibit pattern file */
    FILE *incfile;	/* file descriptor for include pattern file */
    char *current_dir;	/* must have '\n' before directory name */
    char s[MAX_LINE_LEN];
    char working_dir[MAX_LINE_LEN];
    struct stat sbuf;

    current_dir_buf[0] = '\n';
    current_dir_buf[1] = '\0';
    current_dir = &current_dir_buf[1];

    if ((dummyout = fopen("/dev/null", "w")) == NULL) return -1;
    exin_argv[0] = "glimpseindex";
    exin_argv[1] = "dummypat";
    exin_argc = 2;
    if ((dummylen = memagrep_init(exin_argc, exin_argv, MAX_PAT, dummypat)) <= 0) return -1;	/* exclude/include pattern search */

    sprintf(s, "%s/%s", INDEX_DIR, PROHIBIT_LIST);
    patfile = fopen(s, "r");
    if(patfile == NULL) {
	/* fprintf(stderr, "can't open exclude-pattern file\n"); -- no need! */
	num_pat = 0;
    }
    else {
	stat(s, &excstbuf);
	while((num_pat < MAX_EXCLUSIVE) && fgets(patbuf, MAX_PAT, patfile)) {
		if ((len = strlen(patbuf)) < 1) continue;
		patbuf[len-1] = '\0';
		if ((pat_len[num_pat] = convert2agrepregexp(patbuf, len-1)) == 0) continue;
		pat[num_pat++] = (unsigned char *) strdup(patbuf);
	}
	fclose(patfile);
    }
#if	0
    printf("num_pat %d\n", num_pat);
    for(i=0; i<num_pat; i++) printf("len=%d pat=%s\n", pat_len[i], pat[i]);
#endif

    sprintf(s, "%s/%s", INDEX_DIR, INCLUDE_LIST);
    incfile = fopen(s, "r");
    if(incfile == NULL) {
	/* fprintf(stderr, "can't open include-pattern file\n"); -- no need! */
	num_inc = 0;
    }
    else {
	stat(s, &incstbuf);
	while((num_inc < MAX_INCLUSIVE) && fgets(patbuf, MAX_PAT, incfile)) {
		if ((len = strlen(patbuf)) < 1) continue;
		patbuf[len-1] = '\0';
		if ((inc_len[num_inc] = convert2agrepregexp(patbuf, len-1)) == 0) continue;
		inc[num_inc++] = (unsigned char *) strdup(patbuf);
	}
	fclose(incfile);
    }
#if	0
    printf("num_inc %d\n", num_inc);
    for(i=0; i<num_inc; i++) printf("len=%d inc=%s\n", inc_len[i], inc[i]);
#endif

#ifdef	SW_DEBUG
    printf("dir_num = %d part_num = %d", dir_num-1, part_num);
#endif

    if (!OneFilePerBlock) {	/* Worry about partitions */
	files_per_partition = ((file_num - 1)/part_num) + 1;	/* approximate only but gives a fair idea... */
	files_in_partition = 0;
	new_partition = part_num;	/* part_num itself is guaranteed to be <= MaxNumPartition */

	if (new_partition + 1 > MaxNumPartition) {
	    printed_warning = 1;
	    if (AddToIndex) {
		fprintf(MESSAGEFILE, "Warning: partition-table overflow! Fresh indexing recommended.\n");
	    }
	    else {
		fprintf(MESSAGEFILE, "Warning: partition-table overflow! Commencing fresh indexing...\n");
		return partition(dir_num, dir_name);
	    }
	}
    }

    if ((dir_num <= 1) && FilenamesOnStdin) while (fgets(current_dir, MAX_LINE_LEN, stdin) == current_dir) {
	current_dir[strlen(current_dir)-1] = '\0';	/* overwrite \n with \0 */

	/* Get absolute path name of the directory or file being indexed */
	if (-1 == my_stat(current_dir, &sbuf)) {
		fprintf(stderr, "permission denied or non-existent: %s\n", current_dir);
		continue;
	}
	if ((S_ISDIR(sbuf.st_mode)) && (current_dir[0] != '/')) {
	    getcwd(working_dir, MAX_LINE_LEN - 1);
	    if (-1 == chdir(current_dir)) {
		fprintf(stderr, "Cannot chdir to %s\n", current_dir);
		continue;
	    }
	    getcwd(current_dir, MAX_LINE_LEN - 1);
	    chdir(working_dir);
	}

	if (!DeleteFromIndex) printf("Indexing \"%s\" ...\n", current_dir);
    	fsize(current_dir, pat, pat_len, num_pat, inc, inc_len, num_inc, 0); /* the file names will be in name_list[]: NOT TOP LEVEL!!! Mar/11/96 */
    }
    else for(i=1; i<dir_num; i++) {
    	strcpy(current_dir, dir_name[i]);

	/* Get absolute path name of the directory or file being indexed */
	if (-1 == my_stat(current_dir, &sbuf)) {
		fprintf(stderr, "permission denied or non-existent: %s\n", current_dir);
		continue;
	}
	if ((S_ISDIR(sbuf.st_mode)) && (current_dir[0] != '/')) {
	    getcwd(working_dir, MAX_LINE_LEN - 1);
	    if (-1 == chdir(current_dir)) {
		fprintf(stderr, "Cannot chdir to %s\n", current_dir);
		continue;
	    }
	    getcwd(current_dir, MAX_LINE_LEN - 1);
	    chdir(working_dir);
	}

	if (!DeleteFromIndex) printf("Indexing \"%s\" ...\n", current_dir);
    	if (-1 == fsize(current_dir, pat, pat_len, num_pat, inc, inc_len, num_inc, 1)) { /* the file names will be in name_list[] */
	    return -1;
	}
    }

    if (!OneFilePerBlock) {
	p_table[new_partition] = file_num;
	part_num = new_partition;
    }

    for (i=0; i<num_inc; i++) {
#if	BG_DEBUG
	memory_usage -= strlen(inc) + 2;
#endif	/*BG_DEBUG*/
	my_free(inc[i], 0);
    }
    for (i=0; i<num_pat; i++) {
#if	BG_DEBUG
	memory_usage -= strlen(pat) + 2;
#endif	/*BG_DEBUG*/
	my_free(pat[i], 0);
    }

    for(i=0; i<file_num; i++) total_size += LIST_GET(size_list, i);
    for(i=0; i<file_num; i++) if (LIST_GET(name_list, i) == NULL) total_deleted ++;
    if (DeleteFromIndex) {
	if (total_size <= 0) {
	    fprintf(STATFILE, "#of files being deleted = %d, Total #of files = %d\n", total_deleted, file_num - total_deleted);
	    printf("\n#of files being deleted = %d, Total #of files = %d\n", total_deleted, file_num - total_deleted);	/* the only output the user sees */
	}
	else {
	    fprintf(STATFILE, "Size of files being indexed = %d B, #of files being deleted = %d, Total #of files = %d\n", total_size, total_deleted, file_num - total_deleted);
	    printf("\nSize of files being indexed = %d B, #of files being deleted = %d, Total #of files = %d\n", total_size, total_deleted, file_num - total_deleted);	/* the only output the user sees */
	}
    }
    else {
	fprintf(STATFILE, "Size of files being indexed = %d B, Total #of files = %d\n", total_size, file_num);
	printf("\nSize of files being indexed = %d B, Total #of files = %d\n", total_size, file_num);	/* the only output the user sees */
    }

#ifdef	SW_DEBUG
    for (i=0; i<file_num; i++)
	printf("name_list[%d] = %s, size=%d\n", i, LIST_GET(name_list, i), LIST_GET(size_list, i));
#endif	/*SW_DEBUG*/

    return 0;
}

save_data_structures()
{
    int	i, hashtablesize;
    char s[MAX_LINE_LEN], s1[MAX_LINE_LEN], es1[MAX_LINE_LEN], es2[MAX_LINE_LEN], es3[MAX_LINE_LEN], temp_rdelim[MAX_LINE_LEN];
    FILE *f_out;
    FILE *p_out;
    int j;
    unsigned char c;
    FILE *i_in;
    FILE *i_out;
    int offset, index;
    char indexnumberbuf[256];
    int	onefileperblock, structuredindex;
    int name_list_size = file_num;
    name_hashelement *e;

    ATLEASTONEFILE = 0;
#if	0
	fflush(stdout);
	printf("BEFORE SAVE_DATA_STRUCTURES:\n");
	sprintf(s, "exec %s -lg .glimpse_*", SYSTEM_LS);
	system(s);
	sprintf(s, "exec %s .glimpse_index", SYSTEM_HEAD);
	system(s);
	getchar();
#endif	/*0*/

    if ((new_file_num >= 0) && (new_file_num <= file_num)) file_num = new_file_num;	/* only if purge_index() was called: -f/-a/-d only */
    /* Dump attributes */
    if (StructuredIndex && (attr_num > 0)) {
	int	ret;
	sprintf(s, "%s/%s", INDEX_DIR, ATTRIBUTE_FILE);
	if (-1 == (ret = attr_dump_names(s))) {
	    fprintf(stderr, "can't open %s for writing\n", s);
	    exit(2);
	}
    }

    /* Dump partition table; change index if necessary */
    sprintf(s, "%s/%s", INDEX_DIR, P_TABLE);
    if((p_out = fopen(s, "w")) == NULL) {
	fprintf(stderr, "can't open for writing: %s\n", s);
	exit(2);
    }
    if (!OneFilePerBlock) {
#ifdef SW_DEBUG
	printf("part_num = %d, part_size = %d\n", part_num, part_size);
#endif
	for(i=0; i<=part_num; i++) {
	    /* Assumes sizeof(int) is 32bits, which is true even for ALPHA */
	    putc((p_table[i] & 0xff000000) >> 24, p_out);
	    putc((p_table[i] & 0x00ff0000) >> 16, p_out);
	    putc((p_table[i] & 0x0000ff00) >> 8, p_out);
	    if (putc((p_table[i] & 0x000000ff), p_out) == EOF) {
		fprintf(stderr, "Error: write failed at %s:%d\n", __FILE__, __LINE__);
		exit(2);
	    }
	    if (i==part_num) break;
	    if (p_table[i] == p_table[i+1]) {
		fprintf(STATFILE, "part_num = %d, files = none, part_size = 0\n",i);
		continue;
	    }
	    fprintf(STATFILE, "part_num = %d, files = %d .. %d, part_size = %d\n",
		i, p_table[i], p_table[i+1] - 1, p_size_list[i]);
	}

	if (StructuredIndex) {	/* check if we can reduce default 2B attributeids to smaller ones */
	    sprintf(s, "%s/.glimpse_split.%d", INDEX_DIR, getpid());
	    if((i_out = fopen(s, "w")) == NULL) {
		fprintf(stderr, "can't open %s for writing\n", s);
		exit(2);
	    }
	    sprintf(s, "%s/%s", INDEX_DIR, INDEX_FILE);
	    if((i_in = fopen(s, "r")) == NULL) {
		fprintf(stderr, "can't open %s for reading\n", s);
		exit(2);
	    }

	    /* modified the original in glimpse's main.c */
	    fgets(indexnumberbuf, 256, i_in);
	    fputs(indexnumberbuf, i_out);
	    fscanf(i_in, "%%%d\n", &onefileperblock);
	    fprintf(i_out, "%%%d\n", onefileperblock);	/* If #of files change, then they are added to a new partition, which is updated above */
	    if ( !fscanf(i_in, "%%%d\n", &structuredindex) ) /* temp_rdelim may not be present in new-style indexes.  Fixed by mhubin 10/25/99 --GV */
                 fscanf(i_in, "%%%d%s\n", &structuredindex, temp_rdelim);
	    if (structuredindex <= 0) structuredindex = 0;
	    if (RecordLevelIndex) fprintf(i_out, "%%-2 %s\n", old_rdelim);	/* robint@zedcor.com (CANNOT HAPPEN SINCE RecordLevel AND Strucured ARE NOT COMPATIBLE!!!) */
	    else fprintf(i_out, "%%%d\n", attr_num);	/* attributes might have been added during last merge */

	    while(fgets(src_index_buf, REAL_INDEX_BUF, i_in)) {
		j = 0;
		while ((j < REAL_INDEX_BUF) && (src_index_buf[j] != WORD_END_MARK) && (src_index_buf[j] != ALL_INDEX_MARK) && (src_index_buf[j] != '\0') && (src_index_buf[j] != '\n')) j++;
		if ((j >= REAL_INDEX_BUF) || (src_index_buf[j] == '\0') || (src_index_buf[j] == '\n')) continue;
		/* else it is WORD_END_MARK or ALL_INDEX_MARK */
		c = src_index_buf[j+1];
		src_index_buf[j+1] = '\0';
		fputs(src_index_buf, i_out);
		src_index_buf[j+1] = c;
		index=decode16b((src_index_buf[j+1] << 8) | (src_index_buf[j+2]));
		if ((attr_num > 0) && (attr_num < MaxNum8bPartition - 1)) {
		    putc(encode8b(index), i_out);
		}
		else if (attr_num > 0) {
		    putc(src_index_buf[j+1], i_out);
		    putc(src_index_buf[j+2], i_out);
		}
		j += 3;
		if (fputs(src_index_buf+j, i_out) == EOF) {	/* Rest of the partitions information */
		    fprintf(stderr, "Error: write failed at %s:%d\n", __FILE__, __LINE__);
		    exit(2);
		}
	    }

	    fclose(i_in);
	    fflush(i_out);
	    fclose(i_out);
#if	SFS_COMPAT
	    sprintf(s, "%s/.glimpse_split.%d", INDEX_DIR, getpid());
	    sprintf(s1, "%s/%s", INDEX_DIR, INDEX_FILE);
	    rename(s, s1);
#else
	    sprintf(s, "exec %s '%s/.glimpse_split.%d' '%s/%s'", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), getpid(), escapesinglequote(INDEX_DIR, es2), INDEX_FILE);
	    system(s);
#endif
	}
    }
    else {
	/* Don't care about individual file sizes in statistics since the user can look at it anyway by ls -l! */
	sprintf(s, "%s/.glimpse_split.%d", INDEX_DIR, getpid());
        if((i_out = fopen(s, "w")) == NULL) {
	    fprintf(stderr, "can't open %s for writing\n", s);
	    exit(2);
        }
	sprintf(s, "%s/%s", INDEX_DIR, INDEX_FILE);
        if((i_in = fopen(s, "r")) == NULL) {
	    fprintf(stderr, "can't open %s for reading\n", s);
	    exit(2);
        }

        /* modified the original in glimpse's main.c */
        fgets(indexnumberbuf, 256, i_in);
        fputs(indexnumberbuf, i_out);
        fscanf(i_in, "%%%d\n", &onefileperblock);
        if (ByteLevelIndex) fprintf(i_out, "%%-%d\n", file_num);	/* #of files might have changed due to -f/-a */
	else fprintf(i_out, "%%%d\n", file_num);	/* This was the stupidest thing of all! */
        if ( !fscanf(i_in, "%%%d\n", &structuredindex) ) /* 10/25/99 as per mhubin --GV */
               fscanf(i_in, "%%%d%s\n", &structuredindex, temp_rdelim);
	if (structuredindex <= 0) structuredindex = 0;
	if (RecordLevelIndex) fprintf(i_out, "%%-2 %s\n", old_rdelim);	/* robint@zedcor.com */
        else fprintf(i_out, "%%%d\n", attr_num);	/* attributes might have been added during last merge */

	part_size = 0;	/* current offset in the p_table file */
	while(fgets(src_index_buf, REAL_INDEX_BUF, i_in)) {
	    j = 0;
	    while ((j < REAL_INDEX_BUF) && (src_index_buf[j] != WORD_END_MARK) && (src_index_buf[j] != ALL_INDEX_MARK) && (src_index_buf[j] != '\n')) j++;
	    if ((j >= REAL_INDEX_BUF) || (src_index_buf[j] == '\n')) continue;
	    /* else it is WORD_END_MARK or ALL_INDEX_MARK */
	    c = src_index_buf[j+1];
	    src_index_buf[j+1] = '\0';
	    fputs(src_index_buf, i_out);
	    src_index_buf[j+1] = c;
	    c = src_index_buf[j];
	    if (StructuredIndex) {
		index = decode16b((src_index_buf[j+1] << 8) | (src_index_buf[j+2]));
		if ((attr_num > 0) && (attr_num < MaxNum8bPartition - 1)) {
		    putc(encode8b(index), i_out);
		}
		else if (attr_num > 0) {
		    putc(src_index_buf[j+1], i_out);
		    putc(src_index_buf[j+2], i_out);
		}
		j += 2;
	    }
	    if (c == ALL_INDEX_MARK) {
		putc(DONT_CONFUSE_SORT, i_out);
		if (putc('\n', i_out) == EOF) {
		    fprintf(stderr, "Error: write failed at %s:%d\n", __FILE__, __LINE__);
		    exit(2);
		}
		continue;
	    }

	    offset = encode32b(part_size);
	    putc((offset & 0xff000000) >> 24, i_out);	/* force big-endian */
	    putc((offset & 0x00ff0000) >> 16, i_out);
	    putc((offset & 0x0000ff00) >> 8, i_out);
	    putc((offset & 0x000000ff), i_out);
	    if (putc('\n', i_out) == EOF) {
		fprintf(stderr, "Error: write failed at %s:%d\n", __FILE__, __LINE__);
		exit(2);
	    }

	    j++;	/* @first byte of the block numbers */
	    while((src_index_buf[j] != '\n') && (src_index_buf[j] != '\0')) {
		putc(src_index_buf[j++], p_out);
		part_size ++;
	    }
	    if (putc('\n', p_out) == EOF) {
		fprintf(stderr, "Error: write failed at %s:%d\n", __FILE__, __LINE__);
		exit(2);
	    }
	    part_size ++;
	}
	fclose(i_in);
	fflush(i_out);
	fclose(i_out);
#if	SFS_COMPAT
	sprintf(s, "%s/.glimpse_split.%d", INDEX_DIR, getpid());
	sprintf(s1, "%s/%s", INDEX_DIR, INDEX_FILE);
	rename(s, s1);
#else
	sprintf(s, "exec %s '%s/.glimpse_split.%d' '%s/%s'", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), getpid(), escapesinglequote(INDEX_DIR, es2), INDEX_FILE);
	system(s);
#endif
	system(sync_path);	/* sync() has a BUG */
	sprintf(s, "%s/%s", INDEX_DIR, INDEX_FILE);
	if (BuildTurbo) dump_mini(s);
    }
    fflush(p_out);
    fclose(p_out);

    /* Dump file names */
    if (KeepFilenames) {
	sprintf(s, "exec %s '%s/%s' '%s/%s.prev'", SYSTEM_CP, escapesinglequote(INDEX_DIR, es1), NAME_LIST, escapesinglequote(INDEX_DIR, es2), NAME_LIST);
	system(s);
	sprintf(s, "exec %s '%s/%s' '%s/%s.prev'", SYSTEM_CP, escapesinglequote(INDEX_DIR, es1), NAME_LIST_INDEX, escapesinglequote(INDEX_DIR, es2), NAME_LIST_INDEX);
	system(s);
    }
    sprintf(s, "%s/%s", INDEX_DIR, NAME_LIST);
    if((f_out = fopen(s, "w")) == NULL) {
        fprintf(stderr, "can't open %s for writing\n", s);
        exit(2);
    }
    sprintf(s, "%s/%s", INDEX_DIR, NAME_LIST_INDEX);
    if((i_out = fopen(s, "w")) == NULL) {
        fprintf(stderr, "can't open %s for writing\n", s);
        exit(2);
    }
    fprintf(f_out, "%d\n", file_num);
    for(i=0,offset=ftell(f_out); i<name_list_size; i++) {
	if ((LIST_GET(name_list, i) != NULL) && (name_list[0] != '\0')) {
		ATLEASTONEFILE = 1;
		putc((offset&0xff000000) >> 24, i_out);
		putc((offset&0xff0000) >> 16, i_out);
		putc((offset&0xff00) >> 8, i_out);
		putc((offset&0xff), i_out);
		fputs(LIST_GET(name_list, i), f_out);
		putc('\n', f_out);
		offset += strlen(LIST_GET(name_list, i)) + 1;
	}
	else {	/* else empty line to indicate file that was removed = HOLE */
		if (name_list_size == file_num) {
			putc((offset&0xff000000) >> 24, i_out);
			putc((offset&0xff0000) >> 16, i_out);
			putc((offset&0xff00) >> 8, i_out);
			putc((offset&0xff), i_out);
			putc('\n', f_out);
			offset += 1;
		}
	}
	/* else there are no holes since index was purged, so don't put anything */
    }
    if (!ATLEASTONEFILE) {
	fprintf(MESSAGEFILE, "Warning: number of files in the index is zero!\n");
    }
    fflush(f_out);
    fclose(f_out);
    fflush(i_out);
    fclose(i_out);

    if (GenerateHash) {
    /* Dump file hash: don't want to keep filenames in hash-order like index since adding a file can shift many hash-values and change the whole index! */
    if (KeepFilenames) {
	sprintf(s, "exec %s '%s/%s' '%s/%s.prev'", SYSTEM_CP, escapesinglequote(INDEX_DIR, es1), NAME_HASH, escapesinglequote(INDEX_DIR, es2), NAME_HASH);
	system(s);
	sprintf(s, "exec %s '%s/%s' '%s/%s.prev'", SYSTEM_CP, escapesinglequote(INDEX_DIR, es1), NAME_HASH_INDEX, escapesinglequote(INDEX_DIR, es2), NAME_HASH_INDEX);
	system(s);
    }
    sprintf(s, "%s/%s", INDEX_DIR, NAME_HASH);
    if((f_out = fopen(s, "w")) == NULL) {
        fprintf(stderr, "can't open %s for writing\n", s);
        exit(2);
    }
    sprintf(s, "%s/%s", INDEX_DIR, NAME_HASH_INDEX);
    if((i_out = fopen(s, "w")) == NULL) {
        fprintf(stderr, "can't open %s for writing\n", s);
        exit(2);
    }
    if (!built_filename_hashtable) build_filename_hashtable(name_list, file_num);
    hashtablesize = (BigFilenameHashTable ? MAX_64K_HASH : MAX_4K_HASH);
    for (i=0,offset=ftell(f_out); i<hashtablesize; i++) {
	putc((offset&0xff000000) >> 24, i_out);
	putc((offset&0xff0000) >> 16, i_out);
	putc((offset&0xff00) >> 8, i_out);
	putc((offset&0xff), i_out);
	e = name_hashtable[i];
	while(e!=NULL) {
		if ((index = get_new_index(deletedlist, e->index)) < 0) {
			e = e->next;
			continue;
		}
		putc(((index)&0xff000000)>>24, f_out);
		putc(((index)&0xff0000)>>16, f_out);
		putc(((index)&0xff00)>>8, f_out);
		putc(((index)&0xff), f_out);
		offset += 4;
		fputs(e->name, f_out);
		fputc('\0', f_out);	/* so that I can do direct strcmp */
		offset += strlen(e->name) + 1;
		e = e->next;
	}
    }
    fflush(f_out);
    fclose(f_out);
    fflush(i_out);
    fclose(i_out);
    }

#if	0
	fflush(stdout);
	printf("AFTER SAVE_DATA_STRUCTURES:\n");
	sprintf(s, "exec %s -lg .glimpse_*", SYSTEM_LS);
	system(s);
	sprintf(s, "exec %s .glimpse_index", SYSTEM_WC);
	system(s);
	getchar();
#endif	/*0*/

    return 0;
}

/* Merges the index split by save_data_structures into a single index */
merge_splits()
{
	FILE *i_in;
	FILE *p_in;
	FILE *i_out;
	char s[MAX_LINE_LEN], s1[MAX_LINE_LEN], es1[MAX_LINE_LEN], es2[MAX_LINE_LEN], es3[MAX_LINE_LEN], temp_rdelim[MAX_LINE_LEN];
	int j, index;
	unsigned char c;
	char indexnumberbuf[256];
	int onefileperblock, structuredindex, i, recordlevelindex;

#if	0
	fflush(stdout);
	printf("BEFORE MERGE_SPLITS:\n");
	sprintf(s, "exec %s -lg .glimpse_*", SYSTEM_LS);
	system(s);
	sprintf(s, "exec %s .glimpse_index", SYSTEM_HEAD);
	system(s);
	getchar();
#endif	/*0*/

	temp_rdelim[0] = '\0';  /* Initialize in case not read. 10/25/99 --GV */

	sprintf(s, "%s/%s", INDEX_DIR, P_TABLE);
	if ((p_in = fopen(s, "r")) == NULL) {
		fprintf(stderr, "cannot open for reading: %s\n", s);
		exit(2);

	}

	sprintf(s, "%s/%s", INDEX_DIR, INDEX_FILE);
	if ((i_in = fopen(s, "r")) == NULL) {
		fprintf(stderr, "cannot open for reading: %s\n", s);
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
	fprintf(i_out, "%%%d\n", onefileperblock);
      	if ( !fscanf(i_in, "%%%d\n", &structuredindex) )    /* 10/25/99 as per mhubin --GV */
             fscanf(i_in, "%%%d%s\n", &structuredindex, temp_rdelim);
	if (structuredindex == -2) recordlevelindex = 1;
	if (structuredindex <= 0) structuredindex = 0;
	if (recordlevelindex) fprintf(i_out, "%%-2 %s\n", temp_rdelim);
	else fprintf(i_out, "%%%d\n", structuredindex);
	printf("merge: %s\n", temp_rdelim);

#if	!WORD_SORTED
	if (!DeleteFromIndex || FastIndex) {	/* a new index is going to be built in this case: must sort by word */
		fclose(i_in);
		sprintf(s, "%s/%s", INDEX_DIR, MINI_FILE);
		if ((i_in = fopen(s, "r")) != NULL) {	/* minifile exists */
#if	DONTUSESORT_T_OPTION || SFS_COMPAT
			sprintf(s, "exec %s '%s/%s' > '%s/%s.tmp'", SYSTEM_SORT, escapesinglequote(INDEX_DIR, es1), INDEX_FILE, escapesinglequote(INDEX_DIR, es2), INDEX_FILE);
#else
			sprintf(s, "exec %s -T '%s' '%s/%s' > '%s/%s.tmp'", SYSTEM_SORT, escapesinglequote(INDEX_DIR, es1), escapesinglequote(INDEX_DIR, es2), INDEX_FILE, escapesinglequote(INDEX_DIR, es3), INDEX_FILE);
#endif
			system(s);
#if	SFS_COMPAT
			sprintf(s, "%s/%s.tmp", INDEX_DIR, INDEX_FILE);
			sprintf(s1, "%s/%s", INDEX_DIR, INDEX_FILE);
			rename(s, s1);
#else
			sprintf(s, "exec %s '%s/%s.tmp' '%s/%s'", SYSTEM_MV, escapesinglequote(INDEX_DIR, es1), INDEX_FILE, escapesinglequote(INDEX_DIR, es2), INDEX_FILE);
			system(s);
#endif
			system(sync_path);	/* sync() has a BUG */
			fclose(i_in);
		}
		sprintf(s, "%s/%s", INDEX_DIR, INDEX_FILE);
		if ((i_in = fopen(s, "r")) == NULL) {
			fprintf(stderr, "cannot open for reading: %s\n", s);
			exit(2);
		}
		/* skip the 1st 3 lines which might get jumbled up */
		fgets(s, MAX_LINE_LEN, i_in);
		fgets(s, MAX_LINE_LEN, i_in);
		fgets(s, MAX_LINE_LEN, i_in);
	}
#endif	/* !WORD_SORTED */

	while (fgets(src_index_buf, REAL_INDEX_BUF, i_in)) {
	    j = 0;
	    while ((j < REAL_INDEX_BUF) && (src_index_buf[j] != WORD_END_MARK) && (src_index_buf[j] != ALL_INDEX_MARK) && (src_index_buf[j] != '\0') && (src_index_buf[j] != '\n')) j++;
	    if ((j >= REAL_INDEX_BUF) || (src_index_buf[j] == '\0') || (src_index_buf[j] == '\n')) continue;
	    /* else it is WORD_END_MARK or ALL_INDEX_MARK */
	    c = src_index_buf[j+1];
	    src_index_buf[j+1] = '\0';
	    fputs(src_index_buf, i_out);
	    src_index_buf[j+1] = c;
	    c = src_index_buf[j];
	    if (structuredindex) {	/* convert all attributes to 2B to make merge_in()s easy in build_in.c */
		if (structuredindex < MaxNum8bPartition - 1) {
		    index = encode16b(decode8b(src_index_buf[j+1]));
		    putc((index & 0x0000ff00) >> 8, i_out);
		    putc(index & 0x000000ff, i_out);
		    j ++;
		}
		else {
		    putc(src_index_buf[j+1], i_out);
		    putc(src_index_buf[j+2], i_out);
		    j += 2;
		}
	    }
	    if (c == ALL_INDEX_MARK) {
		putc(DONT_CONFUSE_SORT, i_out);
		putc('\n', i_out);
		continue;
	    }

	    /* src_index_buf[j+1] points to the first byte of the offset */
	    get_block_numbers(&src_index_buf[j+1], &dest_index_buf[0], p_in);
	    j = 0;	/* first byte of the block numbers */
	    while ((dest_index_buf[j] != '\n') && (dest_index_buf[j] != '\0')) {
		putc(dest_index_buf[j], i_out);
		dest_index_buf[j] = '\0';
		j++;
	    }
	    if (putc('\n', i_out) == EOF) {
		fprintf(stderr, "Error: write failed at %s:%d\n", __FILE__, __LINE__);
		exit(2);
	    }
	}

	fclose(i_in);
	fclose(p_in);
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

#if	0
	fflush(stdout);
	printf("AFTER MERGE_SPLITS:\n");
	sprintf(s, "exec %s -lg .glimpse_*", SYSTEM_LS);
	system(s);
	sprintf(s, "exec %s .glimpse_index"SYSTEM_HEAD);
	system(s);
	getchar();
#endif	/*0*/
}

