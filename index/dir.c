/* Copyright (c) 1994 Sun Wu, Udi Manber, Burra Gopal.  All Rights Reserved. */
/* ./glimpse/index/dir.c */

/* The function of the program is to traverse the
   direcctory tree and print the size of the files in the tree.
   This program is derived from the C-programming language book
   It opens a directory file using opendir system call, and use readdir()
   to read each entry of the directory.
*/

#include "autoconf.h"	/* ../libtemplate/include */
#include <stdio.h>
#include <sys/types.h>

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include <sys/stat.h>
#include <fcntl.h>
#define BUFSIZE 256
#define DIRSIZE 14
#include "glimpse.h"
#undef MAX_LIST
#define MAX_LIST 100000

/* Removed on 16/Feb/1996 becuase changed type returned by lib_fstat to S_IFLNK
#if	SFS_COMPAT
#define FS_TYPEMASK	0x700000
#define FS_LINK		0x300000
#endif
*/

extern FILE *TIMEFILE;
#if	BG_DEBUG
extern FILE *LOGFILE;
#endif	/*BG_DEBUG*/
extern FILE *MESSAGEFILE;
int ndx = 0;                    /* file index */
extern char **name_list[MAXNUM_INDIRECT];  /* store the file names */
extern int  *size_list[MAXNUM_INDIRECT];   /* store the sizes of the files */
extern unsigned int *disable_list;   /* store whether to DISABLE indexing or not: only with FastIndex or AddToIndex */
extern int  file_num;
extern int  file_id;	/* borrowed from filetype.c */
extern char INDEX_DIR[MAX_LINE_LEN];
extern int AddToIndex;
extern int DeleteFromIndex;
extern int FastIndex;
extern int OneFilePerBlock;
extern int IncludeHigherPriority;
extern int BuildDictionaryExisting;
extern int IndexEverything;
extern int printed_warning;
extern int SortByTime;
extern int p_table[];
extern FILE *STATFILE;

extern int ExtractInfo;
extern int IndexableFile;
extern int files_per_partition;
extern int new_partition;
extern int files_in_partition;
extern struct stat istbuf;	/* imported from glimpse.c */
extern int memory_usage;
extern int mask_int[];

extern char exin_argv[8];
extern int exin_argc;
extern char current_dir_buf[2*MAX_LINE_LEN + 4];	/* must have space to store pattern after directory name */
extern unsigned char dummypat[MAX_PAT];
extern int dummylen;
extern FILE *dummyout;

extern struct stat excstbuf;
extern struct stat incstbuf;
extern struct stat filstbuf;

extern int num_filter;
extern int filter_len[MAX_FILTER];
extern CHAR *filter[MAX_FILTER];
extern CHAR *filter_command[MAX_FILTER];

/*
 * Exclude/Include priorities with exclude > include (IncludeHigherPriority = OFF = default):
 * 1. Command line arguments (inclusion --> exclude list is never applied)
 * 2. Exclude list (exclusion)
 * 3. Include list (inclusion)
 * 4. Symbolic links (exclusion)
 * 5. filter processing (inclusion --> so that binary files that can be filtered are not excluded)
 * 6. filetype (exclusion)
 * 
 * Exclude/Include priorities with include > exclude (IncludeHigherPriority = ON = -i):
 * 1. Command line arguments (inclusion --> exclude list is never applied)
 * 2. Include list (inclusion)
 * 3. Symbolic links (exclusion --> applying exclude list is unnecessary: optimization)
 * 4. Exclude list (exclusion)
 * 5. filter processing (inclusion --> so that binary files that can be filtered are not excluded)
 * 6. filetype (exclusion)
 */

char outname[MAX_LINE_LEN];
char inname[MAX_LINE_LEN];

