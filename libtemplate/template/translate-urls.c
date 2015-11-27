static char rcsid[] = "$Id: translate-urls.c,v 1.1 1999/11/03 21:41:04 golda Exp $";
/*
 *  translate-urls - Reads SOIF and prints the new SOIF's with the
 *  translated URLs from the 
 *
 *  Usage: translate-urls [(-access | -host | -path) old new]
 *
 *  Darren Hardy, hardy@cs.colorado.edu, July 1994
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
#include "url.h"
#include "template.h"

static void translate_url(t)
Template *t;
{
}

int main(argc, argv)
int argc;
char *argv[];
{
	Template *template;
	Buffer *b;

	init_parse_template_file(stdin);	/* Read Template from stdin */
	while (1) {
		template = parse_template();
		if (template == NULL && is_parse_end_of_input())
			break;
		if (template == NULL)
			continue;

		translate_url(template);

		b = init_print_template(NULL);	
		print_template(template);	
		fwrite(b->data, 1, b->length, stdout);	
		finish_print_template();
		free_template(template);	
	}
	finish_parse_template();
	exit(0);
}
