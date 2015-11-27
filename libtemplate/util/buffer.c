static char rcsid[] = "$Id: buffer.c,v 1.2 2003/11/13 05:17:39 golda Exp $";
/*
 *  buffer.c - Simple dynamic buffer management.
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
 */
#include <stdio.h>
#include <string.h>
#include "util.h"

/*
 *  create_buffer() - Creates a buffer of default_size bytes allocated.
 */
Buffer *create_buffer(default_size)
int default_size;
{
	static Buffer *b = NULL;

	b = xmalloc(sizeof(Buffer));
	b->size = b->default_size = default_size;
	b->data = xmalloc(b->size);
	b->length = 0;
#ifdef DEBUG
	glimpselog("Creating buffer of %d bytes\n", b->size);
#endif
	return (b);
}

/*
 *  increase_buffer() - Increase the buffer so that it holds sz more bytes.
 */
void increase_buffer(b, sz)
Buffer *b;
int sz;
{
	b->size += sz;
	b->data = xrealloc(b->data, b->size);
#ifdef DEBUG
	glimpselog("Growing buffer by %d bytes to %d bytes\n", sz, b->size);
#endif
}

/*
 *  grow_buffer() - increases the buffer size by the default size
 */
void grow_buffer(b)
Buffer *b;
{
	increase_buffer(b, b->default_size);
}

/*
 *  shrink_buffer() - restores a buffer back to its original size.
 *  all data is lost.
 */
void shrink_buffer(b)
Buffer *b;
{
	b->length = 0;
	if (b->size == b->default_size)		/* nothing to do */
		return;

	if (b->data)
		xfree(b->data);
	b->size = b->default_size;
	b->data = xmalloc(b->size);
#ifdef DEBUG
	glimpselog("Shrinking buffer to %d bytes\n", b->size);
#endif
}

/*
 *  free_buffer() - Cleans up after a buffer.
 */
void free_buffer(b)
Buffer *b;
{
	if (b == NULL)
		return;
#ifdef DEBUG
	glimpselog("Freeing buffer of %d bytes\n", b->size);
#endif
	if (b->data)
		xfree(b->data);
	xfree(b);
}


/*
 *  add_buffer() - Adds the sz bytes of s to the Buffer b.
 */
void add_buffer(b, s, sz)
Buffer *b;
char *s;
int sz;
{
	if (sz < 1)
		return;
	if (b->length + sz + 1 > b->size)
		increase_buffer(b, sz);
	if (sz > 1)
		memcpy(&b->data[b->length], s, sz);
	else
		b->data[b->length] = *s;
	b->length += sz;
	b->data[b->length] = '\0';	/* add NULL to current position */
}
