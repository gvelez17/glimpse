static char rcsid[] = "$Id: string.c,v 1.1 1999/11/03 20:42:14 golda Exp $";
/*
 *  string.c - Simple string manipulation 
 *
 *  Darren Hardy, hardy@cs.colorado.edu, June 1994
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
#include <ctype.h>
#include "util.h"

#ifndef isquote
#define isquote(c)	(((c) == '\"') || ((c) == '\''))
#endif

/*
 *  parse_argv() - Parses the command string to build an argv list.  
 *  Supports simple quoting.  argv is large enough to support the 
 *  command string.
 */
void parse_argv(argv, cmd)
char *argv[];
char *cmd;
{
	char *tmp, *p, *q;
	int i = 0;

	p = q = tmp = strdup(cmd);
	while (1) {
		if (isquote(*q)) {
			p++;
			q++;
			while (*q && !isquote(*q))
				q++;
			if (isquote(*q))
				*q++ = '\0';
			else if (*q == '\0')
				break;
		} else {
			while (*q && !isspace(*q))
				q++;
		}
		while (isspace(*q) && !isquote(*q))
			*q++ = '\0';
		if (*q == '\0') {
			argv[i++] = strdup(p);
			break;
		}
		if (*p)
			argv[i++] = strdup(p);
		p = q;
	}
	argv[i] = NULL;
	xfree(tmp);
}
