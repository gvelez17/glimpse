static char rcsid[] = "$Id: cksoif.c,v 1.1 1999/11/03 21:41:04 golda Exp $";
/*
 *  cksoif - Reads in templates from stdin and prints whether or not
 *  it's a legal SOIF template.
 *
 *  Darren Hardy, hardy@cs.colorado.edu, November 1994
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
#include <time.h>
#include "util.h"
#include "template.h"

int main(argc, argv)
int argc;
char *argv[];
{
	int n = 0, nok = 0;
	Template *t;

	init_parse_template_file(stdin);
	while (1) {
		n++;
		t = parse_template();
		if (t == NULL && is_parse_end_of_input())
			break;
		if (t == NULL) {
			printf("Attempt %d: Invalid SOIF\n", n);
			continue;
		}
		free_template(t);
		printf("Attempt %d: Valid SOIF\n", n);
		nok++;
	}
	finish_parse_template();
	printf("Successfully parsed %d SOIF objects, in %d attempts\n",
		nok, n - 1);
	exit(0);
}
