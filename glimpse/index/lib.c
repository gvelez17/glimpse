/* Copyright (c) 1994 Sun Wu, Udi Manber, Burra Gopal.  All Rights Reserved. */
/* ./glimpse/index/lib.c */
#include <stdio.h>

unsigned char *strdup(str)
unsigned char *str;
{
	int len;
	unsigned char *str1, *str1_bak;
	extern char *my_malloc();

	len = strlen(str);
	str1 = (unsigned char *) my_malloc(len + 2);
	if(str1 == NULL) {
		fprintf(stderr, "malloc failure\n");
		exit(2);
	}
	str1_bak = str1;
	while(*str1++ = *str++);
	return(str1_bak);
}

