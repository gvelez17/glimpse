/* Copyright (c) 1994 Sun Wu, Udi Manber, Burra Gopal.  All Rights Reserved. */
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "glimpse.h"
#include <fcntl.h>
#define  CHAR unsigned char

/* ----------------------------------------------------------------------
get_filenames()
input: an index table, (an index vector, i-th entry is ON if
i-th partition is to be searched.), the partition table in src_index_set[]
and the list of all files in "NAME_LIST".
output: the list of filenames to be searched.
------------------------------------------------------------------------- */

#if	BG_DEBUG
extern FILE *debug;
#endif	/*BG_DEBUG*/

extern int  p_table[MAX_PARTITION];
extern CHAR **GTextfiles;
extern CHAR **GTextfilenames;
extern int *GFileIndex;
extern int GNumfiles;
extern CHAR GProgname[];
extern CHAR FileNamePat[];
extern int  MATCHFILE;
extern int  agrep_outpointer;

extern int mask_int[32];
extern int OneFilePerBlock;
extern char INDEX_DIR[MAX_LINE_LEN];
extern	unsigned int	*multi_dest_index_set[MAXNUM_PAT];
extern int file_num;	/* in index/io.c */
int bigbuffer_size;
int first_line_len = 0;
char *bigbuffer = NULL;	/* constant buffer to read all filenames in NAME_LIST */
char *outputbuffer = NULL;	/* keeps changing: used for -F search via memagrep */
int outputbuffer_len = 0;
extern int REAL_PARTITION, REAL_INDEX_BUF, MAX_ALL_INDEX, FILEMASK_SIZE;

read_filenames()
{
	struct stat st;
	unsigned char buffer[MAX_NAME_SIZE];
	char *currptr;
	int i;

	/* one time processing: assumes during one run of glimpse, the index remains constant! */
	if (bigbuffer == NULL) {
		FILE *fp = fopen(NAME_LIST, "r");

		if (fp == NULL) {
			fprintf(stderr, "Can't open for reading: %s/%s\n", INDEX_DIR, NAME_LIST);
			exit(2);
		}
		if (-1 == stat(NAME_LIST, &st)) {
			fclose(fp);
			fprintf(stderr, "Can't stat: %s/%s\n", INDEX_DIR, NAME_LIST);
			exit(2);
		}
		fgets(buffer, MAX_NAME_SIZE, fp);
		first_line_len = strlen(buffer);
		bigbuffer_size = st.st_size - first_line_len;
		sscanf(buffer, "%d", &file_num);
		if ((file_num < 0) || (file_num > MaxNum24bPartition)) {
			fclose(fp);
			fprintf(stderr, "Error in reading: %s/%s\n", INDEX_DIR, NAME_LIST);
			exit(2);
		}
		if (file_num == 0) {
			fclose(fp);
			fprintf(stderr, "Warning: No files were indexed! Exiting...\n");
			exit(2);
		}
		initialize_data_structures(file_num);
		for (i=0; i<MAXNUM_PAT; i++) {
			multi_dest_index_set[i] = (unsigned int *)my_malloc(sizeof(int)*REAL_PARTITION);
			memset(multi_dest_index_set[i], '\0', sizeof(int) * REAL_PARTITION);
		}
		bigbuffer = (char *)my_malloc(bigbuffer_size + 2*MAX_PAT + 2);	/* The whole file + place to store -F's pattern on BOTH sides */
		outputbuffer_len = (FILES_PER_PARTITION(file_num)*MAX_NAME_SIZE);
		if (bigbuffer != NULL) outputbuffer = (char *)my_malloc(outputbuffer_len);	/* Space for max# files per partition */
		if (outputbuffer != NULL) GTextfiles = (CHAR **) my_malloc(sizeof(CHAR *) * file_num);
		if (GTextfiles != NULL) GTextfilenames = (CHAR **) my_malloc(sizeof(CHAR *) * file_num);
		if (GTextfilenames != NULL) GFileIndex = (int *)my_malloc(sizeof(int) * file_num);
		if (bigbuffer == NULL || outputbuffer == NULL || GTextfiles == NULL || GTextfilenames == NULL || GFileIndex == NULL) {
			fclose(fp);
			fprintf(stderr, "%s: my_malloc failure in %s:%d!\n", GProgname, __FILE__, __LINE__);
			exit(2);	/* No point freeing memory */
		}
		if (bigbuffer_size != fread(bigbuffer+MAX_PAT, 1, bigbuffer_size, fp)) {/* read in whole file in CONTIGUOUS memory */
			fclose(fp);
			fprintf(stderr, "Error in reading: %s/%s\n", INDEX_DIR, NAME_LIST);
			exit(2);	/* No point freeing memory */
		}
		memset(bigbuffer, '\n', MAX_PAT);
		memset(bigbuffer+bigbuffer_size+MAX_PAT, '\n', MAX_PAT + 2);
		for (i=0, currptr = bigbuffer+MAX_PAT; i<file_num && currptr < bigbuffer + MAX_PAT + bigbuffer_size; i++, currptr ++) {
			GTextfilenames[i] = (unsigned char *)currptr;
			while (*currptr != '\n') currptr ++;
		}
	}
	return 0;
}

