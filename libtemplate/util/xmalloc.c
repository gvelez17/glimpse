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
 *  This copyright notice applies to all code in Harvest other than
 *  subsystems developed elsewhere, which contain other copyright notices
 *  in their source text.
 *  
 *  The Harvest software was developed by the Internet Research Task
 *  Force Research Group on Resource Discovery (IRTF-RD).  The Harvest
 *  software may be used for academic, research, government, and internal
 *  business purposes without charge.  If you wish to sell or distribute
 *  the Harvest software to commercial clients or partners, you must
 *  license the software.  See
 *  http://harvest.cs.colorado.edu/harvest/copyright,licensing.html#licensing.
 *  
 *  The Harvest software is provided ``as is'', without express or
 *  implied warranty, and with no support nor obligation to assist in its
 *  use, correction, modification or enhancement.  We assume no liability
 *  with respect to the infringement of copyrights, trade secrets, or any
 *  patents, and are not responsible for consequential damages.  Proper
 *  use of the Harvest software is entirely the responsibility of the user.
 *  
 *  For those who are using Harvest for non-commercial purposes, you may
 *  make derivative works, subject to the following constraints:
 *  
 *  - You must include the above copyright notice and these accompanying 
 *    paragraphs in all forms of derivative works, and any documentation 
 *    and other materials related to such distribution and use acknowledge 
 *    that the software was developed at the above institutions.
 *  
 *  - You must notify IRTF-RD regarding your distribution of the 
 *    derivative work.
 *  
 *  - You must clearly notify users that your are distributing a modified 
 *    version and not the original Harvest software.
 *  
 *  - Any derivative product is also subject to the restrictions of the 
 *    copyright, including distribution and use limitations.
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
