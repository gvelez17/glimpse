static char rcsid[] = "$Id: print-attr.c,v 1.1 1999/11/03 21:41:04 golda Exp $";
/*
 *  print-attr - Reads in a template from stdin and prints the data
 *  associated with the given attributed to stdout.
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
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "util.h"
#include "template.h"

static void usage() 
{
	fprintf(stderr, "Usage: print-attr Attribute\n");
	exit(1);
}

int main(argc, argv)
int argc;
char *argv[];
{
	Template *template;
	AVPair *avp;
	char *attr;

	if (argc != 2) 
		usage();

	attr = strdup(argv[1]);

	init_parse_template_file(stdin);
	while ((template = parse_template()) != NULL) {
		avp = extract_AVPair(template->list, attr);
		if (avp) {
			fwrite(avp->value, 1, avp->vsize, stdout);
			putchar('\n');
		}
		free_template(template);	
	}
	finish_parse_template();
	exit(0);
}
