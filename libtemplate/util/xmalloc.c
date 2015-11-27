static char rcsid[] = "$Id: xmalloc.c,v 1.1 1999/11/03 20:42:14 golda Exp $";
/*
 *  xmalloc.c - Memory allocation for Essence system.
 *
 *  Darren Hardy, hardy@cs.colorado.edu, February 1994
 *
 *  ----------------------------------------------------------------------
 *  Copyright (c) 1994, 1995.  All rights reserved.
 *  
 *          Mic Bowman of Transarc Corporation.
 *          Peter Danzig of the University of Southern California.
 *          Darren R. Hardy of the University of Colorado at Boulder.
 *          Udi Manber of the University of Arizona.
 *          Michael F. Schwartz of the University of Colorado at Boulder. 
 *  
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#ifdef MEMORY_LEAKS
#include "leak.h"
#endif

/*
 *  xmalloc() - same as malloc(3) except if malloc() returns NULL, then
 *  xmalloc() prints an error message and calls exit(3).  So xmalloc()
 *  always returns non-NULL values.
 */
void *xmalloc(sz)
size_t sz;
{
	static void *p;

#ifdef MEMORY_LEAKS
	leak_logging = 1;
#endif
	if ((p = malloc(sz)) == NULL) {
		errorlog("malloc: Out of memory! Exiting...\n");
		exit(1);
	}
	memset(p, '\0', sz);	/* NULL out the memory */
	return (p);
}

/*
 *  xfree() - same as free(3).
 */
void xfree(s)
void *s;
{
#ifdef MEMORY_LEAKS
	leak_logging = 1;
#endif
	if (s != NULL) {
		free(s);
	}
}

/*
 *  xrealloc() - same as realloc(3).  Exits on error, so always returns
 *  non-NULL values.
 */
void *xrealloc(s, sz)
void *s;
size_t sz;
{
	static void *p;

#ifdef MEMORY_LEAKS
	leak_logging = 1;
#endif
	if ((p = realloc(s, sz)) == NULL) {
		errorlog("realloc: Out of memory! Exiting...\n");
		exit(1);
	}
	return (p);
}
