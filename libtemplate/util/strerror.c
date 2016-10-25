static char rcsid[] = "$Id: strerror.c,v 1.2 1999/11/19 08:11:49 golda Exp $";
/*
 *  strerror.c - print message associated with errno
 *
 *  Darren Hardy, hardy@cs.colorado.edu, April 1994
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
#include "util.h"

#ifdef NO_STRERROR
/*
 *   strerror() - same as strerror(3)
 */
char *strerror(n)
int n;
{
	if (n < 0 || n >= sys_nerr)
		return (NULL);
	return (sys_errlist[n]);
}
#endif