fsize(name, pat, pat_len, num_pat, inc, inc_len, num_inc, toplevel)
char *name;
char **pat;
int *pat_len;
int num_pat;
char **inc;
int *inc_len;
int num_inc;
int toplevel;
{
	struct stat stbuf;
	int i;
	int fileindex = -1;
	int force_include = 0;
	int len_current_dir_buf = strlen(current_dir_buf) + 1;	/* includes the '\0' which is going to be replaced by '\n' below */
	int name_len;
	char *t1;
	char xinfo[MAX_LINE_LEN], temp[MAX_LINE_LEN];
	int xinfo_len = 0;

	if ((name == NULL) || (*name == '\0')) return 0;
	name_len = strlen(name);	/* name[name_len] is '\0' */

#ifdef	SW_DEBUG
	printf("num_pat= %d num_inc= %d\n", num_pat, num_inc);
	printf("name= %s\n", name);
#endif

	/*
	 * Find out what to exclude, what to include and skip
	 * over symbolic links that don't HAVE to be included.
	 * Some Extra get_filename_index calls are done but
	 * that won't cost you anything (just #ing twice).
	 */

	/* Check if cache set in glimpse.c is correct */
	if (!IndexableFile && !DeleteFromIndex && FastIndex && ((fileindex = get_filename_index(name, name_list, file_num)) != -1) && (disable_list[block2index(fileindex)] & mask_int[fileindex % (8*sizeof(int))])) {
	    if (num_pat <= 0) {
		if (num_inc <= 0) return 0;
		else if (incstbuf.st_ctime <= istbuf.st_ctime) return 0;
	    }
	    else {
		if (num_inc <= 0) {
			if (excstbuf.st_ctime <= istbuf.st_ctime) return 0;
		}
		else if ((excstbuf.st_ctime <= istbuf.st_ctime) && (incstbuf.st_ctime <= istbuf.st_ctime)) return 0;
	    }
	}

#define PROCESS_EXIT \
{\
	if (AddToIndex || FastIndex || DeleteFromIndex) {\
		if ((fileindex = get_filename_index(name, name_list, file_num)) != -1) \
			remove_filename(fileindex, new_partition);\
	}\
}

#define PROCESS_EXCLUDE \
{\
	if (!toplevel) for(i=0; i<num_pat; i++) {	/* bg: 15/mar/94 */\
		if (pat_len[i] > 0) {\
			name[name_len] = '\0';\
			if (strstr(name, (const char *)pat[i]) != NULL) {\
				PROCESS_EXIT;\
				return 0;\
			}\
		}\
		else {	/* must call memagrep */\
			int	ret;\
			name[name_len] = '\n';	/* memagrep wants names to end with '\n': '\0' is not necessary */\
			/* printf("i=%d patlen=%d pat=%s inlen=%d input=%s\n", i, -pat_len[i], pat[i], len_current_dir_buf, current_dir_buf); */\
			if (((pat_len[i] == -2) && (pat[i][0] == '.') && (pat[i][1] == '*')) ||\
			    ((ret = memagrep_search(-pat_len[i], pat[i], len_current_dir_buf, current_dir_buf, 0, dummyout)) > 0))\
			{\
				/* printf("excluding with %d %s\n", ret, name); */\
				name[name_len] = '\0';	/* restore */\
				PROCESS_EXIT;\
		  		return 0; \
			}\
			/* else printf("ret=%d\n");*/\
		}\
	}\
	name[name_len] = '\0';\
}

#define PROCESS_INCLUDE \
{\
 	/*\
	 * When include has higher priority, we want to include directories\
	 * by default and match the include patterns only against filenames.\
	 * Based on bug reports for glimpse-2.1. bg: 2/mar/95.\
 	 */\
 	if (IncludeHigherPriority && ((stbuf.st_mode & S_IFMT) == S_IFDIR)) force_include = 1;\
 	else for (i=0; i<num_inc; i++) {	/* bg: 15/mar/94 */\
		if (inc_len[i] > 0) {\
			name[name_len] = '\0';\
			if (strstr(name, (const char *)inc[i]) != NULL) {\
				force_include = 1;\
				break;\
			}\
		}\
		else {	/* must call memagrep */\
			name[name_len] = '\n';	/* memagrep wants names to end with '\n': '\0' is not necessary */\
			/* printf("pat=%s input=%s\n", pat[i], current_dir_buf); */\
			if (((inc_len[i] == -2) && (inc[i][0] == '.') && (inc[i][1] == '*')) ||\
			    (memagrep_search(-inc_len[i], inc[i], len_current_dir_buf, current_dir_buf, 0, dummyout) > 0))\
			{\
				force_include = 1;\
				break;\
			}\
		}\
	}\
	name[name_len] = '\0';	/* restore */\
	if (toplevel) force_include = 1;\
}

#define PROCESS_FILTER \
{\
	/*\
	 * Filters should be processed independent of .include since they might have to be\
	 * excluded first. However, they must be processed before filetype since legitimate\
	 * files like *.Z might be excluded by it. Based on bug reports for glimpse-3.5: bg: 11/Apr/96.\
	 */\
	if (!force_include) for (i=0; i<num_filter; i++) {	/* bg: 16/sep/94 */\
		if (filter_len[i] > 0) {\
			name[name_len] = '\0';\
			if (strstr(name, (const char *)filter[i]) != NULL) {\
				force_include = 1;\
				break;\
			}\
		}\
		else {	/* must call memagrep */\
			name[name_len] = '\n';	/* memagrep wants names to end with '\n': '\0' is not necessary */\
			/* printf("pat=%s input=%s\n", pat[i], current_dir_buf); */\
			if (((filter_len[i] == -1) && (filter[i][0] == '.') && (filter[i][1] == '*')) ||\
			    (memagrep_search(-filter_len[i], filter[i], len_current_dir_buf, current_dir_buf, 0, dummyout) > 0))\
			{\
				force_include = 1;\
				break;\
			}\
		}\
	}\
	name[name_len] = '\0';	/* restore */\
}

        if(my_lstat(name, &stbuf) == -1) {
		if (IndexableFile) return 0;
		/* Can happen for command line arguments, not stuff obtained from fsize_directory() */
#if	BG_DEBUG
		fprintf(LOGFILE, "cannot find %s -- not indexing\n", name);
#endif	/*BG_DEBUG*/
		PROCESS_EXIT;
		return 0;
	}
	/* Else lstat has all the requisite information */

/* Removed on 16/Feb/1996 becuase changed type returned by lib_fstat to S_IFLNK
#if	SFS_COMPAT
	if ((stbuf.st_spare1 & FS_TYPEMASK) == FS_LINK) return 0;
#endif
*/
	if ((stbuf.st_mode & S_IFMT) == S_IFLNK)  {
		/* if (IndexableFile) return 0; ---> not correct! must process include/exclude with -I too */
		PROCESS_INCLUDE;
		if (!force_include) {
#if	BG_DEBUG
			fprintf(LOGFILE, "%s is a symbolic link -- not indexing\n", name);
#endif	/*BG_DEBUG*/
			PROCESS_EXIT;
			return 0;
		}
		if (-1 == my_stat(name, &stbuf)) {
#if	BG_DEBUG
			fprintf(LOGFILE, "cannot find target of symbolic link %s -- not indexing\n", name);
#endif	/*BG_DEBUG*/
			PROCESS_EXIT;
			return 0;
		}
	}
	else /* if (!IndexableFile) ---> not correct! must process include/exclude with -I too */ {
		/* Put exclude include processing here... stat all the time: that is faster than former! */
		if (FastIndex && ((fileindex = get_filename_index(name, name_list, file_num)) != -1)) {
			/* Don't process exclude/include if the file `name' is older then the index AND the exclude/include file is older then the index */
			if (IncludeHigherPriority) {
				if (!((stbuf.st_ctime <= istbuf.st_ctime) && (incstbuf.st_ctime <= istbuf.st_ctime)))
					PROCESS_INCLUDE;
				if (!force_include && !((stbuf.st_ctime <= istbuf.st_ctime) && (excstbuf.st_ctime <= istbuf.st_ctime)))
					PROCESS_EXCLUDE;
			}
			else {
				if (!((stbuf.st_ctime <= istbuf.st_ctime) && (excstbuf.st_ctime <= istbuf.st_ctime)))
					PROCESS_EXCLUDE;
				if (!((stbuf.st_ctime <= istbuf.st_ctime) && (incstbuf.st_ctime <= istbuf.st_ctime)))
					PROCESS_INCLUDE;
			}
			if (!((stbuf.st_ctime <= istbuf.st_ctime) && (filstbuf.st_ctime <= istbuf.st_ctime)))
				PROCESS_FILTER;
		}
		else {	/* Either AddToIndex or fresh indexing or previously excluded file: process exclude and include */
			if (IncludeHigherPriority) {
				PROCESS_INCLUDE;
				if (!force_include)
					PROCESS_EXCLUDE;
			}
			else {
				PROCESS_EXCLUDE;
				PROCESS_INCLUDE;
			}
			PROCESS_FILTER;
		}
	}

	/* Here, the file exists and has not been excluded -- possibly has been included */
index_everything:
	if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
		if (-1 == fsize_directory(name, pat, pat_len, num_pat, inc, inc_len, num_inc)) return -1;
	}
        else if ((stbuf.st_mode & S_IFMT) == S_IFREG) {	/* regular file */
	    if (IndexableFile) {
		if (!filetype(name, IndexEverything?2:1, NULL, NULL)) printf("%s\n", name);
		return 0;
	    }
	    if (DeleteFromIndex) {
		if ((fileindex = get_filename_index(name, name_list, file_num)) != -1) {
		    remove_filename(fileindex, new_partition);
		}
		/* else doesn't exist in index, so doesn't matter */
		return 0;
	    }
	    file_id ++;
	    if (BuildDictionaryExisting) {
		/* Don't even store the names of the files that are not uncompressible */
		if (file_num >= MaxNum24bPartition) {
		    fprintf(stderr, "Too many files in index: indexing the first %d only.\n", MaxNum24bPartition);
		    return -1;
		}
	        if (tuncompress_file(name, outname, TC_EASYSEARCH | TC_OVERWRITE | TC_NOPROMPT) <= 0) return 0;
		file_num++;
		t1 = (char *) my_malloc(strlen(outname) + 2);
		strcpy(t1, outname);
		/* name_list[ndx] = t1; */
		LIST_ADD(name_list, ndx, t1, char*);
		/* size_list[ndx] = stbuf.st_size;*/
		LIST_ADD(size_list, ndx, stbuf.st_size, int);
		ndx ++;
		return 0;
	    }

#ifdef SW_DEBUG
	    printf("%s: ", name);
#endif

	    if (AddToIndex || FastIndex) {
		if ((fileindex = get_filename_index(name, name_list, file_num)) != -1) {
		    LIST_ADD(size_list, fileindex, stbuf.st_size, int);
		    if (FastIndex && (stbuf.st_ctime <= istbuf.st_ctime))
			disable_list[block2index(fileindex)] |= mask_int[fileindex % (8*sizeof(int))];
		    else { /* AddToIndex or file was modified (=> its type might have changed!) */
			if (filetype(name, IndexEverything?2:1, &xinfo_len, xinfo)) {
			    if (!force_include) {
				remove_filename(fileindex, new_partition);
				return 0;
			    }
			    else {
#if	BG_DEBUG
				fprintf(LOGFILE, "overriding and indexing: %s\n", name);
#endif	/*BG_DEBUG*/
			    }
			}
			if (ExtractInfo && (xinfo_len > 0)/* && (special_get_name(name, name_len, temp) != -1) NOT NEEDED since name is got from UNIX */) {
			    my_free(LIST_SUREGET(name_list, fileindex));
			    t1 = (char *)my_malloc(strlen(name) + xinfo_len + 3);
			    strcpy(t1, name);
			    strcat(t1, " ");
			    strcat(t1, xinfo);
			    LIST_ADD(name_list, fileindex, t1, char*);
			    change_filename(name, name_len, fileindex, t1);
			}
			disable_list[block2index(fileindex)] &= ~(mask_int[fileindex % (8*sizeof(int))]);
		    }
		}
		else {	/* new file not in filenames so no point in checking */
		    if(filetype(name, IndexEverything?2:1, &xinfo_len, xinfo)) {
			if (!force_include) return 0;
			else {
#if	BG_DEBUG
				fprintf(LOGFILE, "overriding and indexing: %s\n", name);
#endif	/*BG_DEBUG*/
			}
		    }

		    if (file_num >= MaxNum24bPartition) {
			fprintf(stderr, "Too many files in index: indexing the first %d only.\n", MaxNum24bPartition);
			return -1;
		    }

		    if (ExtractInfo && (xinfo_len > 0)) {
			t1 = (char *)my_malloc(strlen(name) + xinfo_len + 3);
			strcpy(t1, name);
			strcat(t1, " ");
			strcat(t1, xinfo);
		    }
		    else {
			t1 = (char *)my_malloc(strlen(name) + 2);
			strcpy(t1, name);
		    }
		    /* name_list[file_num] = t1; */
		    LIST_ADD(name_list, file_num, t1, char*);
		    /* size_list[file_num] = stbuf.st_size; */
		    LIST_ADD(size_list, file_num, stbuf.st_size, int);
		    insert_filename(LIST_GET(name_list, file_num), file_num);
		    file_num ++;

		    if (!OneFilePerBlock) {
		        if (files_in_partition + 1 > files_per_partition) {
			    if (new_partition + 1 > MaxNumPartition) {
				if (!printed_warning) {
				    printed_warning = 1;
				    if (AddToIndex) {
					fprintf(MESSAGEFILE, "Warning: partition-table overflow! Fresh indexing recommended.n");
				    }
				    else {
					fprintf(MESSAGEFILE, "Warning: partition-table overflow! Commencing fresh indexing...\n");
					return -1;
				    }
				}
			    }
			    else new_partition++;
			    files_in_partition = 0;
			    /* so that we don't get into this if-branch until another files_per_partition new files are seen */
			}

			p_table[new_partition] = file_num;
			files_in_partition ++;
		    }
		}
	    }
	    else { /* Fresh indexing: very simple -- add everything */
		if(filetype(name, IndexEverything?2:1, &xinfo_len, xinfo)) {
		    if (!force_include) return 0;
		    else {
#if	BG_DEBUG
			fprintf(LOGFILE, "overriding and indexing: %s\n", name);
#endif	/*BG_DEBUG*/
		    }
		}
		if (file_num >= MaxNum24bPartition) {
		    fprintf(stderr, "Too many files in index: indexing the first %d only.\n", MaxNum24bPartition);
		    return -1;
		}
		if (SortByTime) fprintf(TIMEFILE, "%ld %d\n", stbuf.st_mtime, file_num);
		file_num++;
		if (ExtractInfo && (xinfo_len > 0)) {
		    t1 = (char *)my_malloc(strlen(name) + xinfo_len + 3);
		    strcpy(t1, name);
		    strcat(t1, " ");
		    strcat(t1, xinfo);
		}
		else {
		    t1 = (char *) my_malloc(strlen(name) + 2);
		    strcpy(t1, name);
		}
		/* name_list[ndx] = t1; */
		LIST_ADD(name_list, ndx, t1, char*);
		/* size_list[ndx] = stbuf.st_size; */
		LIST_ADD(size_list, ndx, stbuf.st_size, int);
		ndx++;
	    }
        }
	return 0;
}

