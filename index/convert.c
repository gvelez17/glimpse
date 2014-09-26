/**********************************************************************************************/
/* convert.c: Program to inter-convert different representations of neighbourhood sets        */
/*											      */
/* Uses: to compress neighbourhood sets for faster search/uncompress for viewing/editing them */
/* Author: Burra Gopal, bgopal@cs.arizona.edu, Sep 7-8 1996: WebGlimpse support               */
/**********************************************************************************************/

#include <autoconf.h>	/* configured defs */

#include "glimpse.h"
#include <stdlib.h>
#include <sys/time.h>
#if	ISO_CHAR_SET
#include <locale.h>			/* support for 8bit character set:ew@senate.be */
#endif
#include <errno.h>


#define IS_LITTLE_ENDIAN	1
#define IS_BIG_ENDIAN		0
#define IS_INDICES		1
#define IS_BITS			2
#define IS_NAMES		3
#define USUALBUFFER_SIZE	(MAX_LINE_LEN*64)


/* Exported routines */
int		element2name(/*int, out char*, int, int, int*/);
int		mem_element2name(/*int, char*, unsigned char*, unsigned char*, int*/);
int		name2element(/*out int*, char*, int, int, int, int*/);
int		mem_name2element(/*out int*, char*, int, unsigned char*, unsigned char*, int*/);
int		do_conversion(/*FILE*, FILE*, int, int, int, int, int, unsigned int *, int, int*/);
int		change_format(/*int, int, int, int, int, int, char *, char **/);


/* Imported routines */
int		hashNk(/*char *, int*/);/* from io.c */


/* Internal routines */
int		discardinfo(/*char **/);
int		allocate_and_fill(/* out unsigned char **, int, char *, int*/);


/* Imported variables */
extern int	errno;
extern int	get_index_type();	/* from io.c */
extern int	file_num;		/* from io.c */
extern int	mask_int[32];		/* from io.c */
extern int	BigFilenameHashTable;	/* from io.c */
extern int	InfoAfterFilename;	/* from io.c */


/* Internal variables */

/* Variables related to options (i/p-->o/p types)*/
int		InputType, OutputType, InputEndian, OutputEndian, InputFilenames, ReadIntoMemory;
char		glimpseindex_dir[MAX_LINE_LEN];
char		filename_prefix[MAX_LINE_LEN];

/* Variables related to ReadIntoMemory option (I/O efficiency) */
unsigned char	*filenames_buffer, *filenames_index_buffer, *filehash_buffer, *filehash_index_buffer;
int		filenames_len, filenames_index_len, filehash_len, filehash_index_len;
int		fdname, fdname_index, fdhash, fdhash_index;
unsigned char	usualbuffer[USUALBUFFER_SIZE];

/* Variables for statistics */
int		hash_misses = 0;

/********************************************************
 * Discards information after ' ' in filename           *
 * Returns: 0 if it found info to discard, -1 otherwise *
 * Assumes: file ends with '\0'                         *
 * CHANGED from ' ' to FILE_END_MARK 6/7/99 --GB	*
 ********************************************************/
int
discardinfo(file)
	char		file[];
{

	int		k;

	if (InfoAfterFilename) {
		k = 0;
		while (file[k] != '\0') {
			if (file[k] == '\\') {
				k ++;
				if (file[k] == '\0') break;
				k++;
				continue;
			}
			else {
				if (file[k] == FILE_END_MARK) {
					file[k] = '\0';
					return 0;
				}
				k++;
				continue;
			}
		}
	}
	/* pab23feb98: return -1 if !InfoAfterFilename */
	return -1;
}

/********************************************************************************************
 * Allocates the "buffer" of size "len" and fills it up with "len" amount of data from "fd" *
 * Returns: 0 on success, -1 on failure (i.e., if allocation fails or can't read fully)     *
 ********************************************************************************************/
int
allocate_and_fill(buffer, len, filename, fd)
	unsigned char	**buffer;
	int		len;
	char		*filename;
	int		fd;
{

	if ((len <= 0) || ((*buffer = (unsigned char *)my_malloc(len)) == NULL)) {
		fprintf(stderr, "Disable -M option: cannot allocate memory for %s\n", filename);
		return -1;
	}
	if (len != read(fd, *buffer, len)) {
		fprintf(stderr, "Disable -M option: cannot read %s\n", filename);
		return -1;
	}
	return 0;
}

/**************************************************************************************
 * Finds filename for given element (index#:  every element points to indexed object) *
 * Returns: -1 if error and 0 on success                                              *
 * See glimpse/index/io.c/save_datastructures() for the format of the names-file      *
 **************************************************************************************/
