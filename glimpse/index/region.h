/* Copyright (c) 1994 Sun Wu, Udi Manber, Burra Gopal.  All Rights Reserved. */
/* Constructed from the mail messages received from Bill Camargo in June 1994 */
#ifndef	_REGION_H_
#define _REGION_H_
#include <sys/types.h>
#include <autoconf.h>	/* autoconf defines STRUCTURED_SQURIES */

#if	STRUCTURED_QUERIES
/* These are imports from Bill's stuff */
#include "util.h"
#include "template.h"
#endif	/*STRUCTURED_QUERIES*/

/* These are mine */
typedef struct REGION {
	int	length;
	int	offset;
	int	attributeid;
	struct REGION *next, *prev;
} region_t;

/* Assuming there are no more than 2^(8*sizeof(int)) attributes */
typedef struct ATTR_ELEMENT {
	struct ATTR_ELEMENT *next;
	int	attributeid;
	char	*attribute;	/* pointer to the one in the hash entry */
} attr_element_t;

extern FILE *my_fopen();
extern char *my_malloc();
extern int my_free();
#endif	/*_REGION_H_*/
