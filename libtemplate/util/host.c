static char rcsid[] = "$Id: host.c,v 1.2 2006/03/25 02:13:55 root Exp $";
/*
 *  host.c - Retrieves full DNS name of the current host
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
#include <unistd.h>
#include <pwd.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "util.h"

#ifndef MAXHOSTNAMELEN
#define    MAXHOSTNAMELEN  256
#endif

/*
 *  getfullhostname() - Returns the fully qualified name of the current 
 *  host, or NULL on error.  Pointer is only valid until the next call
 *  to the gethost*() functions.
 */
char *getfullhostname()
{
	struct hostent *hp;
	char buf[MAXHOSTNAMELEN + 1];
	extern int gethostname();	/* UNIX system call */

	if (gethostname(buf, MAXHOSTNAMELEN) < 0)
		return (NULL);
	if ((hp = gethostbyname(buf)) == NULL)
		return (NULL);
	return (hp->h_name);
}

/*
 *  getmylogin() - Returns the login for the pid of the current process,
 *  or "nobody" if there is not login associated with the pid of the
 *  current process.  Intended to be a replacement for braindead getlogin(3).
 */
char *getmylogin()
{
	static char *nobody_str = "nobody";
	uid_t myuid = getuid();
	struct passwd *pwp;

	pwp = getpwuid(myuid);
	if (pwp == NULL || pwp->pw_name == NULL)
		return (nobody_str);
	return (pwp->pw_name);
}

/*
 *  getrealhost() - Returns the real fully qualified name of the given
 *  host or IP number, or NULL on error.  
 */
char *getrealhost(s)
char *s;
{
	char *q;
	struct hostent *hp;
	int is_octet = 1, ndots = 0;
	unsigned int addr = 0;

	if (s == NULL || *s == '\0')
		return (NULL);

	for (q = s; *q; q++) {
		if (*q == '.') {
			ndots++;
			continue;
		} else if (!isdigit(*q)) {	/* [^0-9] is a name */
			is_octet = 0;
			break;
		}
	}

	if (ndots != 3)
		is_octet = 0;

	if (is_octet) {
		addr = inet_addr(s);
		hp = gethostbyaddr((char *) &addr, sizeof(unsigned int), AF_INET);
	} else {
		hp = gethostbyname(s);
	}
	return ((char *)(hp != NULL ? strdup(hp->h_name) : NULL));
}
