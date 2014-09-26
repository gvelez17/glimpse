static char rcsid[] = "$Id: print-template.c,v 1.1 1999/11/03 21:41:04 golda Exp $";
/*
 *  print-template - Reads in a template from stdin and prints it to stdout.
 *  Used to test template parsing and printing.
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
#include <string.h>
#include <time.h>
#include "util.h"
#include "template.h"

static void add_print_time(t)
Template *t;
{
	char buf[BUFSIZ];

	sprintf(buf, "%u", (unsigned int) time(NULL));
	add_AVList(t->list, "Print-Time", buf, strlen(buf));
}

int main(argc, argv)
int argc;
char *argv[];
{
	Template *template;
	Buffer *b;

	init_parse_template_file(stdin);	/* Read Template from stdin */
	while ((template = parse_template()) != NULL) {
		add_print_time(template);	/* Add new Attribute-Value */
		b = init_print_template(NULL);	/* Print Template to Buffer */
		print_template(template);	
		fwrite(b->data, 1, b->length, stdout);	/* Buffer to stdout */
		finish_print_template();	/* Clean up */
		free_template(template);	
	}
	finish_parse_template();
	exit(0);
}
