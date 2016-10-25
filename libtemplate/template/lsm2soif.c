static char rcsid[] = "$Id: lsm2soif.c,v 1.1 1999/11/03 21:41:04 golda Exp $";
/*
 *  lsm2soif - Converts Linux Software Maps (lsm) to SOIF.
 *
 *  Usage: lsm2soif url local-file
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
 */
#include <stdio.h>
#include <string.h>
#include "util.h"
#include "url.h"
#include "template.h"

/* Local functions */
static void do_lsmtosoif();

/* Local variables */
static int n_flag = 0;

static void usage()
{
	fprintf(stderr, "Usage: lsm2soif url local-file\n");
	exit(1);
}

static void do_lsmtosoif(url, filename)
char *url;
char *filename;
{
	char buf[BUFSIZ], attr[BUFSIZ], value[BUFSIZ];
	char *sv, *pv, *fv, *s, *p;
	int i;
	Template *t;
	FILE *fp;
	URL *up;
	AVPair *site_avp, *path_avp, *file_avp;

	if ((up = url_open(url)) == NULL) {
		errorlog("Cannot open URL: %s\n", url);
		return;
	}

	/* Build the template */
	t = create_template(NULL, up->url);

	/* Read the file and build a SOIF template from it */
	if ((fp = fopen(filename, "r")) == NULL) {
		log_errno(filename);
		url_close(up);
		return;
	}
	while (fgets(buf, BUFSIZ, fp)) {
		if ((s = strrchr(buf, '\n')) != NULL)
			*s = '\0';
		if ((s = strchr(buf, '=')) == NULL)
			continue;	/* not an LSM line */
		for (p = buf, i = 0; p < s && !isspace(*p); p++, i++)
			attr[i] = *p;
		attr[i] = '\0';
		if (i < 1)
			continue;	/* null attribute */
		if (isdigit(attr[--i]))
			attr[i] = '\0';	/* strip attribute number */

		/* Make Desc lines Description lines */
		if (!strcmp(attr, "Desc")) {
			strcpy(attr, "Description");
		}

		while (*s != '\0' && (*s == '=' || isspace(*s)))
			s++;
		if (!strcmp(attr, "Site") ||
		    !strcmp(attr, "Path") ||
		    !strcmp(attr, "File")) {
			if ((p = strchr(s, ' ')) != NULL) 
				*p = '\0';
			if ((p = strchr(s, '\t')) != NULL) 
				*p = '\0';
		}
		if (strlen(s) < 1)	/* empty line */
			continue;
		strcpy(value, s);
		if (t->list)
			append_AVList(t->list, attr, value, strlen(value));
		else
			t->list = create_AVList(attr, value, strlen(value));
		
	}
	fclose(fp);
	
	/* Reset t->url to the file that the LSM points to, if possible */
	site_avp = extract_AVPair(t->list, "Site");
	path_avp = extract_AVPair(t->list, "Path");
	file_avp = extract_AVPair(t->list, "File");
	if (site_avp && path_avp && file_avp) {
		sv = strdup(site_avp->value);
		pv = strdup(path_avp->value);
		fv = strdup(file_avp->value);
		for (p = sv; *p && !isspace(*p); p++);
		*p = '\0';
		for (p = pv; *p && !isspace(*p); p++);
		*p = '\0';
		for (p = fv; *p && !isspace(*p); p++);
		*p = '\0';
		if (*pv == '/' && *fv == '/')
			sprintf(buf, "ftp://%s%s%s", sv, pv, fv);
		else if (*pv == '/' && *fv != '/')
			sprintf(buf, "ftp://%s%s/%s", sv, pv, fv);
		else if (*pv != '/' && *fv == '/')
			sprintf(buf, "ftp://%s/%s%s", sv, pv, fv);
		else
			sprintf(buf, "ftp://%s/%s/%s", sv, pv, fv);
		xfree(t->url);
		t->url = strdup(buf);
		xfree(sv);
		xfree(pv);
		xfree(fv);
	}

	/* Print out the template */
	(void) init_print_template(stdout);
	print_template(t);
	finish_print_template();
	free_template(t);
	url_close(up);
	return;
}

int main(argc, argv)
int argc;
char *argv[];
{
	char *url, *filename;

	if (argc != 3)
		usage();
	url = strdup(argv[1]);
	filename = strdup(argv[2]);

	init_log(stderr, stderr);
	init_url();
	do_lsmtosoif(url, filename);
	finish_url();
	exit(0);
}
