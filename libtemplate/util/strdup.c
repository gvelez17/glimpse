static char rcsid[] = "$Id: strdup.c,v 1.2 1999/11/19 08:11:49 golda Exp $";
/*
 *  strdup.c - string duplication
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
#include <string.h>
#include <sys/types.h>
#include "util.h"

#ifdef NO_STRDUP
/*
 *   strdup() - same as strdup(3)
 */
char *strdup(s)
const char *s;
{
	static char *p = NULL;
	int sz;

	if (s == NULL)
		return (NULL);

	sz = strlen(s);
	p = xmalloc((size_t) sz + 1);	/* allocate memory for string */
	memcpy(p, s, sz);	/* copy string */
	p[sz] = '\0';		/* terminate string */
	return (p);
}
#endif
