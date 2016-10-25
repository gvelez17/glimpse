static char rcsid[] = "$Id: template.c,v 1.3 2006/03/25 02:13:55 root Exp $";
/*
 *  template.c - SOIF Object ("template") processing code for Harvest 
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
#include <stdlib.h>
#include <ctype.h>
#include "util.h"
#include "template.h"

/* Local functions */
static void output_char();
static void output_buffer();
static int attribute_cmp();

/*
 *  create_AVList() - Creates an Attribute-Value node to include in an AVList
 */
AVList *create_AVList(attr, value, vsize)
char *attr;
char *value;
int vsize;
{
	static AVList *l;

	l = xmalloc(sizeof(AVList));
	l->data = xmalloc(sizeof(AVPair));
	l->data->value = xmalloc(vsize + 1);

	l->data->attribute = (char *)strdup(attr);
	l->data->vsize = vsize;
	memcpy(l->data->value, value, l->data->vsize);
	l->data->value[l->data->vsize] = '\0';
	l->data->offset = -1;
	l->next = NULL;

	return (l);
}

/*
 *  free_AVList() - Cleans up an AVList
 */
void free_AVList(list)
AVList *list;
{
	AVList *walker = list, *t;

	while (walker) {
		if (walker->data)
			free_AVPair(walker->data);
		t = walker;
		walker = walker->next;
		xfree(t);
	}
}

/*
 *  free_AVPair() - Cleans up an AVPair
 */
void free_AVPair(avp)
AVPair *avp;
{
	if (!avp)
		return;
	if (avp->attribute)
		xfree(avp->attribute);
	if (avp->value)
		xfree(avp->value);
	xfree(avp);
}

/*
 *  add_offset() - Adds the offset value to the AVPair matching attribute.
 */
void add_offset(l, attr, off)
AVList *l;
char *attr;
size_t off;
{
	AVPair *avp = extract_AVPair(l, attr);

	if (avp != NULL)
		avp->offset = off;
}

/*
 *  extract_AVPair() - Searches for the given attribute in the AVList.
 *  Does a case insensitive match on the attributes.  Returns NULL
 *  on error; otherwise returns the matching AVPair.
 */
AVPair *extract_AVPair(list, attribute)
AVList *list;
char *attribute;
{
	AVList *walker;

	for (walker = list; walker; walker = walker->next) {
		if (!strcasecmp(walker->data->attribute, attribute))
			return (walker->data);
	}
	return (NULL);
}

/*
 *  exists_AVList() - Checks to see if an AVPair exists for the given
 *  attribute.  Returns non-zero if it does; 0 if it doesn't.
 */
int exists_AVList(list, attr)
AVList *list;
char *attr;
{
	return (extract_AVPair(list, attr) != NULL ? 1 : 0);
}


/*
 *  add_AVList() - Adds the Attribute-Value pair to the given AVList
 */
void add_AVList(list, attr, value, vsize)
AVList *list;
char *attr;
char *value;
int vsize;
{
	AVList *walker = list;

	if (list == NULL)
		return;

	/* move to the end of the list, and add a node */
	while (walker->next) {
		/* Don't add a duplicate Attribute, just replace it */
		if (!strcasecmp(attr, walker->data->attribute)) {
			xfree(walker->data->value);
			walker->data->vsize = vsize;
			walker->data->value = xmalloc(vsize + 1);
			memcpy(walker->data->value, value, vsize);
			walker->data->value[vsize] = '\0';
			return;
		}
		walker = walker->next;
	}
	walker->next = create_AVList(attr, value, vsize);
}

/*
 *  FAST_add_AVList() - Quick version of add_AVList().  Doesn't check
 *  for duplicates.  attr MUST be unique to the list.
 */
void FAST_add_AVList(list, attr, value, vsize)
AVList *list;
char *attr;
char *value;
int vsize;
{
	AVList *t;

	if (list == NULL)
		return;

	t = create_AVList(attr, value, vsize);
	t->next = list->next;
	list->next = t;
}