int
element2name(element, file, fd, fdi, files_used)
	int		element;
	char		file[];		/* out */
	int		fd, fdi;	/* fd=filenames fd, fdi=filenames_index fd */
	int		files_used;
{

	int		k, offset, lastoffset = -1, len;
	unsigned char	array[4];

	if ((element < 0) || (element >= files_used)) {
		errno = EINVAL;
		return -1;
	}
	lseek(fdi, (long)element*4, SEEK_SET);
	if (read(fdi, array, 4) != 4) {
		errno = ENOENT;
		return -1;
	}
	offset = (array[0] << 24) | (array[1] << 16) | (array[2] << 8) | array[3];
	if (read(fdi, array, 4) == 4) {
		lastoffset = (array[0] << 24) | (array[1] << 16) | (array[2] << 8) | array[3];
	}

	if (lseek(fd, (long)offset, SEEK_SET) == -1) {
		fprintf(stderr, ".glimpse_filenames: can't seek to %d\n", offset);
		return -1;
	}
	if (lastoffset != -1)
		len = read(fd, file, lastoffset - offset);
	else len = read(fd, file, MAX_LINE_LEN);
	if (len == -1) {
		errno = ENOENT;
		return -1;
	}
	file[len - 1] = '\0';	/* separated by '\n', so zero that out: if empty file, will get its strlen() to be 0, as expected */
	if (InfoAfterFilename) discardinfo(file);
	return 0;
}

/**************************************************************************************
 * Finds filename for given element (index#:  every element points to indexed object) *
 * Returns: -1 if error and 0 on success                                              *
 * See glimpse/index/io.c/save_datastructures() for the format of the names-file      *
 * Works by reading in-memory copy of the files 				      *
 **************************************************************************************/
int
mem_element2name(element, file, filenames_buffer, filenames_index_buffer, files_used)
	int		element;
	char		file[];		/* out */
	unsigned char	*filenames_buffer, *filenames_index_buffer;
	int		files_used;
{

	int		i, offset, lastoffset = -1, len;

	if ((element < 0) || (element >= files_used) || (element >= filenames_index_len)) {
		errno = EINVAL;
		return -1;
	}
	i = element*4;
	offset = (filenames_index_buffer[i] << 24) | (filenames_index_buffer[i+1] << 16) |
			(filenames_index_buffer[i+2] << 8) | filenames_index_buffer[i+3];
	if (element == files_used - 1) lastoffset = filenames_len;
	else lastoffset = (filenames_index_buffer[i+4] << 24) | (filenames_index_buffer[i+5] << 16) |
				(filenames_index_buffer[i+6] << 8) | filenames_index_buffer[i+7];
/* fprintf(stderr, "element=%d offset=%d, lastoffset=%d, filenames_len=%d, files_used=%d\n", element, offset, lastoffset, filenames_len, files_used); */
	if ((offset < 0) || (offset > filenames_len) || (lastoffset < 0) || (lastoffset > filenames_len) || (offset >= lastoffset)) {
		errno = ENOENT;
		return -1;
	}
	if (lastoffset - offset >= MAX_LINE_LEN) {
		errno = EINVAL;
		return -1;
	}
	memcpy(file, &filenames_buffer[offset], lastoffset-offset);
	file[lastoffset - offset - 1] = '\0';	/* separated by '\n', so zero that out: if empty file, will get its strlen() to be 0, as expected */
	if (InfoAfterFilename) discardinfo(file);
	return 0;
}

/*****************************************************************************************
 * Returns: element (index#) for given filename (every element points to indexed object) *
 * Returns: -1 if error (assuming that element#s are >= 0, ofcourse...)                  *
 * See glimpse/index/io.c/save_datastructures() for the format of the hash-file          *
 *****************************************************************************************/
int
name2element(pelement, file, len, fd, fdi, files_used)
	int		*pelement;	/* out */
	char		file[];
	int		len;
	int 		fd, fdi;	/* fd=filehash fd, fdi=filehash_index fd */
	int		files_used;
{

	int		malloced = 0, ret, i, k, foundblank=0, offset, lastoffset = -1, hash, size;
	unsigned char	*buffer, array[4];

	if ((len <= 0) || (len >= MAX_LINE_LEN)) {
		errno = EINVAL;
		return -1;
	}
	hash = hashNk(file, len);
/* fprintf(stderr, "len=%d file=%s hash=%d\n", len, file, hash); */
	if (lseek(fdi, (long)hash*4, SEEK_SET) == -1) {
		fprintf(stderr, ".glimpse_filehash_index: can't seek to %d\n", hash*4);
		return -1;
	}
	if ((ret = read(fdi, array, 4)) != 4) {
		fprintf(stderr, "read only %d bytes from %d\n", ret, hash*4);
		errno = ENOENT;
		return -1;
	}
	offset = (array[0] << 24) | (array[1] << 16) | (array[2] << 8) | array[3];
/* fprintf(stderr, "offset=%d\n", offset); */
	if (read(fdi, array, 4) == 4) {
		lastoffset = (array[0] << 24) | (array[1] << 16) | (array[2] << 8) | array[3];
	}
	else lastoffset = lseek(fd, (long)0, SEEK_END /*2*/ /* from end */);	/* so that next time I get prev-value = file size */
/* fprintf(stderr, "lastoffset=%d\n", lastoffset); */
	size = lastoffset - offset;
	if (size <= 1) {
		errno = ENOENT;
		return -1;
	}
	if (size < USUALBUFFER_SIZE) buffer = usualbuffer;
	else {
		buffer = (unsigned char *)my_malloc(size);
		malloced = 1;
	}
/* fprintf(stderr, "hash=%d offset=%d lastoffset=%d size=%d\n", hash, offset, lastoffset, size); */

	lseek(fd, (long)offset, SEEK_SET);
	if (size != read(fd, buffer, size)) {
		if (malloced) my_free((char *)buffer, size);
		errno = ENOENT;
		return -1;
	}
/* fprintf(stderr, "buffer=%s\n", buffer+4); */
	for (i=0; i<size; i+=4+strlen((char *)&buffer[i+4])+1) {
		if (InfoAfterFilename) {
			k = i+4;
			while (buffer[k] != '\0') {
				if (buffer[k] == '\\') {
					k ++;
					if (buffer[k] == '\0') break;
					k++;
					continue;
				}
				else {
					if (buffer[k] == FILE_END_MARK) {
						buffer[k] = '\0';
						foundblank = 1;
						break;
					}
					k++;
					continue;
				}
			}
		}
		if (!strcmp((char *)&buffer[i+4], file)) {
			*pelement = (buffer[i] << 24) | (buffer[i+1] << 16) | (buffer[i+2] << 8) | buffer[i+3];
			if (InfoAfterFilename && foundblank) {
				buffer[k] = FILE_END_MARK;
			}
			if (malloced) my_free((char *)buffer, size);
			return 0;
		}
		if (InfoAfterFilename && foundblank) {
			buffer[k] = FILE_END_MARK;
		}
		hash_misses ++;
	}
	if (malloced) my_free((char *)buffer, size);
	errno = ENOENT;
	return -1;
}

