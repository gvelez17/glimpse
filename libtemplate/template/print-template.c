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