/* If too many files obtained as result of search, go to original slow algorithm */
/* Else, -f files as pattern to search the actual list of filenames that matched (compute new list from those that matched) */
int
mask_filenames(index_vect, infile, num_files, num_blocks)
int  *index_vect;
char *infile;
int num_files;	/* total number of files */
int num_blocks;	/* number of files matching the search expression */
{
	char	*argv[8];
	int	ret, i /* filenames_index */, j /* outputbuffer */, k, l, count, maxcount, offset, prevreadoffset, readoffset, num_read, name_list_size, found;
	char	*temp_bigbuffer;
	int	*temp_bigbuffer_offset;
	int	*temp_bigbuffer_index;
	int	temp_bigbuffer_len = 0;
	int	curr_temp_bigbuffer;
	char	*name_list_buffer;

	if ((num_blocks*100/num_files > DEF_MAX_INDEX_PERCENT/2) && (num_blocks > MaxNum8bPartition)) return slow_mask_filenames(index_vect, infile);
	for (i=0; i<num_files; i++) {
		if (index_vect[block2index(i)] & mask_int[i % 32]) {
			if (i < num_files - 1) temp_bigbuffer_len += GTextfilenames[i+1] - GTextfilenames[i] /* including '\n' */;
			else temp_bigbuffer_len += &bigbuffer[bigbuffer_size + MAX_PAT] - (char *)GTextfilenames[i];
		}
	}
	if ((temp_bigbuffer = (char *)malloc(temp_bigbuffer_len + 2)) == NULL) {
		fprintf(stderr, "%s: my_malloc failure in %s:%d!\n", GProgname, __FILE__, __LINE__);
		exit(2);
	}
	if ((temp_bigbuffer_offset = (int *)malloc((num_blocks+1)*sizeof(int))) == NULL) {
		fprintf(stderr, "%s: my_malloc failure in %s:%d!\n", GProgname, __FILE__, __LINE__);
		exit(2);
	}
	if ((temp_bigbuffer_index = (int *)malloc(num_blocks*sizeof(int))) == NULL) {
		fprintf(stderr, "%s: my_malloc failure in %s:%d!\n", GProgname, __FILE__, __LINE__);
		exit(2);
	}
	temp_bigbuffer[0] = '\n';
	for (i=0, curr_temp_bigbuffer=1, j=0; (i<num_files) && (j<num_blocks); i++) {
		if (index_vect[block2index(i)] & mask_int[i % 32]) {	/* will be satisfied num_blocks times */
			temp_bigbuffer_offset[j] = curr_temp_bigbuffer;
			temp_bigbuffer_index[j] = i;
			j++;
			if (i < num_files - 1) {
				memcpy(&temp_bigbuffer[curr_temp_bigbuffer], GTextfilenames[i], GTextfilenames[i+1] - GTextfilenames[i] /* including '\n' */);
				curr_temp_bigbuffer += GTextfilenames[i+1] - GTextfilenames[i] /* including '\n' */;
			}
			else {
				memcpy(&temp_bigbuffer[curr_temp_bigbuffer], GTextfilenames[i], &bigbuffer[bigbuffer_size + MAX_PAT] - (char *)GTextfilenames[i]);
				curr_temp_bigbuffer += &bigbuffer[bigbuffer_size  + MAX_PAT] - (char *)GTextfilenames[i] /* including '\n' */;
			}
		}
	}
	temp_bigbuffer_offset[num_blocks] = temp_bigbuffer_len + 1;	/* last one */

	/* Now, call agrep with print offset of match */
	name_list_buffer = temp_bigbuffer;
	name_list_size = temp_bigbuffer_len + 1;	/* including initial '\n' */
	argv[0] = "glimpse";
	argv[1] = "-b";
	argv[2] = "-u";
	argv[3] = "-f";
	argv[4] = infile;
	argv[5] = "";
	errno = 0;
	if ((((ret = memagrep(5, argv,  name_list_size, name_list_buffer, outputbuffer_len, outputbuffer)) <= 0) || (agrep_outpointer <= 0)) && (errno != ERANGE)) {
		for (i=0; i<round(file_num, 8*sizeof(int)); i++)
			index_vect[i] = 0;
		free(temp_bigbuffer);
		free(temp_bigbuffer_offset);
		free(temp_bigbuffer_index);
		return -1;
	}

	/*
	outputbuffer[agrep_outpointer] = '\0';
	printf("%s", outputbuffer);
	getchar();
	*/

	/* Check these offsets (outputbuffer[0..agrep_outpointer]) against those stored in temp_bigbuffer_offset and figure out the mask */
	/* Use the fact that temp_bigbuffer_offset has the beginning offset of each file name in bigbuffer */
	for (i=0; i<round(file_num, 8*sizeof(int)); i++)
		multi_dest_index_set[0][i] = 0;
	ret = sizeof(int);
	num_read = ret;
	prevreadoffset = temp_bigbuffer_offset[0];
	/* printf("prevreadoffset=%d name_list_size=%d\n", prevreadoffset, name_list_size); */
	count = 0;
	maxcount = 0;
	i = 0;
	j = 0;
	l = 0;
	while (j < agrep_outpointer) {	/* offsets will be printed by agrep always in the increasing order, so this is a one pass algorithm */
		k = j;
		while ((k<agrep_outpointer) && (isalnum(((unsigned char*)outputbuffer)[k]))) k++;
		outputbuffer[k] = '\0';
		offset = 0;
		errno = 0;
		offset = atoi(&outputbuffer[j]);
		/* printf("offset=%d\n", offset); */
		k++;
		while ((k<agrep_outpointer) && (!isalnum(((unsigned char *)outputbuffer)[k]))) k++;
		j = k;
		if ((offset <= 0) || (errno == ERANGE)) continue;
		found = 0;

		while (!found) {
			if (count >= maxcount) {	/* first time (not compressing into smaller code since I want it to be similar to slow_mask... below) */
				ret = (num_blocks - 1)*sizeof(int);
				num_read += ret;
				maxcount = num_blocks;

				for (i=0; i<=ret /* to process last one also */; i+=sizeof(int), count++) {
					readoffset = temp_bigbuffer_offset[1 + i/sizeof(int)];
					/* printf("readoffset=%d\n", readoffset); */
					if ((offset >= prevreadoffset) && (offset < readoffset)) {
						/* printf("count=%d\n", count); */
						if (OneFilePerBlock)
							multi_dest_index_set[0][block2index(temp_bigbuffer_index[count])] |= mask_int[temp_bigbuffer_index[count] % 32];
						else {
							for (; l<MAX_PARTITION; l++) {
								if ((temp_bigbuffer_index[count] >= p_table[l]) && (temp_bigbuffer_index[count] < p_table[l+1])) {
									multi_dest_index_set[0][l] = 1;
									break;	/* out of for */
								}
							}
							/* can't come here without break: if it does (serious!) will break out w/o setting anything */
						}
						prevreadoffset = readoffset;
						i += sizeof(int);
						count ++;
						found = 1;
						break;	/* out of for */
					}
					prevreadoffset = readoffset;
				}
			}
			else {
				for (; i<=ret /* to process last one also */; i+=sizeof(int), count++) {
					readoffset = temp_bigbuffer_offset[1 + i/sizeof(int)];
					/* printf("readoffset=%d\n", readoffset); */
					if ((offset >= prevreadoffset) && (offset < readoffset)) {
						/* printf("count=%d\n", count); */
						if (OneFilePerBlock)
							multi_dest_index_set[0][block2index(temp_bigbuffer_index[count])] |= mask_int[temp_bigbuffer_index[count] % 32];
						else {
							for (; l<MAX_PARTITION; l++) {
								if ((temp_bigbuffer_index[count] >= p_table[l]) && (temp_bigbuffer_index[count] < p_table[l+1])) {
									multi_dest_index_set[0][l] = 1;
									break;	/* out of for */
								}
							}
							/* can't come here without break: if it does (serious!) will break out without setting anything */
						}

						prevreadoffset = readoffset;
						i += sizeof(int);
						count ++;
						found = 1;
						break;	/* out of for */
					}
					prevreadoffset = readoffset;
				}

			}
		}
	}

	/* Now AND the incoming mask with the one constructed above */
	if (OneFilePerBlock) {
		for (i=0; i<round(file_num, 8*sizeof(int)); i++)
			index_vect[i] &= multi_dest_index_set[0][i];
	}
	else {
		for (i=0; i<MAX_PARTITION; i++)
			index_vect[i] &= multi_dest_index_set[0][i];
	}
	free(temp_bigbuffer);
	free(temp_bigbuffer_offset);
	free(temp_bigbuffer_index);
	return 0;
}

