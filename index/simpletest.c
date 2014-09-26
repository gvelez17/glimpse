/* Copyright (c) 1994 Sun Wu, Udi Manber, Burra Gopal.  All Rights Reserved. */
/* simple tests which don't need to access indexing data structures */
#include <stdio.h>
#include "glimpse.h"
#define b_sample_size   2048    /* the number of bytes sampled to determine
                                   whether a file is binary  */
#define u_sample_size   1024    /* the number of bytes sampled to determine
                                   whether a file is uuencoded */

extern UseFilters;

#if	0
/* ---------------------------------------------------------------------
   check for binary stream
--------------------------------------------------------------------- */
test_binary(buffer, length)
unsigned char *buffer;
int  length;
{
	int  i=0;
	int  b_count=0;

        if(length > b_sample_size) length = b_sample_size;

        for(i=0; i<length; i++) {
		if(buffer[i] > 127) b_count++;
	}
        if(b_count*10 >= length) return(1);
        return(0);
}
#else	/*0*/
/* Lets try this one instead: Chris Dalton */
test_binary(buffer, length)
unsigned char *buffer;
int  length;
{
    int permitted_errors;

    if (length > b_sample_size) { length= b_sample_size; }
    permitted_errors= length/10;

    while (permitted_errors && length--) {
	if (!(isgraph(*buffer) || isspace(*buffer))) --permitted_errors;
	buffer ++;
    }
/* printf("\t\t\tpermerr=%d in %d\n", permitted_errors, length); */
    return (permitted_errors == 0);
}
#endif	/*0*/

/* ---------------------------------------------------------------------
   check for uuencoded stream
--------------------------------------------------------------------- */
test_uuencode(buffer, length)
unsigned char *buffer;
int  length;
{
        int  i=0;
	int  j;

        if(length > u_sample_size) length = u_sample_size;

	if(strncmp((char *)buffer, "begin", 5) == 0) {
		i=5;
		goto CONT;
	}
        i = memlook("\nbegin", buffer, length);
	if(i < 0) return(0);
CONT:
	while(buffer[i] != '\n' && i<length) i++;
	if(i == length) return(0);
	i++;
	if(buffer[i] == 'M') {
		if((j=memlook("\nM", &buffer[i], length-i)) < 80) return(1);
	}
	/* pab23feb98: return(0) formerly in else of buffer[i]=='M' compare.
	 * I.e., fellthrough with invalid return if nested if failed. */
	return(0);
}

int
test_postscript(buffer, length)
unsigned char *buffer;
int  length;
{
	int	i=0;
	char	*first;

	while((i<length) && (buffer[i] != '\n')) i++;
	if (i>=length) return 0;
	buffer[i] = '\0';
	if ((first = (char *)strstr((char *)buffer, "PS-Adobe")) == NULL) {
		buffer[i] = '\n';
		return 0;
	}
	buffer[i] = '\n';
	return 1;
}

char	*suffixlist[NUM_SUFFIXES] = IGNORED_SUFFIXES;

int
test_special_suffix(name)
	char	*name;
{
	int	len = strlen(name);
	int	j, i = len-1;
	char	*suffix;

#if	SFS_COMPAT
	while(i>=0) if (name[i] == '/') break; else i--;
	if (i<0) return 0;	/* no suffix: can be directory... */
	else suffix = &name[i+1];

	for (j=0; j<NUM_SUFFIXES; j++)
		if (!strcmp(suffix, suffixlist[j])) break;
	if (j >= NUM_SUFFIXES) return 0;
	return 1;
#else
	while(i>=0) if (name[i] == '.') break; else i--;
	if (i<0) return 0;	/* no suffix: can be directory... */
	else suffix = &name[i+1];

	for (j=0; j<NUM_SUFFIXES; j++)
		if (!strcmp(suffix, suffixlist[j])) break;
	if (j >= NUM_SUFFIXES) return 0;
	return 1;
#endif
}
