static char rcsid[] = "$Id: print-urlrefs.c,v 1.1 1999/11/03 21:41:04 golda Exp $";
/*
 *  print-urlrefs - Reads SOIF and prints normalized URLs from the 
 *  URL-References attribute.  Used to extract URLs from HTML object sums.
 *
 *  Usage: print-urlrefs
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
 *  
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "util.h"
#include "url.h"
#include "template.h"

static void print_urlrefs(t)
Template *t;
{
	AVPair *avp;
	char *s, url[BUFSIZ];
	URL *up;

	if ((avp = extract_AVPair(t->list, "URL-References")) == NULL)
		return;

	/* For each line in the data, grab the URL */
        for (s = strtok(avp->value, "\n"); s != NULL; s = strtok(NULL, "\n")) {
		url[0] = '\0';

		/* Remove poorly formated lines */
		if (strchr(s, '=') || strchr(s, ' ') || strchr(s, '<'))
			continue;
		if (strncmp(s, "mailto:", 6) == 0)
			continue;
		if (strncmp(s, "http:", 5) == 0 && !strstr(s, "://"))
			continue;

                if (strstr(s, "://") != NULL) {
			/* Is this URL ok as-is?  If so, save it */
                        strcpy(url, s);
                } else if (s[0] == '/') {
			/* This URL is relative to the top of t->url */
                        char *thishost = t->url + strlen("http://"), *z;

                        z = strchr(thishost, '/');
                        if (z != NULL) *z = '\0';
                        sprintf(url, "http:/%s%s", thishost, s);
                        if (z != NULL) *z = '/';
                } else {
			/* This URL is relative to t->url */
			char *z = strdup(t->url), *p;

			if ((p = strrchr(z, '/')) != NULL) *p = '\0';
                        sprintf(url, "%s/%s", z, s);
			xfree(z);
                }
		/* If the URL is set, then parse it and print if ok */
		if (url[0]) {
			if ((up = url_open(url)) != NULL) {
				printf("%s\n", up->url);
				url_close(up);
			}
		}
	}
}


int main(argc, argv)
int argc;
char *argv[];
{
	Template *template;
	Buffer *b;

	init_parse_template_file(stdin);
	while ((template = parse_template()) != NULL) {
		print_urlrefs(template);
		printf("%s\n", template->url);
		free_template(template);	
	}
	finish_parse_template();
	exit(0);
}