/* Searches the set of file names in bigbuffer for the files mentioned in infile and forces index_vect to contain the mask for these files only */
/* Works only when .glimpse_filenames_hash is created by glimpseindex, i.e., glimpse version 3.0 or more */
int
slow_mask_filenames(index_vect, infile)
int  *index_vect;
char *infile;
{
	char	*argv[8], tempfile[MAX_NAME_LEN], *name_list_buffer;
	CHAR	tempbuf[MAX_LINE_LEN];
	FILE	*fp;
	int	ret, i /* filenames_index */, j /* outputbuffer */, k, l, count, maxcount, offset, prevreadoffset, readoffset, num_read, name_list_size, found;
	struct stat st_buf;

	/* Call agrep with "print byte-offset = -b", "don't print pattern = -u"  and "multi-pattern search = -f"; put the output in outputbuffer */
#if	0
	strcpy(tempfile, INDEX_DIR);
	strcat(tempfile, "/");
	strcat(tempfile, NAME_LIST); 
	stat(tempfile, &st_buf);
	name_list_size = st_buf.st_size;
#else
	name_list_buffer = bigbuffer + MAX_PAT - 1;
	name_list_size = bigbuffer_size + 1;
#endif

	argv[0] = "glimpse";
	argv[1] = "-b";
	argv[2] = "-u";
	argv[3] = "-f";
	argv[4] = infile;
#if	0
	argv[5] = tempfile;
	argv[6] = "";
	errno = 0;
	if ((((ret = fileagrep(6, argv, outputbuffer_len, outputbuffer)) <= 0) || (agrep_outpointer <= 0)) && (errno != ERANGE)) {
		for (i=0; i<round(file_num, 8*sizeof(int)); i++)
			index_vect[i] = 0;
		return -1;
	}
#else
	argv[5] = "";
	errno = 0;
	if ((((ret = memagrep(5, argv,  name_list_size, name_list_buffer, outputbuffer_len, outputbuffer)) <= 0) || (agrep_outpointer <= 0)) && (errno != ERANGE)) {
		for (i=0; i<round(file_num, 8*sizeof(int)); i++)
			index_vect[i] = 0;
		return -1;
	}
#endif

	/*
	outputbuffer[agrep_outpointer] = '\0';
	printf("%s", outputbuffer);
	getchar();
	*/

	/* Check these offsets (outputbuffer[0..agrep_outpointer]) against those stored in NAME_LIST_INDEX and figure out the mask */
	/* Use the fact that NAME_LIST_INDEX has the beginning offset of each file name in NAME_LIST (ending offset of last filename = size of NAME_LIST) */
	strcpy(tempfile, INDEX_DIR);
	strcat(tempfile, "/");
	strcat(tempfile, NAME_LIST_INDEX);
	if ((fp = fopen(tempfile, "r")) == NULL) {
		for (i=0; i<round(file_num, 8*sizeof(int)); i++)
			index_vect[i] = 0;
		return -1;
	}
	fstat(fileno(fp), &st_buf);
	st_buf.st_size = (st_buf.st_size/sizeof(int)) * sizeof(int);	/* chop it off */
	for (i=0; i<round(file_num, 8*sizeof(int)); i++)
		multi_dest_index_set[0][i] = 0;

	if ((ret = fread(tempbuf, 1, sizeof(int), fp)) != sizeof(int)) {
		fclose(fp);
		for (i=0; i<round(file_num, 8*sizeof(int)); i++)
			index_vect[i] = 0;
		return -1;
	}
	num_read = ret;
	prevreadoffset = (tempbuf[0] << 24) | (tempbuf[1] << 16) | (tempbuf[2] << 8) | tempbuf[3];
	/* printf("prevreadoffset=%d name_list_size=%d\n", prevreadoffset, name_list_size); */
	count = 0;
	maxcount = 0;
	i = 0;
	j = 0;
	l = 0;
	while (j < agrep_outpointer) {	/* offsets will be printed by agrep always in the increasing order, so this is a one pass algorithm */
		k = j;
		while ((k<agrep_outpointer) && (isalnum(((unsigned char *)outputbuffer)[k]))) k++;
		outputbuffer[k] = '\0';
		offset = 0;
		errno = 0;
		offset = atoi(&outputbuffer[j]) + first_line_len - 1;	/* I have \n part of it included in name_list_buffer */
		/* printf("offset=%d\n", offset); */
		k++;
		while ((k<agrep_outpointer) && (!isalnum(((unsigned char *)outputbuffer)[k]))) k++;
		j = k;
		if ((offset <= first_line_len - 1) || (errno == ERANGE)) continue;
		found = 0;

		while (!found) {
			if (count >= maxcount) {
				if (num_read<st_buf.st_size) {
					if ((ret = fread(tempbuf, 1, MAX_LINE_LEN, fp)) <= 0) goto endofinput;
					num_read += ret;
					maxcount += ret/sizeof(int);

					for (i=0; i<ret; i+=sizeof(int), count++) {
						readoffset = (tempbuf[i] << 24) | (tempbuf[i+1] << 16) | (tempbuf[i+2] << 8) | tempbuf[i+3];
						/* printf("readoffset=%d\n", readoffset); */
						if ((offset >= prevreadoffset) && (offset < readoffset)) {
							/* printf("count=%d\n", count); */
							if (OneFilePerBlock)
								multi_dest_index_set[0][block2index(count)] |= mask_int[count % 32];
							else {
								for (; l<MAX_PARTITION; l++) {
									if ((count >= p_table[l]) && (count < p_table[l+1])) {
										multi_dest_index_set[0][l] = 1;
										break;	/* out of for */
									}
								}
								/* can't come here without break: if it does (serious!) will break out w/o setting anything */
							}
							prevreadoffset = readoffset;
							i += sizeof(int);
							count ++;
							found = 1;
							break;	/* out of for */
						}
						prevreadoffset = readoffset;
					}
				}
				else if ((offset >= prevreadoffset) && (offset < name_list_size)) {
					/* printf("count=%d\n", count); */
					if (OneFilePerBlock)
						multi_dest_index_set[0][block2index(count)] |= mask_int[count % 32];
					else {
						for (; l<MAX_PARTITION; l++) {
							if ((count >= p_table[l]) && (count < p_table[l+1])) {
								multi_dest_index_set[0][l] = 1;
								break;	/* out of for */
							}
						}
						/* can't come here without break: if it does (serious!) will break out without setting anything */
					}
					count ++;
					found = 1;
				}
				else goto endofinput;	/* since this offset >= name_list_size and there's no more input after that */
			}
			else {
				for (; i<ret; i+=sizeof(int), count++) {
					readoffset = (tempbuf[i] << 24) | (tempbuf[i+1] << 16) | (tempbuf[i+2] << 8) | tempbuf[i+3];
					/* printf("readoffset=%d\n", readoffset); */
					if ((offset >= prevreadoffset) && (offset < readoffset)) {
						/* printf("count=%d\n", count); */
						if (OneFilePerBlock)
							multi_dest_index_set[0][block2index(count)] |= mask_int[count % 32];
						else {
							for (; l<MAX_PARTITION; l++) {
								if ((count >= p_table[l]) && (count < p_table[l+1])) {
									multi_dest_index_set[0][l] = 1;
									break;	/* out of for */
								}
							}
							/* can't come here without break: if it does (serious!) will break out without setting anything */
						}

						prevreadoffset = readoffset;
						i += sizeof(int);
						count ++;
						found = 1;
						break;	/* out of for */
					}
					prevreadoffset = readoffset;
				}
			}
		}
	}

endofinput:
	/* Now AND the incoming mask with the one constructed above */
	if (OneFilePerBlock) {
		for (i=0; i<round(file_num, 8*sizeof(int)); i++)
			index_vect[i] &= multi_dest_index_set[0][i];
	}
	else {
		for (i=0; i<MAX_PARTITION; i++)
			index_vect[i] &= multi_dest_index_set[0][i];
	}
	fclose(fp);
	return 0;
}