/*****************************************************************************************
 * Returns: element (index#) for given filename (every element points to indexed object) *
 * Returns: -1 if error (assuming that element#s are >= 0, ofcourse...)                  *
 * See glimpse/index/io.c/save_datastructures() for the format of the hash-file          *
 * Works by reading in-memory copy of the files						 *
 *****************************************************************************************/
mem_name2element(pelement, file, len, filehash_buffer, filehash_index_buffer, files_used)
	int		*pelement;	/* out */
	char		*file;
	int		len;
	unsigned char	*filehash_buffer, *filehash_index_buffer;
	int		files_used;
{

	int		lasti, ret, i, k, foundblank=0, offset, lastoffset = -1, hash, size;
	unsigned char	*buffer;

	if ((len <= 0) || (len >= MAX_LINE_LEN)) {
		errno = EINVAL;
		return -1;
	}
	hash = hashNk(file, len);
	i = hash*4;
	offset = (filehash_index_buffer[i] << 24) | (filehash_index_buffer[i+1] << 16) |
			(filehash_index_buffer[i+2] << 8) | filehash_index_buffer[i+3];
	if (BigFilenameHashTable) lasti = MAX_64K_HASH - 1;
	else lasti = MAX_4K_HASH - 1;
	if (i == lasti) lastoffset = filehash_len;
	else lastoffset = (filehash_index_buffer[i+4] << 24) | (filehash_index_buffer[i+5] << 16) |
				(filehash_index_buffer[i+6] << 8) | filehash_index_buffer[i+7];
	if ((offset < 0) || (offset > filehash_len) || (lastoffset < 0) || (lastoffset > filehash_len) || (offset >= lastoffset)) {
		errno = ENOENT;
		return -1;
	}
	size = lastoffset - offset;
	if (size <= 1) {
		errno = ENOENT;
		return -1;
	}
/* fprintf(stderr, "hash=%d offset=%d lastoffset=%d size=%d\n", hash, offset, lastoffset, size); */

	buffer = &filehash_buffer[offset];
	for (i=0; i<size; i+=4+strlen((char *)&buffer[i+4])+1) {
		if (InfoAfterFilename) {
			k = i+4;
			while (buffer[k] != '\0') {
				if (buffer[k] == '\\') {
					k ++;
					if (buffer[k] == '\0') break;
					k++;
					continue;
				}
				else {
					if (buffer[k] == FILE_END_MARK) {
						buffer[k] = '\0';
						foundblank = 1;
						break;
					}
					k++;
					continue;
				}
			}
		}
		if (!strcmp((char *)&buffer[i+4], file)) {
			*pelement = (buffer[i] << 24) | (buffer[i+1] << 16) | (buffer[i+2] << 8) | buffer[i+3];
			if (InfoAfterFilename && foundblank) {
				buffer[k] = FILE_END_MARK;
			}
			return 0;
		}
		if (InfoAfterFilename && foundblank) {
			buffer[k] = FILE_END_MARK;
		}
		hash_misses ++;
	}
	errno = ENOENT;
	return -1;
}

/********************************************************************************************
 * Converts format of one file "inputfile" to another "outputfile"                          *
 * Returns: always 0 for now indicating there was no error: might want to modify this later *
 * Uses global file descriptors (fdname....) and memory buffers (filenames_buffer/len...)   *
 ********************************************************************************************/