/*
 *  merge_AVList() - Merges the b list into the a list.  If the AVPair
 *  in b exists in the a list, then the data is replaced.  Otherwise,
 *  the data is appended to the list.
 */
AVList *merge_AVList(a, b)
AVList *a, *b;
{
	AVList *walker = b;
	AVPair *avp;

	if (a == NULL)
		return (NULL);

	while (walker) {
		avp = extract_AVPair(a, walker->data->attribute);
		if (avp != NULL) {
			/* replace the data */
			xfree(avp->value);
			avp->value = xmalloc(walker->data->vsize);
			memcpy(avp->value, walker->data->value,
			       walker->data->vsize);
			avp->vsize = walker->data->vsize;
			avp->offset = walker->data->offset;
		} else {
			/* append it to 'a' */
			add_AVList(a, walker->data->attribute,
			       walker->data->value, walker->data->vsize);
			add_offset(a, walker->data->attribute,
				   walker->data->offset);
		}
		walker = walker->next;
	}
	return (a);
}

/*
 *  append_AVList() - Adds the Attribute-Value pair to the given AVList.
 *  If the attr is present in the list, then it appends the value to
 *  the previous value.
 */
void append_AVList(list, attr, value, vsize)
AVList *list;
char *attr;
char *value;
int vsize;
{
	AVPair *avp;
	char *buf;

	if ((avp = extract_AVPair(list, attr)) == NULL) {
		add_AVList(list, attr, value, vsize);
	} else {		/* replace the data */
		buf = xmalloc(avp->vsize + vsize + 2);
		memcpy(buf, avp->value, avp->vsize);
		buf[avp->vsize] = '\n';
		memcpy(buf + avp->vsize + 1, value, vsize);
		xfree(avp->value);
		avp->value = buf;
		avp->vsize += vsize + 1;
		avp->offset = -1;
	}
}

/*
 *  create_template() - Creats a new Template structure.
 */
Template *create_template(type, url)
char *type;
char *url;
{
	static Template *t = NULL;

	t = xmalloc(sizeof(Template));
	if (type == NULL)
		t->template_type = (char *)strdup("FILE");
	else
		t->template_type = (char *)strdup(type);
	t->url = (char *)strdup(url);
	t->list = NULL;
	t->offset = -1;
	t->length = -1;
	return (t);
}

/*
 *  free_template() - Cleans up the template.
 */
void free_template(t)
Template *t;
{
	if (!t)
		return;
	if (t->list)
		free_AVList(t->list);
	if (t->template_type)
		xfree(t->template_type);
	if (t->url)
		xfree(t->url);
	xfree(t);
}

/*
 *  Template Parsing and Printing code
 *
 *  Template Parsing can read from memory or from a file.
 *  Template Printing can write to memory or to a file.
 */

static FILE *outputfile = NULL;	/* user's file */
Buffer *bout = NULL;

/*
 *  init_print_template() - Print template to memory buffer or to
 *  a file if fp is not NULL.  Returns NULL if printing to a file;
 *  otherwise returns a pointer to the Buffer where the data is stored.
 */
Buffer *init_print_template(fp)
FILE *fp;
{
	if (fp) {
		outputfile = fp;
		return (NULL);
	} else {
		bout = create_buffer(BUFSIZ);
		return (bout);
	}
}

/*
 *  output_char() - writes a single character to memory or a file.
 */
static void output_char(c)
char c;
{
	output_buffer(&c, 1);
}

/*
 *  output_buffer() - writes a buffer to memory or a file.
 */
static void output_buffer(s, sz)
char *s;
int sz;
{
	if (outputfile)
		fwrite(s, sizeof(char), sz, outputfile);
	else
		add_buffer(bout, s, sz);
}

/*
 *  print_template() - Prints a SOIF Template structure into a file
 *  or into memory.   MUST call init_print_template_file() or 
 *  init_print_template_string() before, and finish_print_template() after.  
 */
