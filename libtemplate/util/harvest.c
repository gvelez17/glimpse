static char rcsid[] = "$Id: harvest.c,v 1.2 2003/11/13 05:17:39 golda Exp $";
/*
 *  harvest.c - Routines specific to the Harvest installation
 *
 *  Darren Hardy, hardy@cs.colorado.edu, December 1994
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
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include "util.h"

#define DEFAULT_HARVEST_HOME "/usr/local/harvest"

/*
 *  harvest_bindir() - Returns a static buffer that contains the 
 *  pathname that contains the binaries for Harvest.
 */
char *harvest_bindir()
{
	static char bindir[MAXPATHLEN + 1];
	char *s;

	if ((s = getenv("HARVEST_HOME")) != NULL)
		sprintf(bindir, "%s/bin", s);
	else
		sprintf(bindir, "%s/bin", DEFAULT_HARVEST_HOME);
	return (bindir);
}

/*
 *  harvest_libdir() - Returns a static buffer that contains the 
 *  pathname that contains the libraries for Harvest.
 */
char *harvest_libdir()
{
	static char libdir[MAXPATHLEN + 1];
	char *s;

	if ((s = getenv("HARVEST_HOME")) != NULL)
		sprintf(libdir, "%s/lib", s);
	else
		sprintf(libdir, "%s/lib", DEFAULT_HARVEST_HOME);
	return (libdir);
}

/*
 *  harvest_topdir() - Returns a static buffer that contains the 
 *  pathname that contains the libraries for Harvest.
 */
char *harvest_topdir()
{
	static char topdir[MAXPATHLEN + 1];
	char *s;

	if ((s = getenv("HARVEST_HOME")) != NULL)
		sprintf(topdir, "%s", s);
	else
		sprintf(topdir, "%s", DEFAULT_HARVEST_HOME);
	return (topdir);
}

/*
 *  add_harvest_to_path() - If xtra is not-NULL, then it will
 *  add harvest_libdir() + xtra to the path as well.  For example,
 *  a Gatherer process would call:
 *              add_harvest_to_path("gatherer:")
 *  to add $harvest_libdir/ and $harvest_libdir/gatherer
 */
void harvest_add_path(xtra)
char *xtra;
{
	char *s = getenv("PATH"), *newpath, *oldpath, *q;
	char *tmpxtra;

	if (s == NULL)
		fatal("This process does not have a PATH environment variable");
	newpath = xmalloc(strlen(s) + BUFSIZ);
	sprintf(newpath, "PATH=%s", s);
	sprintf(newpath + strlen(newpath), ":%s", harvest_bindir());
	if (xtra != NULL) {
		tmpxtra = strdup(xtra);
		q = strtok(tmpxtra, ":");
		while (q != NULL) {
			sprintf(newpath + strlen(newpath), ":%s/%s",
				harvest_libdir(), q);
			q = strtok(NULL, ":");
		}
		xfree(tmpxtra);
	}
#ifdef DEBUG
	glimpselog("Adding new PATH to environment: %s\n", newpath);
#endif
	(void) putenv(newpath);
}