int
do_conversion(inputfile, outputfile, indextype, InputType, OutputType, InputEndian, OutputEndian, index_set, index_set_size, ReadIntoMemory)
	FILE		*inputfile;
	FILE		*outputfile;
	int		indextype;
	int		InputType;
	int		OutputType;
	int		InputEndian;
	int		OutputEndian;
	unsigned int	*index_set;
	unsigned int	index_set_size;
	int		ReadIntoMemory;
{

	int		i, j, m = 0, name_len, ret;
	int		nextchar;
	char		s[MAX_LINE_LEN];
	char		name[MAX_LINE_LEN];
	char		outname[MAX_LINE_LEN];
	struct stat	istbuf;

	memset(index_set, '\0', index_set_size * sizeof(unsigned int));	/* zero out bits set in a previous call to this function ... */

	/* Do actual conversion */
	if (InputType == IS_NAMES) {
		while (fgets(name, MAX_LINE_LEN, inputfile) != NULL) {
			name_len = strlen(name);
			name[name_len - 1] = '\0';	/* discard '\n' */
			if (InfoAfterFilename) discardinfo(name);
			name_len = strlen(name);
			if (ReadIntoMemory) ret = mem_name2element(&i, name, name_len, filehash_buffer, filehash_index_buffer, file_num);
			else ret = name2element(&i, name, name_len, fdhash, fdhash_index, file_num);
			if (ret != -1) {
/* fprintf(stderr, "%s-->%d %x\n", name, i, mask_int[i%(8*sizeof(int))]); */
				index_set[block2index(i)] |= mask_int[i%(8*sizeof(int))];
				if (OutputType == IS_INDICES) {	/* indices is always bigendian */
					putc(((i & 0xff000000) >> 24)&0xff, outputfile);
					putc(((i & 0x00ff0000) >> 16)&0xff, outputfile);
					putc(((i & 0x0000ff00) >> 8)&0xff, outputfile);
					putc((i & 0x000000ff), outputfile);
				}
			}
		}
		if (OutputType == IS_BITS) {
			for (i=0; i<index_set_size; i++) {
				if (OutputEndian == IS_BIG_ENDIAN) {
					putc(((index_set[i] & 0xff000000) >> 24)&0xff, outputfile);
					putc(((index_set[i] & 0x00ff0000) >> 16)&0xff, outputfile);
					putc(((index_set[i] & 0x0000ff00) >> 8)&0xff, outputfile);
					putc((index_set[i] & 0x000000ff), outputfile);
				}
				else if (OutputEndian == IS_LITTLE_ENDIAN) {	/* little */
					putc((index_set[i] & 0x000000ff), outputfile);
					putc(((index_set[i] & 0x0000ff00) >> 8)&0xff, outputfile);
					putc(((index_set[i] & 0x00ff0000) >> 16)&0xff, outputfile);
					putc(((index_set[i] & 0xff000000) >> 24)&0xff, outputfile);
				}
			}
		}
	}

	else if (InputType == IS_INDICES) {	/* indices is always bigendian */
		while ((nextchar = getc(inputfile)) != EOF) {
			nextchar = nextchar & 0xff;
			i = nextchar << 24;
			if ((nextchar = getc(inputfile)) == EOF) break;
			nextchar = nextchar & 0xff;
			i |= nextchar << 16;
			if ((nextchar = getc(inputfile)) == EOF) break;
			nextchar = nextchar & 0xff;
			i |= nextchar << 8;
			if ((nextchar = getc(inputfile)) == EOF) break;
			nextchar = nextchar & 0xff;
			i |= nextchar;
			if (indextype != 0) {
				if (i < file_num) index_set[block2index(i)] |= mask_int[i%(8*sizeof(int))];
			}
			else {
				if (i < MAX_PARTITION) index_set[i] = 1;
			}
			if (OutputType == IS_NAMES) {
				if (ReadIntoMemory) ret = mem_element2name(i, outname, filenames_buffer, filenames_index_buffer, file_num);
				else ret = element2name(i, outname, fdname, fdname_index, file_num);
				if (ret != -1) fprintf(outputfile, "%s\n", outname);
			}
		}
		if (OutputType == IS_BITS) {
			for (i=0; i<index_set_size; i++) {
				if (OutputEndian == IS_BIG_ENDIAN) {
					putc(((index_set[i] & 0xff000000) >> 24)&0xff, outputfile);
					putc(((index_set[i] & 0x00ff0000) >> 16)&0xff, outputfile);
					putc(((index_set[i] & 0x0000ff00) >> 8)&0xff, outputfile);
					putc((index_set[i] & 0x000000ff), outputfile);
				}
				else if (OutputEndian == IS_LITTLE_ENDIAN) {	/* little */
					putc((index_set[i] & 0x000000ff), outputfile);
					putc(((index_set[i] & 0x0000ff00) >> 8)&0xff, outputfile);
					putc(((index_set[i] & 0x00ff0000) >> 16)&0xff, outputfile);
					putc(((index_set[i] & 0xff000000) >> 24)&0xff, outputfile);
				}
			}
		}
	}

	else if (InputType == IS_BITS) {
		i = 0;
		while ((i < sizeof(int) * index_set_size) && (nextchar = getc(inputfile)) != EOF) {
			nextchar = nextchar & 0x000000ff;
/* fprintf(stderr, "nextchar=%x\n", nextchar); */
			if (indextype != 0) {
				if (InputEndian == IS_LITTLE_ENDIAN) {	/* little-endian: little end of integer was dumped first in bitfield_file */
					index_set[i/4] |= (nextchar << (8*(i%4)));
				}
				else if (InputEndian == IS_BIG_ENDIAN) {	/* big-endian: big end of integer is first was dumped first in bitfield_file */
					index_set[i/4] |= (nextchar << (8*(4-1-(i%4))));
				}
			}
			else {
				if (i < MAX_PARTITION) {	/* interpretation of "bit" changes without OneFilePerBlock */
					index_set[i] = (nextchar != 0) ? 1 : 0;
				}
				else break;	/* BITFIELDLENGTH, by above definition, is always > MAX_PARTITION: see io.c */
			}
			i++;
		}
		for (i=0; i<index_set_size; i++) {
/* fprintf(stderr, "\nindex_set[%d]=%x\n", i, index_set[i]); */
			for (j=0; j<sizeof(int)*8; j++) {
				if (index_set[i] & mask_int[j]) {
/* fprintf(stderr, " %d", j); */
					m = i*sizeof(int)*8 + j;
					if (OutputType == IS_NAMES) {
						if (ReadIntoMemory) ret = mem_element2name(m, outname, filenames_buffer, filenames_index_buffer, file_num);
						else ret = element2name(m, outname, fdname, fdname_index, file_num);
						if (ret != -1) fprintf(outputfile, "%s\n", outname);
					}
					else if (OutputType == IS_INDICES) {	/* indices is always bigendian */
						putc((m&0xff000000)>>24, outputfile);
						putc((m&0x00ff0000)>>16, outputfile);
						putc((m&0x0000ff00)>>8, outputfile);
						putc((m&0x000000ff), outputfile);
					}
				}
			}
		}
	}

	return 0;
}