void print_template(template)
Template *template;
{
	/* Estimate the buffer size to prevent too many realloc() calls */
	if (outputfile == NULL) {
		AVList *walker;
		int n = 0;

		for (walker = template->list; walker; walker = walker->next)
			n += walker->data->vsize;
		if (bout->length + n > bout->size)
			increase_buffer(bout, n);	/* need more */
	}
	print_template_header(template);
	print_template_body(template);
	print_template_trailer(template);
}

void print_template_header(template)
Template *template;
{
	char buf[BUFSIZ];

	sprintf(buf, "@%s { %s\n", template->template_type, template->url);
	output_buffer(buf, strlen(buf));
}

void print_template_body(template)
Template *template;
{
	char buf[BUFSIZ];
	AVList *walker;

	for (walker = template->list; walker; walker = walker->next) {
		if (walker->data->vsize == 0)
			continue;
		/* Write out an Attribute value pair */
		sprintf(buf, "%s{%u}:\t", walker->data->attribute,
			(unsigned int) walker->data->vsize);
		output_buffer(buf, strlen(buf));
		output_buffer(walker->data->value, walker->data->vsize);
		output_char('\n');
	}
}

void print_template_trailer(template)
Template *template;
{
	output_char('}');
	output_char('\n');
	if (outputfile != NULL)
		fflush(outputfile);
}

/*
 *  finish_print_template() - Cleanup after printing template.
 *  Buffer is no longer valid.
 */
void finish_print_template()
{
	outputfile = NULL;
	if (bout)
		free_buffer(bout);
	bout = NULL;
}


/* Parsing templates */

static char *inputbuf = NULL;
static FILE *inputfile = NULL;
static int inputbufsz = 0, curp = 0;
static size_t inputoffset = 0, inputlength = 0;

void init_parse_template_file(fp)
FILE *fp;
{
	inputfile = fp;
	inputoffset = ftell(fp);
	inputlength = 0;
}

void init_parse_template_string(s, sz)
char *s;
int sz;
{
	inputbuf = s;
	inputbufsz = sz;
	curp = 0;
	inputfile = NULL;
	inputoffset = 0;
	inputlength = 0;
}

void finish_parse_template()
{
	inputfile = NULL;
	curp = 0;
	inputbufsz = 0;
}

int is_parse_end_of_input()
{
	if (inputfile != NULL)
		return (feof(inputfile));
	return (curp >= inputbufsz || inputbuf[curp] == '\0');
}

/* input_char() -> performs c = input_char(); */
#define input_char() \
	if (inputfile != NULL) { \
		inputoffset++; \
		inputlength++; \
		c = fgetc(inputfile);  \
	} else if (curp >= inputbufsz || inputbuf[curp] == '\0') { \
		c = (char) EOF; \
	} else { \
		inputoffset++; \
		inputlength++; \
		c = inputbuf[curp++];  \
	}

static void backup_char(x)
char x;
{
	inputoffset--;
	inputlength--;
	if (inputfile != NULL)
		ungetc(x, inputfile);
	else
		curp--;
	return;
}


#define skip_whitespace()	\
	while (1) { \
		input_char(); \
		if (c == EOF) return(NULL); \
		if (!isspace(c)) { backup_char(c); break; } \
	}

#define skip_tab()	\
	while (1) { \
		input_char(); \
		if (c == EOF) return(NULL); \
		if (c != '\t') { backup_char(c); break; } \
	}

#define skip_whitespace_and(a)	\
	while (1) { \
		input_char(); \
		if (c == EOF) return(NULL); \
		if (c == a) continue; \
		if (!isspace(c)) { backup_char(c); break; } \
	}

#define skip_whitespace_break()	\
	while (1) { \
		input_char(); \
		if (c == EOF) { done = 1; break; }\
		if (c == '}') { done = 1; break; }\
		if (!isspace(c)) { backup_char(c); break; } \
	}