get_filenames(index_vect, argc, argv, dummylen, dummypat, file_num)
int  *index_vect;
int argc; /* the arguments to agrep for -F */
char *argv[];
int dummylen;
CHAR dummypat[];
int file_num;
{
	int  i=0,j, ret;
        int  start, end, k, prevk;
	int filesseen;
	char *beginptr, *endptr;
	char tempbuf[MAX_PAT * 3];

#if	BG_DEBUG
	fprintf(debug, "get_filenames(): the following partitions are ON\n");
	for(i=0; i<((OneFilePerBlock > 0) ? round(file_num, 8*sizeof(int)) : MAX_PARTITION); i++)
		if(index_vect[i]) fprintf(debug, "i=%d,%x\n", i, index_vect[i]);
#endif	/*BG_DEBUG*/

	GNumfiles = 0;
	filesseen = 0;
	endptr = beginptr = bigbuffer + MAX_PAT;

	if(MATCHFILE == OFF) {	/* just copy the filenames */
	    if (OneFilePerBlock) {
		for (i=0; i<round(file_num, 8*sizeof(int)); i++) {
		    if (index_vect[i] == 0) continue;
		    for (j=0; j<8*sizeof(int); j++) {
			if (!(index_vect[i] & mask_int[j])) continue;
			start = i*8*sizeof(int) + j;
			end = start + 1;
#if	BG_DEBUG
			fprintf(debug, "start=%d, end=%d\n", start, end);
#endif	/*BG_DEBUG*/
			/*
			 * skip over so many filenames and get the filenames to copy.
			 * NOTE: successive "start"s ALWAYS increase.
			 */

			while(filesseen < start) {
				while(*beginptr != '\n') beginptr ++;
				beginptr ++;	/* skip over '\n' */
				filesseen ++;
			}

			endptr = beginptr;
			while (filesseen < end) {
				while(*endptr != '\n') endptr ++;
				if (endptr == beginptr + 1) goto end_of_loop1;	/* null name of non-existent file */
				*endptr = '\0';
				/* return with all the names you COULD get */
				if ((GTextfiles[GNumfiles] = (CHAR *)strdup(beginptr)) == NULL) {
					*endptr = '\n';
					fprintf(stderr, "Out of memory at: %s:%d\n", __FILE__, __LINE__);
					return;
				}
				GFileIndex[GNumfiles] = i*8*sizeof(int) + j;
				*endptr = '\n';
				if (++GNumfiles >= file_num) goto end_files;
			end_of_loop1:
				beginptr = endptr = endptr + 1;	/* skip over '\n' */
				filesseen ++;
			}
		    }
		}
	    } /* one file per block */
	    else {
		/* Just the outer for-loop and initial begin/end values are different: rest is same */
		for (i=0; i<MAX_PARTITION; i++) {
		    if(index_vect[i] > 0) {
			start = p_table[i];
			end = p_table[i+1];
			if (start >= end) continue;
#if	BG_DEBUG
			fprintf(debug, "start=%d, end=%d\n", start, end);
#endif	/*BG_DEBUG*/
			/*
			 * skip over so many filenames and get the filenames to copy.
			 * NOTE: successive "start"s ALWAYS increase.
			 */

			while(filesseen < start) {
				while(*beginptr != '\n') beginptr ++;
				beginptr ++;	/* skip over '\n' */
				filesseen ++;
			}

			endptr = beginptr;
			while (filesseen < end) {
				while(*endptr != '\n') endptr ++;
				if (endptr == beginptr + 1) goto end_of_loop2;	/* null name of non-existent file */
				*endptr = '\0';
				/* return with all the names you COULD get */
				if ((GTextfiles[GNumfiles] = (CHAR *)strdup(beginptr)) == NULL) {
					*endptr = '\n';
					fprintf(stderr, "Out of memory at: %s:%d\n", __FILE__, __LINE__);
					return;
				}
				GFileIndex[GNumfiles] = filesseen;
				*endptr = '\n';
				if (++GNumfiles >= file_num) goto end_files;
			end_of_loop2:
				beginptr = endptr = endptr + 1;	/* skip over '\n' */
				filesseen ++;
			}
		    }
		}
	    }
	}
	else {	/* search and copy matched filenames */
	    extern int REGEX, FASTREGEX, D, WORDBOUND;	/* agrep global which tells us whether the pattern is a regular expression or not, and if there are errors w/ -w */
	    int myREGEX, myFASTREGEX, myD, myWORDBOUND;
	    errno = 0;
	    if ((dummylen = memagrep_init(argc, argv, MAX_PAT, dummypat)) <= 0) goto end_files;
	    memcpy(tempbuf, bigbuffer, bigbuffer_size >= MAX_PAT ? MAX_PAT*3 : MAX_PAT*2 + bigbuffer_size);
	    ret = memagrep_search(dummylen, dummypat, dummylen*2, beginptr, outputbuffer_len, outputbuffer);
	    memcpy(bigbuffer, tempbuf, bigbuffer_size >= MAX_PAT ? MAX_PAT*3 : MAX_PAT*2 + bigbuffer_size);
	    myREGEX = REGEX; myFASTREGEX = FASTREGEX; myD = D; myWORDBOUND = WORDBOUND;

	    if (OneFilePerBlock) {
		for (i=0; i<round(file_num, 8*sizeof(int)); i++) {
		    if (index_vect[i] == 0) continue;
		    for (j=0; j<8*sizeof(int); j++) {
			if (!(index_vect[i] & mask_int[j])) continue;
			start = i*8*sizeof(int) + j;
			end = start + 1;
#if	BG_DEBUG
			fprintf(debug, "start=%d, end=%d\n", start, end);
#endif	/*BG_DEBUG*/
			/*
			 * skip over so many filenames and get the region to search =
			 * beginptr to endptr: NOTE: successive "start"s ALWAYS increase.
			 */

			while(filesseen < start) {
				while(*beginptr != '\n') beginptr ++;
				beginptr ++;	/* skip over '\n' */
				filesseen ++;
			}
			beginptr --;	/* I need '\n' for memory search */

			endptr = beginptr+1;
			while (filesseen < end) {
				while(*endptr != '\n') endptr ++;
				endptr ++;	/* skip over '\n' */
				filesseen ++;
			}
			endptr --;	/* I need '\n' for memory search */
			if (endptr == beginptr + 1) goto end_of_loop3;	/* null name of non-existent file */

#if	BG_DEBUG
			*endptr = '\0';
			fprintf(debug, "From %d searching:\n%s\n", filesseen, beginptr+1);
			*endptr = '\n';
#endif	/*BG_DEBUG*/

			/* if file in the partition matches then copy it */
#if	EACHOPTION
			if (myREGEX || myFASTREGEX || (myD && myWORDBOUND)) ret = memagrep_search(dummylen, dummypat, endptr-beginptr + 1, beginptr, outputbuffer_len, outputbuffer);
			else ret = memagrep_search(dummylen, dummypat, endptr-beginptr/* + 1*/, beginptr+1, outputbuffer_len, outputbuffer);
#else
			ret = memagrep_search(dummylen, dummypat, endptr-beginptr+1, beginptr, outputbuffer_len, outputbuffer);

#endif
			if (ret > 0) {
#if	BG_DEBUG
			    {
				char c = outputbuffer[agrep_outpointer + 1];
				outputbuffer[agrep_outpointer + 1] = '\0';
				fprintf(debug, "OUTPUTBUFFER=%s\n", outputbuffer);
				outputbuffer[agrep_outpointer + 1] = c;
			    }
#endif	/*BG_DEBUG*/
			    k = prevk = 0;
#if	EACHOPTION
#else
			    while (outputbuffer[k] == '\n') {
				k ++; prevk ++;
			    }
#endif
			    while(k+1<agrep_outpointer) {	/* name of a file cannot have '\n' in it */
				k++;
				if (outputbuffer[k] == '\n') {
					outputbuffer[k] = '\0';
					/* return with all the names you COULD get */
					if ((GTextfiles[GNumfiles] = (CHAR *)strdup(outputbuffer+prevk)) == NULL) {
						outputbuffer[k] = '\n';
						fprintf(stderr, "Out of memory at: %s:%d\n", __FILE__, __LINE__);
						return;
					}
					outputbuffer[k] = '\n';
					GFileIndex[GNumfiles] = i*8*sizeof(int)+j;
					if (++GNumfiles >= file_num) goto end_files;
					k = prevk = k+1;
				}
			    }
			}
			else {
			    index_vect[i] &= ~mask_int[j];	/* remove it from the list: used if ByteLevelIndex */
			}

		    end_of_loop3:
			beginptr = endptr = endptr + 1;
		    }
		}
	    } /* one file per block */
	    else {
		/* Just the outer for-loop and initial begin/end values are different: rest is same */
		for (i=0; i<MAX_PARTITION; i++) {
		    if(index_vect[i] > 0) {
			start = p_table[i];
			end = p_table[i+1];
			if (start >= end) continue;
#if	BG_DEBUG
			fprintf(debug, "start=%d, end=%d\n", start, end);
#endif	/*BG_DEBUG*/
			/*
			 * skip over so many filenames and get the region to search =
			 * beginptr to endptr: NOTE: successive "start"s ALWAYS increase.
			 */

			while(filesseen < start) {
				while(*beginptr != '\n') beginptr ++;
				beginptr ++;	/* skip over '\n' */
				filesseen ++;
			}
			beginptr --;	/* I need '\n' for memory search */

			endptr = beginptr+1;
			while (filesseen < end) {
				while(*endptr != '\n') endptr ++;
				endptr ++;	/* skip over '\n' */
				filesseen ++;
			}
			endptr --;	/* I need '\n' for memory search */
			if (endptr == beginptr + 1) goto end_of_loop4;	/* null name of non-existent file */

#if	BG_DEBUG
			*endptr = '\0';
			fprintf(debug, "From %d searching:\n%s\n", filesseen, beginptr+1);
			*endptr = '\n';
#endif	/*BG_DEBUG*/

			/* if file in the partition matches then copy it */
#if	EACHOPTION
			if (myREGEX || myFASTREGEX || (myD && myWORDBOUND)) ret = memagrep_search(dummylen, dummypat, endptr-beginptr + 1, beginptr, outputbuffer_len, outputbuffer);
			else ret = memagrep_search(dummylen, dummypat, endptr-beginptr/* + 1*/, beginptr+1, outputbuffer_len, outputbuffer);
#else
			/* beginptr points to '\n', entptr+1 points to '\n' */
			ret = memagrep_search(dummylen, dummypat, endptr-beginptr+1, beginptr, outputbuffer_len, outputbuffer);
#endif
			if (ret > 0) {
			    k = prevk = 0;
#if	EACHOPTION
#else
			    while (outputbuffer[k] == '\n') {
				k ++; prevk ++;
			    }
#endif
			    while(k+1<agrep_outpointer) {	/* name of a file cannot have '\n' in it */
				k++;
				if (outputbuffer[k] == '\n') {
					outputbuffer[k] = '\0';
					/* return with all the names you COULD get */
					if ((GTextfiles[GNumfiles] = (CHAR *)strdup(outputbuffer+prevk)) == NULL) {
						outputbuffer[k] = '\n';
						fprintf(stderr, "Out of memory at: %s:%d\n", __FILE__, __LINE__);
						return;
					}
					outputbuffer[k] = '\n';
					GFileIndex[GNumfiles] = filesseen - 1;	/* not sure here which one but this is never used so ok to fill junk */
					if (++GNumfiles >= file_num) goto end_files;
					k = prevk = k+1;
				}
			    }
			}
			else {
			    index_vect[i] = 0;	/* mask it off */
			}

		    end_of_loop4:
			beginptr = endptr = endptr + 1;
		    }
		}
	    }
	}

end_files:
#if	BG_DEBUG
	fprintf(debug, "The following %d filenames are ON\n", GNumfiles);
	for (i=0; i<GNumfiles; i++)
		fprintf(debug, "\t%s\n", GTextfiles[i]);
#endif	/*BG_DEBUG*/
	return;
}