/**********************************************************************
 * Calls do_conversion() to convert storage format of a set of files; *
 * Optimizes some cases by reading important files into memory.       *
 * Returns: 0 on success, -1 on failure                               *
 **********************************************************************/
int
change_format(InputFilenames, ReadIntoMemory, InputType, OutputType, InputEndian, OutputEndian, glimpseindex_dir, filename_prefix)
	int		InputFilenames;
	int		ReadIntoMemory;
	int		InputType;
	int		OutputType;
	int		InputEndian;
	int		OutputEndian;
	char		*glimpseindex_dir;
	char		*filename_prefix;
{

	char		outname[MAX_LINE_LEN];		/* place where converted output is stored */
	char		s[MAX_LINE_LEN];		/* temp buffer */
	char		realname[MAX_LINE_LEN];		/* name after prefix of neighbourhood file is added to it */
	char		name[MAX_LINE_LEN];		/* name of file gotten from stdin: only if (InputFilenames) */
	int		lastslash, name_len, indextype, indexnumber, structuredindex, recordlevelindex, temp_attr_num, bytelevelindex;	/*indextype*/
	int		i, ret;				/* for-loop/return-value */
	int		num_input_filenames;		/* for statistics */
	char		temp_rdelim[MAX_LINE_LEN];	/*indextype*/
	struct stat	istbuf;				/*indexstat*/
	struct stat	fstbuf;				/*filestat*/
	unsigned int	*index_set, index_set_size;	/*neighbourhood's bitmap representation*/
	FILE		*inputfile, *outputfile;	/*file to be converted/file to store converted output: only if (InputFilenames) */

	/* Options set: read index */
	sprintf(s, "%s/%s", glimpseindex_dir, INDEX_FILE);
	if (-1 == stat(s, &istbuf)) {
		fprintf(stderr, "Cannot find index in directory `%s'\n\tuse `-H dir' to specify a glimpse index directory\n", glimpseindex_dir);
		return usage();
	}
	/* Find out existing index of words and partitions/filenumbers */
	indextype = get_index_type(s, &indexnumber, &indextype, &structuredindex, temp_rdelim);
	if (structuredindex == -2) {
	    recordlevelindex = 1;
	    bytelevelindex = 1;
	}
	if (structuredindex <= 0) structuredindex = 0;
	else {
	    temp_attr_num = structuredindex;
	    structuredindex = 1;
	}
	if (indextype == 0) {
		file_num = MAX_PARTITION;	/*tiny*/
		index_set_size = MAX_PARTITION;
	}
	else {
		if (indextype > 0) file_num = indextype;	/*small*/
		else file_num = -indextype;	/*medium*/
		index_set_size = ((file_num + 8*sizeof(int) - 1)/(8*sizeof(int)));
	}
	index_set = (unsigned int *)my_malloc(index_set_size * sizeof(unsigned int));
	memset(index_set, '\0', index_set_size * sizeof(unsigned int));

	sprintf(name, "%s/%s", glimpseindex_dir, NAME_LIST);
	if ((fdname = open(name, O_RDONLY, 0)) == -1) {
		fprintf(stderr, "Cannot open for reading: %s\n", name);
		return -1;
	}
	fstbuf.st_size = 0;
	fstat(fdname, &fstbuf);
	if (ReadIntoMemory) {
		filenames_len = fstbuf.st_size;
		filenames_buffer = NULL;
		if (allocate_and_fill(&filenames_buffer, filenames_len, name, fdname) == -1) {
			close(fdname);
			if (filenames_buffer != NULL) my_free(filenames_buffer, filenames_len);
			return -1;
		}
		close(fdname);
	}

	sprintf(name, "%s/%s", glimpseindex_dir, NAME_LIST_INDEX);
	if ((fdname_index = open(name, O_RDONLY, 0)) == -1) {
		fprintf(stderr, "Cannot open for reading: %s\n", name);
		if (!ReadIntoMemory) {
			close(fdname);
		}
		else {
			if (filenames_buffer != NULL) my_free(filenames_buffer, filenames_len);
		}
		return -1;
	}
	fstbuf.st_size = 0;
	fstat(fdname_index, &fstbuf);
	if (ReadIntoMemory) {
		filenames_index_len = fstbuf.st_size;
		filenames_index_buffer = NULL;
		if (allocate_and_fill(&filenames_index_buffer, filenames_index_len, name, fdname_index) == -1) {
			close(fdname_index);
			if (filenames_buffer != NULL) my_free(filenames_buffer, filenames_len);
			if (filenames_index_buffer != NULL) my_free(filenames_index_buffer, filenames_index_len);
			return -1;
		}
		close(fdname_index);
	}

	sprintf(name, "%s/%s", glimpseindex_dir, NAME_HASH);
	if ((fdhash = open(name, O_RDONLY, 0)) == -1) {
		fprintf(stderr, "Cannot open for reading: %s\n", name);
		fprintf(stderr, "To change formats, the index must be built using `glimpseindex -h ...'\n");
		if (!ReadIntoMemory) {
			close(fdname);
			close(fdname_index);
		}
		else {
			if (filenames_buffer != NULL) my_free(filenames_buffer, filenames_len);
			if (filenames_index_buffer != NULL) my_free(filenames_index_buffer, filenames_index_len);
		}
		return -1;
	}
	fstbuf.st_size = 0;
	fstat(fdhash, &fstbuf);
	if (ReadIntoMemory) {
		filehash_len = fstbuf.st_size;
		filehash_buffer = NULL;
		if (allocate_and_fill(&filehash_buffer, filehash_len, name, fdhash) == -1) {
			close(fdhash);
			if (filenames_buffer != NULL) my_free(filenames_buffer, filenames_len);
			if (filenames_index_buffer != NULL) my_free(filenames_index_buffer, filenames_index_len);
			if (filehash_buffer != NULL) my_free(filehash_buffer, filehash_len);
			return -1;
		}
		close(fdhash);
	}

	sprintf(name, "%s/%s", glimpseindex_dir, NAME_HASH_INDEX);
	if ((fdhash_index = open(name, O_RDONLY, 0)) == -1) {
		fprintf(stderr, "Cannot open for reading: %s\n", name);
		fprintf(stderr, "To change formats, the index must be built using `glimpseindex -h ...'\n");
		if (!ReadIntoMemory) {
			close(fdname);
			close(fdname_index);
			close(fdhash);
		}
		else {
			if (filenames_buffer != NULL) my_free(filenames_buffer, filenames_len);
			if (filenames_index_buffer != NULL) my_free(filenames_index_buffer, filenames_index_len);
			if (filehash_buffer != NULL) my_free(filehash_buffer, filehash_len);
		}
		return -1;
	}
	fstbuf.st_size = 0;
	fstat(fdhash_index, &fstbuf);
	if (fstbuf.st_size == MAX_64K_HASH * 4) BigFilenameHashTable = 1;
	else if (fstbuf.st_size == MAX_4K_HASH * 4) BigFilenameHashTable = 0;
	else {
		fprintf(stderr, "Corrupted file: %s\n", name);
		if (!ReadIntoMemory) {
			close(fdname);
			close(fdname_index);
			close(fdhash);
			close(fdhash_index);
		}
		else {
			if (filenames_buffer != NULL) my_free(filenames_buffer, filenames_len);
			if (filenames_index_buffer != NULL) my_free(filenames_index_buffer, filenames_index_len);
			if (filehash_buffer != NULL) my_free(filehash_buffer, filehash_len);
		}
		return -1;
	}
	if (ReadIntoMemory) {
		if (BigFilenameHashTable) filehash_index_len = MAX_64K_HASH * 4;
		else filehash_index_len = MAX_4K_HASH * 4;
		/* filehash_index_len = fstbuf.st_size; */
		filehash_index_buffer = NULL;
		if (allocate_and_fill(&filehash_index_buffer, filehash_index_len, name, fdhash_index) == -1) {
			close(fdhash_index);
			if (filenames_buffer != NULL) my_free(filenames_buffer, filenames_len);
			if (filenames_index_buffer != NULL) my_free(filenames_index_buffer, filenames_index_len);
			if (filehash_buffer != NULL) my_free(filehash_buffer, filehash_len);
			if (filehash_index_buffer != NULL) my_free(filehash_index_buffer, filehash_index_len);
			return -1;
		}
		close(fdhash_index);
	}

/* fprintf(stderr, "file_num=%d, indextype=%d, structuredindex=%d, index_set_size=%d\n", file_num, indextype, structuredindex, index_set_size); */

	/* Initialize statistics information */
	hash_misses = 0;

	/* Do actual conversion */
	if (!InputFilenames) ret = do_conversion(stdin, stdout, indextype, 
						InputType, OutputType, InputEndian, OutputEndian, index_set, index_set_size, ReadIntoMemory);
	else {
		sprintf(outname, "./.wgconvert.%d", getpid());	/* place where converted neighbourhoods are gonna be (./ => same file system as input :-) */

		/* convert file by file: if there is an error in converting one file, go to the next one!!! */
		num_input_filenames = 0;
		while (fgets(name, MAX_LINE_LEN, stdin) != NULL) {
			num_input_filenames ++;
			name_len = strlen(name);
			name[name_len - 1] = '\0';

			/* Figure out filename and put the -P prefix before it */
			lastslash = -1;
			for (i=0; i<name_len - 1; i++) {
				if (name[i] == '/') {
					lastslash = i;
				}
				else if (name[i] == '\\') {
					i++;
				}
			}
			if (lastslash >= 0) {
				memcpy(realname, name, lastslash+1);
				realname[lastslash+1] = '\0';
			}
			else realname[0] = '\0';
			strcat(realname, filename_prefix);
			strcat(realname, &name[lastslash+1]);

			/* Call do_conversion() and check if it worked OK */
			if ((inputfile = fopen(realname, "r")) == NULL) {
				fprintf(stderr, "Can't open for reading: %s\n", realname);
				continue;
			}
			if ((fstat(fileno(inputfile), &fstbuf) == -1) || (fstbuf.st_size <= 0)) {
				fprintf(stderr, "Zero sized file: %s\n", realname);
				fclose(inputfile);
				continue;
			}
				
			if ((outputfile = fopen(outname, "w")) == NULL) {
				fprintf(stderr, "Can't open for writing: %s\n", realname);
				fclose(inputfile);
				continue;
			}
			do_conversion(inputfile, outputfile, indextype, 
					InputType, OutputType, InputEndian, OutputEndian, index_set, index_set_size, ReadIntoMemory);
			fclose(inputfile);
			fflush(outputfile);
			if ((fstat(fileno(outputfile), &fstbuf) == -1) || (fstbuf.st_size <= 0)) {
				fprintf(stderr, "Zero sized output for: %s\n", realname);
				fclose(outputfile);
				continue;
			}
			fclose(outputfile);

			/* move the converted neighbourhood file into the old neighbourhood file */
#if	1
			sprintf(s, "mv -f %s %s", outname, realname);
			if (system(s) == -1) fprintf(stderr, "Errno=%d -- could not execute: %s\n", errno, s);
#else
			if (rename(outname, realname) == -1) fprintf(stderr, "Errno=%d -- could not rename %s as %s\n", errno, outname, realname);
#endif
		}
		unlink(outname);
		ret = 0;
	}

	/* Cleanup and return */
	if (!ReadIntoMemory) {
		close(fdname);
		close(fdname_index);
		close(fdhash);
		close(fdhash_index);
	}
	else {
		if (filenames_buffer != NULL) my_free(filenames_buffer, filenames_len);
		if (filenames_index_buffer != NULL) my_free(filenames_index_buffer, filenames_index_len);
		if (filehash_buffer != NULL) my_free(filehash_buffer, filehash_len);
		if (filehash_index_buffer != NULL) my_free(filehash_index_buffer, filehash_index_len);
	}
	my_free(index_set, index_set_size*sizeof(int));
#if	1
	if (InputFilenames && (InputType == IS_NAMES)) printf("hash_misses=%d num_input_filenames=%d\n", hash_misses, num_input_filenames);
#endif
	return ret;
}

