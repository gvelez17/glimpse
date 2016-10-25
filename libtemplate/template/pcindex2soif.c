static char rcsid[] = "$Id: pcindex2soif.c,v 1.1 1999/11/03 21:41:04 golda Exp $";
/*
 *  pcindex2soif.c - Converts Nebula's PC Index data file into SOIF
 *
 *  Usage: pcindex2soif 
 *
 *  Input data file format:
 *    First line:
 *       ("root1" "root2" ... "rootn")
 *    Other object lines:
 *    (("." "p1" "p2" .. "name") "date" size "desc")
 *    Directory lines:
 *    (("." "p1" "p2" .. "name") "desc" "root")
 *
 *  Has some ugly parsing; anyone know of a good lisp-style data reader in C?
 *  Doesn't work with the Directory lines.
 *
 *  Added the 'Archive-Site' tag.
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
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "url.h"
#include "template.h"

/*
 *  grab_wrapped_value - returns the text of the largest value wrapped by 
 *  char c[0] and char c[1]
 */
char *grab_wrapped_value(buf, c)
char **buf, *c;
{
	static char s[BUFSIZ];
	char *q;

	if ((q = strchr(*buf, c[0])) == NULL) 
		return(NULL);
	*buf = q + 1;				/* mark beginning */

	strcpy(s, *buf);			/* copy */
	if ((q = strrchr(s, c[1])) != NULL) {
		*q = '\0';			/* terminate match */
		q = strrchr(*buf, c[1]);
		*buf = q + 1;			/* advance buf ptr */
	} else {
		return(NULL);	
	}
	return(s);
}


/*
 *  grab_next_value - returns the text in the next smallest value wrapped 
 *  by char c[0] and char c[1]
 */
char *grab_next_value(buf, c)
char **buf, *c;
{
	static char s[BUFSIZ];
	char *q;

	if ((q = strchr(*buf, c[0])) == NULL) 
		return(NULL);
	*buf = q + 1;				/* mark beginning */

	strcpy(s, *buf);			/* copy */
	if ((q = strchr(s, c[1])) != NULL) {
		*q = '\0';			/* terminate match */
		q = strchr(*buf, c[1]);
		*buf = q + 1;			/* advance buf ptr */
	} else {
		return(NULL);	
	}
	return(s);
}

int main(argc, argv)
int argc;
char *argv[];
{
	char buf[BUFSIZ], url[BUFSIZ];
	char *s, *q, *p, *tmp, *line, *tmp2;
	URL *rootup;
	char archive_site[BUFSIZ];
	Template *t;

	strcpy(archive_site, "Unknown");
	init_log(stderr, stderr);
#ifdef HAVE_SETLINEBUF
	setlinebuf(stderr);
	setlinebuf(stdout);
#endif

	/* Grab the Root URL from the first line of the data file */
	if (fgets(buf, BUFSIZ, stdin) == NULL) 
		fatal("Cannot read first line.\n");

	tmp = buf;
	tmp2 = grab_wrapped_value(&tmp, "()");
	if ((q = grab_next_value(&tmp2, "\"\"")) == NULL)
		fatal("Cannot read root URL.\n");
	if ((rootup = url_open(q)) == NULL)
		fatal("Cannot parse URL: %s\n", q);
	if (strstr(rootup->url, "cica") != NULL) {
		strcpy(archive_site, "CICA DOS");
	} else if (strstr(rootup->url, "garbo") != NULL) {
		strcpy(archive_site, "Garbo DOS (Finland)");
	} else if (strstr(rootup->url, "hobbes") != NULL) {
		strcpy(archive_site, "Hobbes OS/2");
	} else if (strstr(rootup->url, "ulowell") != NULL) {
		strcpy(archive_site, "U. Lowell DOS Games");
	} else if (strstr(rootup->url, "oakland") != NULL) {
		strcpy(archive_site, "Oakland U. DOS");
	} else if (strstr(rootup->url, "umich") != NULL) {
		strcpy(archive_site, "U. Michigan DOS");
	}

	/* Now read all of the lines */
	while (fgets(buf, BUFSIZ, stdin) != NULL) {
		strcpy(url, rootup->url);

		/* Grab the entry in parens */
		tmp = buf;
		line = grab_wrapped_value(&tmp, "()");
		tmp = strdup(line);

		/* Build the URL */
		s = grab_next_value(&tmp, "()");
		tmp2 = strdup(s);
		while ((p = grab_next_value(&tmp2, "\"\"")) != NULL) {
			if (!strcmp(p, "."))
				continue;
			strcat(url, "/");
			strcat(url, p);
		}

		if ((t = create_template(NULL, url)) == NULL)
			fatal("Cannot create a template for URL: %s\n", url);

		/* Grab the Date */
		if ((p = grab_next_value(&tmp, "\"\"")) == NULL)
			fatal("Could not grab the date: %s\n", buf);

		t->list = create_AVList("Archive-Site", archive_site,
					strlen(archive_site));

		if (p != NULL && strlen(p) >= 6) /* yymmdd */
			add_AVList(t->list, "ASCII-Date", p, strlen(p));

		/* Grab the size */
		if ((p = grab_next_value(&tmp, "  ")) == NULL)
			fatal("Could not grab the size: %s\n", buf);
		if (strcmp(p, "0") != 0 && strlen(p) > 0) 
			add_AVList(t->list, "File-Size", p, strlen(p));

		/* Grab the Description */
		if ((p = grab_next_value(&tmp, "\"\"")) == NULL)
			fatal("Could not grab the description: %s\n", buf);
		if (p != NULL && strlen(p) > 0)
			add_AVList(t->list, "Description", p, strlen(p));

		init_print_template(stdout);
		print_template(t);
		finish_print_template();
		free_template(t);
	}
	exit(0);
}
