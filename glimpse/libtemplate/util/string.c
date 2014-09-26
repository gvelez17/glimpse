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