/***************************************
 * Processes options                   *
 * Returns 0 on success, -1 on failure *
 ***************************************/
int
main(argc, argv)
	int	argc;
	char	*argv[];
{

	/* Initialize */
	InfoAfterFilename = 0;
	InputFilenames = 0;
	ReadIntoMemory = 0;
	filenames_buffer = filenames_index_buffer = filehash_buffer = filehash_index_buffer = NULL;
	filename_prefix[0] = '\0';
	InputType = 0;
	OutputType = 0;
	InputEndian = IS_BIG_ENDIAN;
	OutputEndian = IS_BIG_ENDIAN;
	glimpseindex_dir[0] = '\0';

	/* Read options (to know what they mean, check usage() below */
	while (argc > 1) {
		if (strcmp(argv[1], "-ni") == 0) {
			InputType = IS_NAMES;
			OutputType = IS_INDICES;
			argc --; argv ++;
		}
		else if (strcmp(argv[1], "-in") == 0) {
			InputType = IS_INDICES;
			OutputType = IS_NAMES;
			argc --; argv ++;
		}
		else if (strcmp(argv[1], "-nb") == 0) {
			InputType = IS_NAMES;
			OutputType = IS_BITS;
			argc --; argv ++;
		}
		else if (strcmp(argv[1], "-bn") == 0) {
			InputType = IS_BITS;
			OutputType = IS_NAMES;
			argc --; argv ++;
		}
		else if (strcmp(argv[1], "-ib") == 0) {
			InputType = IS_INDICES;
			OutputType = IS_BITS;
			argc --; argv ++;
		}
		else if (strcmp(argv[1], "-bi") == 0) {
			InputType = IS_BITS;
			OutputType = IS_INDICES;
			argc --; argv ++;
		}
		else if (strcmp(argv[1], "-lo") == 0) {
			OutputEndian = IS_LITTLE_ENDIAN;
			argc --; argv ++;
		}
		else if (strcmp(argv[1], "-li") == 0) {
			InputEndian = IS_LITTLE_ENDIAN;
			argc --; argv ++;
		}
		else if (strcmp(argv[1], "-H") == 0) {
		    if (argc == 2) {
			fprintf(stderr, "-H should be followed by a directory name\n");
			return usage();
		    }
		    strncpy(glimpseindex_dir, argv[2], MAX_LINE_LEN);
		    argc -= 2; argv += 2;
		}
		else if (strcmp(argv[1], "-P") == 0) {
		    if (argc == 2) {
			fprintf(stderr, "-P should be followed by a prefix for filenames\n");
			return usage();
		    }
		    strncpy(filename_prefix, argv[2], MAX_LINE_LEN);
		    argc -= 2; argv += 2;
		}
		else if (strcmp(argv[1], "-F") == 0) {
			InputFilenames = 1;
			argc --; argv ++;
		}
		else if (strcmp(argv[1], "-M") == 0) {
			ReadIntoMemory = 1;
			argc --; argv ++;
		}
		else if (strcmp(argv[1], "-U") == 0) {
			InfoAfterFilename = 1;
			argc --; argv ++;
		}
		else {
			fprintf(stderr, "Invalid option %s\n", argv[1]);
			return usage();
		}
	}

	/* Check for errors */
	if ((InputType == 0) || (OutputType == 0)) {
		fprintf(stderr, "Must specify one of: -ib -bi -ni -in -nb -bn\n");
		return usage();
	}

	return change_format(InputFilenames, ReadIntoMemory, InputType, OutputType, InputEndian, OutputEndian, glimpseindex_dir, filename_prefix);
}