/* uses the space in the same "name" to get names of files in that directory and calls fsize */
/* pat, pat_len, num_pat, inc, inc_len, num_inc are just used for recursive calls to fsize */
/* special_get_name() doesn't have to be done since glimpseindex indexes just files, not directories, so dir's have no URL information, etc. */
fsize_directory(name, pat, pat_len, num_pat, inc, inc_len, num_inc)
char *name;
char **pat;
int *pat_len;
int  num_pat;
char **inc;
int *inc_len;
int  num_inc;
{
	struct dirent *dp;
	char *nbp, *nep;
	int i;
	DIR *dirp;
	/*
	printf("in fsize_directory, name= %s\n",name);
	*/
	if ((name == NULL) || (*name == '\0')) return 0;
	nbp = name + strlen(name);
	if( nbp+DIRSIZE+2 >= name+BUFSIZE ) /* name too long */
	{       fprintf(stderr, "name too long: %s\n", name);
		return 0;
	}
        if((dirp = opendir(name)) == NULL) {
		fprintf(stderr, "permission denied or non-existent directory: %s\n", name);
		return 0;
	}
	*nbp++ = '/';
        for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
        	if (dp->d_name[0] == '\0' || strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) continue;
		for(i=0, nep=nbp; (dp->d_name[i] != '\0') && (nep < name+BUFSIZ-1); i++)
			*nep++ = dp->d_name[i];
		if (dp->d_name[i] != '\0') {
			*nep = '\0';
			fprintf(stderr, "name too long: %s\n", name);
			continue;
		}
		*nep = '\0';
		/*
		printf("name= %s\n", name);
		*/
		if (-1 == fsize(name, pat, pat_len, num_pat, inc, inc_len, num_inc, 0)) return -1;
	}
        closedir (dirp);
	*--nbp = '\0'; /* restore name */
	return 0;
}