#define grab_token() \
	p = &buf[0]; \
	while (1) { \
		input_char(); \
		if (c == (char) EOF) return(NULL); \
		if (isspace(c)) { backup_char(c); break; } \
		*p++ = c; \
		if (p == &buf[BUFSIZ-1]) return(NULL); \
	} \
	*p = '\0';

#define grab_attribute() \
	p = buf; \
	while (1) { \
		input_char(); \
		if (c == EOF) return(NULL); \
		if (c == '{') break; \
		if (c == '}') break; \
		*p++ = c; \
		if (p == &buf[BUFSIZ-1]) return(NULL); \
	} \
	*p = '\0';


/*
 *  parse_template() - Returns a Template structure for the template
 *  stored in memory or in a file.  MUST call init_parse_template_file()
 *  or init_parse_template_string() before, and finish_parse_template()
 *  after.  Returns NULL on error.
 */
Template *parse_template()
{
	static Template *template = NULL;
	char buf[BUFSIZ], *p, *attribute, *value;
	int vsize, i, done = 0, c;
	size_t voffset;

	template = xmalloc(sizeof(Template));
	while (1) {		/* Find starting point: @ */
		input_char();
		if (c == EOF) {
			xfree(template);
			return (NULL);
		}
		if (c == '@')
			break;
	}
	template->offset = inputoffset;
	/* Get Template-Type */
	grab_token();
	template->template_type = (char *)strdup(buf);

	/* Get URL */
	skip_whitespace_and('{');
	grab_token();
	template->url = (char *)strdup(buf);
	template->list = NULL;

#ifdef DEBUG
	glimpselog("Grabbing Template Object: %s %s\n", template->template_type,
	    template->url);
#endif

	while (1) {
		/* Get Attribute name and value */
		skip_whitespace_break();
		if (done == 1)
			break;
		grab_attribute();
		attribute = (char *)strdup(buf);
#ifdef DEBUG
		log("Grabbed Attribute: %s\n", attribute);
#endif
		grab_attribute();
		vsize = atoi(buf);

		/* Get Value */
		input_char();
		if (c != ':') {
			free_template(template);
			xfree(attribute);
			return (NULL);
		}
		input_char();
		if (c != '\t') {
			free_template(template);
			xfree(attribute);
			return (NULL);
		}
		/* This is a very tight loop, so optimize */
		value = xmalloc(vsize + 1);
		voffset = inputoffset;
		if (inputfile == NULL) {	/* normal one-by-one */
			for (i = 0; i < vsize; i++) {
				input_char();
				value[i] = c;
			}
			value[i] = '\0';
		} else {			/* do the fast file copy */
			if (fread(value, 1, vsize, inputfile) != vsize) {	
				free_template(template);
				xfree(attribute);
				xfree(value);
				return(NULL);
			}
			inputoffset += vsize;
			inputlength += vsize;
		}
		if (template->list == NULL) {
			template->list = create_AVList(attribute, value, vsize);
		} else
			FAST_add_AVList(template->list,attribute,value,vsize);
		add_offset(template->list, attribute, voffset);
		xfree(attribute);
		xfree(value);
	}
	template->length = inputlength;
	return (template);
}


/* Sorting Attribute-Value Lists */

/*
 *  attribute_cmp() - strcmp(3) for attributes.  Works with "Embed<n>" 
 *  attributes so that they are first sorted by number, then by attribute.
 *  Does case insenstive compares.
 */
static int attribute_cmp(a, b)
char *a, *b;
{
	if ((tolower(a[0]) == 'e') && (tolower(b[0]) == 'e') &&		/* quickie */
	    !strncasecmp(a, "embed", 5) && !strncasecmp(b, "embed", 5)) {
		char *p, *q;
		int an, bn;

		p = strchr(a, '<');	/* Find embedded number */
		q = strchr(a, '>');
		if (!p || !q)
			return (strcasecmp(a, b));	/* bail */
		*q = '\0';
		an = atoi(p + 1);
		*q = '>';

		p = strchr(b, '<');	/* Find embedded number */
		q = strchr(b, '>');
		if (!p || !q)
			return (strcasecmp(a, b));	/* bail */
		*q = '\0';
		bn = atoi(p + 1);
		*q = '>';
		if (an != bn)	/* If numbers are different */
			return (an < bn ? -1 : 1);
		/* otherwise, do strcmp on attr */
		return (strcasecmp(strchr(a, '>') + 1, strchr(b, '>') + 1));
	}
	return (strcasecmp(a, b));
}