/*****************************************
 * Prints out a help/usage message       *
 * Returns: nothing (exits from program) *
 *****************************************/
int
usage()
{

	fprintf(stderr, "\nusage: wgconvert {-ni,-in,-bn,-nb,-ib,-bi} [-li,-lo] [-F] [-H dir] [-M] [-P prefix] <infile >outfile\n");
	fprintf(stderr, "`wgconvert' is used to change the format of neighbourhood files in `webglimpse'\n");
	fprintf(stderr, "To change formats, the index must be built using `glimpseindex -h ...'\n\n");
	fprintf(stderr, "There are 3 formats available:\n");
	fprintf(stderr, "\t1. Complete path names of files (n)\n");
	fprintf(stderr, "\t2. Indices of the files in .glimpse_filenames (i)\n");
	fprintf(stderr, "\t3. A bit-mask of total#files bits for files in the neighborhood (b)\n");
	fprintf(stderr, "We recommend options 1 or 2 since they are easy to use. To use #3, you must\n");
	fprintf(stderr, "specify the proper `endian', since glimpse's -p option reads bits in 4B units.\n\n");
	fprintf(stderr, "-ni: input is names file, output is indices file\n");
	fprintf(stderr, "-in: input is indices file, output is names file\n");
	fprintf(stderr, "-bn: input is bit-field file, output is names file\n");
	fprintf(stderr, "-nb: input is names file, output is bit-field file\n");
	fprintf(stderr, "-ib: input is indices file, output is bit-field file\n");
	fprintf(stderr, "-bi: input is bit-field file, output is indices file\n");
	fprintf(stderr, "-li: input bit-field file is little-endian (default big endian)\n");
	fprintf(stderr, "-lo: output bit-field file is little-endian (default big endian)\n");
	fprintf(stderr, "-F: expect filenames on stdin, not data of an input file\n\tIn this case, this program will convert each filename one by one\n");
	fprintf(stderr, "-H dir: glimpse's index is in directory `dir'\n");
	fprintf(stderr, "-M: cache some .glimpse* files in memory for speed\n\tUseful with -F when a lot of files are being wgconvert-ed at the same time\n");
	fprintf(stderr, "-P prefix: prefix for filenames when -F option is used\n\tIf file=`/a/b.html', with `-P .nh.', wgconvert will access `/a/.nh.b.html'\n");
	fprintf(stderr, "\nFor questions about wgconvert, please contact: `%s'\n", GLIMPSE_EMAIL);
	exit(2);
	return -1;	/* so that the compiler doesn't cry */
}
