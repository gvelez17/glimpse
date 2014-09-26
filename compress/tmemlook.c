/* Copyright (c) 1994 Sun Wu, Udi Manber, Burra Gopal.  All Rights Reserved. */
/* copy of ./glimpse/index/memlook.c */
/* ------------------------------------------------------------------------
   this is a brute-force string matching function.
   it's used for short text, or the matching place is expected to be
   not far from the beginning.
   the function returns the position where the pattern first matches.
------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

int tmemlook(pattern, text, length)
unsigned char *pattern;
unsigned char *text;
int  length;
{
	unsigned char *text_end = text+length;
	unsigned char *text_begin = text;
	register unsigned char pat_0=pattern[0];
	register unsigned char *px, *tx;

	if(pat_0 == '\n') {
		if(strncmp((char *)pattern+1, text, strlen((char *)pattern) -1) == 0) {
			return(0);
		}
	}
	/* this is a special case when the pattern is to begin of a line 
	   while the text match the pattern right at the beginning, in
	   which case, '\n' won't be matched.
	*/
	pattern++;
	*text_end = pat_0 ;
	while(text < text_end) {
		while(*text++ != pat_0);
		if(text < text_end) {
			px = pattern; 
			tx = text;
			while(*px == *tx) {
				px++; tx++;
			};
			if(*px == '\0') {
				/*
				printf("begin matched\n");
				*/
				return(text - text_begin);
			}
		}
	}
	return(-2);
}