/*
 *  sort_AVList() - Uses an insertion sort to sort the AVList by attribute.
 *  Returns the new head of the list.  
 */
AVList *sort_AVList(avl)
AVList *avl;
{
	AVList *walker, *n, *a, *t;
	static AVList *head;
	int (*acmp) ();

	acmp = attribute_cmp;

	/* Set the first node to be the head of the new list */
	head = avl;
	walker = avl->next;
	head->next = NULL;

	while (walker) {
		/* Pick off this node */
		n = walker;
		walker = walker->next;
		n->next = NULL;

		/* Find insertion point */
		for (a = head; a->next &&
		  acmp(a->next->data->attribute, n->data->attribute) < 0;
		     a = a->next);
		if (a == head) {	/* prepend to list */
			if (acmp(a->data->attribute, n->data->attribute) < 0) {
				/* As the second node */
				t = a->next;
				a->next = n;
				n->next = t;
			} else {
				/* As the first node */
				head = n;
				n->next = a;
			}
		} else {	/* insert into list */
			t = a->next;
			a->next = n;
			n->next = t;
		}
	}
	return (head);
}

/*
 *  embed_template() - Embeds the given Template t into the Template template.
 *  Returns NULL on error; otherwise returns template.
 */
Template *embed_template(t, template)
Template *t, *template;
{
	int nembed = 0;		/* number of embedded documents in t */
	AVList *walker;
	char *p, *q, buf[BUFSIZ];

	/* Find out what the last embedded document in template is */
	for (walker = template->list; walker; walker = walker->next) {
		if (strncasecmp(walker->data->attribute, "embed<", 6))
			continue;
		p = strchr(walker->data->attribute, '<') + 1;
		if ((q = strchr(walker->data->attribute, '>')) != NULL)
			*q = '\0';
		else
			continue;
		nembed = (nembed < atoi(p)) ? atoi(p) : nembed;
		*q = '>';	/* replace */
	}
#ifdef DEBUG
	log("%s has %d embedded documents\n", template->url, nembed);
#endif

	/* Now add all of the fields from t into template */
	nembed++;
	for (walker = t->list; walker; walker = walker->next) {
		sprintf(buf, "Embed<%d>-%s", nembed, walker->data->attribute);
		FAST_add_AVList(template->list, buf, walker->data->value,
				walker->data->vsize);
		if (walker->data->offset != -1)
			add_offset(template->list, buf, walker->data->offset);
	}
	return (template);
}

/*
 *  sink_embedded() - Places all of the embedded attributes at the bottom
 *  of the list.  *Must* be sorted first.
 */
AVList *sink_embedded(list)
AVList *list;
{
	AVList *start, *end, *walker, *last, *t;
	static AVList *head;

	for (walker = last = head = list, start = end = NULL;
	     walker != NULL;
	     last = walker, walker = walker->next) {
		if (!strncasecmp(walker->data->attribute, "embed", 5)) {
			start = start ? start : last;
		} else if (start != NULL) {
			end = end ? end : last;
		}
	}
	if (start == NULL || end == NULL) {
		/* No embedded section, or at bottom of list */
		return (head);
	} else if (start == head) {
		last->next = start;	/* Embed section at top of list */
		head = end->next;
		end->next = NULL;
	} else {		/* Embed section at middle of list */
		t = start->next;
		last->next = t;
		start->next = end->next;
		end->next = NULL;
	}
	return (head);
}
