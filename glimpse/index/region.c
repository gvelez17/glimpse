/* Copyright (c) 1994 Sun Wu, Udi Manber, Burra Gopal.  All Rights Reserved. */
/* From mail received from Bill Camargo and Darren Hardy in June 1994 */
#include <stdio.h>
#include "region.h"

/*
 * Exports the following routines. Any filtering/attr-val parsing mechanism
 * can be integrated into glimpse and glimpseindex with this interface.
 */

char * /* attrname = */ attr_id_to_name(/* int attrid */);
int /* attrid = */ attr_name_to_id(/* char *attrname */);
int attr_dump_names(/* char *filename */);
int attr_load_names(/* char *filename */);
int attr_free_table(/* void */);
int region_initialize(/* void */);
int region_destroy(/* void */);
int region_create(/* char *filename */);
int /* attrid = */ region_identify(/* int offset_in_file, int len_of_region */);

#if	BG_DEBUG
extern int memory_usage;
#endif	/* BG_DEBUG*/

#if	STRUCTURED_QUERIES

printsp()
{
	int	x;

	printf("stack at %x\n", &x);
}

/*****************************************************************************/

#define ATTR_HASH_TABLE_SIZE	256	/* must be a power of 16=multiple of 4 bits */
#define ATTR_HASH_TABLE_MASK	0xff	/* bits that mask off the bits in TABLE_SIZE */
#define ATTR_HASH_STEP_SIZE	2	/* #of nibbles that make up TABLE_SIZE */

attr_element_t	*attr_hash_table[ATTR_HASH_TABLE_SIZE];
char	**attr_name_table = NULL;
int	attr_num = 0;
int	attr_maxid = 0;

/* English language characters have all info in lowest 4 bits */
int
attr_hash_index(word, len)
	char	*word;
	int	len;
{
	int	i=0, j, index = 0, temp;

	for (i=0; i+ATTR_HASH_STEP_SIZE<=len; i+=ATTR_HASH_STEP_SIZE) {
		temp = 0;
		for (j=0; j<ATTR_HASH_STEP_SIZE; j++)
			temp = (temp << 4) | word[i+j] & 0x0f;
		index = (index + temp) & ATTR_HASH_TABLE_MASK;
	}
	temp = 0;
	for (j=0; i+j<len; j++)
		temp = (temp << 4) | word[i+j] & 0x0f;
	index = (index + temp) & ATTR_HASH_TABLE_MASK;
	return index;
}

char *
attr_id_to_name(id)
	int	id;
{
#if	0
	printf("id = %d\n", id);
#endif	/*0*/
	if ((attr_name_table == NULL) || (id > attr_maxid)) return NULL;
	else return attr_name_table[id];
}

/*
 * returns the attribute number associated with name, 0 for no attribute --
 * NOTE: name may not be null terminated and you are not allowed to alter it.
 * called during indexing and search.
 */
int
attr_name_to_id(name, len)
	char	*name;
	int	len;
{
	int		index = attr_hash_index(name, len);
	attr_element_t	*e = attr_hash_table[index];
#if	0
	char		c = name[len];
	name[len] = '\0';
	fprintf(stderr, "attr=%s @ %d?\n", name, index);
	fflush(stderr);
	name[len] = c;
#endif	/*0*/

	while(e != NULL) {
		if (!strncmp(e->attribute, name, len)) break;
		else e = e->next;
	}
	if (e!=NULL) {
#if	0
		fprintf(stderr, "foundid=%d\n", e->attributeid);
#endif	/*0*/
		return e->attributeid;
	}
	return 0;
}

/*
 * returns the attribute number (> 0) for the attribute "name". It adds the
 * name as a newly seen attribute if it doesn't exist already (using #tables).
 * called in region_create, which is called during indexing.
 */
attr_insert_name(name, len)
	char	*name;
	int	len;
{
	int		index = attr_hash_index(name, len);
	attr_element_t	**pe = &attr_hash_table[index], *e;

	while(*pe != NULL) {
		if (!strcmp((*pe)->attribute, name)) break;
		else pe = &(*pe)->next;
	}
	if (*pe!=NULL) return (*pe)->attributeid;

	e = (attr_element_t *)my_malloc(sizeof(attr_element_t));
	e->attribute = (char *)my_malloc(len + 2);
	strncpy(e->attribute, name, len + 1);
	e->attributeid = (++attr_num);
	e->next = NULL;
	*pe = e;
#if	0
	fprintf(stderr, "inserting %s %d\n", name, attr_num);
#endif	/*0*/
	return e->attributeid;
}

/*
 * frees current hash table of attr-value pairs.
 * called after dump in indexing, and at end of search (after previous load).
 */
int
attr_free_table()
{
	int	i;
	attr_element_t *e, *temp;

	for (i=0; i<ATTR_HASH_TABLE_SIZE; i++) {
		e = attr_hash_table[i];
		while (e != NULL) {
			temp = e->next;
#if	BG_DEBUG
			memory_usage -= strlen(e->attribute) + 2;
#endif	/*BG_DEBUG*/
			my_free(e->attribute, 0);
			my_free(e, sizeof(attr_element_t));
			e = temp;
		}
		attr_hash_table[i] = NULL;
	}
	if (attr_name_table != NULL) {
		my_free(attr_name_table, sizeof(attr_element_t *) * ATTR_HASH_TABLE_SIZE);
		attr_name_table = NULL;
	}
	return 0;
}

/* Looks for embedded attributes and copies the real attribute into dest */
attr_extract(dest, src)
	char	*dest, *src;
{
	char	*oldsrc = src;

check_again:
	if (!strncmp("embed<", src, 6) || !strncmp("Embed<", src, 6) || !strncmp("EMBED<", src, 6)) {
		src += 6;
		while ((*src != '>') && (*src != '\0')) src++;
		if (*src == '\0') {
			strcpy(dest, oldsrc);
			return;
		}
		while (!isalnum(*(unsigned char *)src)) src ++;	/* assuming type names are .. */
		oldsrc = src;
		goto check_again;
	}
	strcpy(dest, src);
	return;
}

/*
 * dumps the attribute-list into a file name (id, name, \n)
 * into the file specified and then destroys the hash table.
 * Returns #of attributes dumped into the file, -1 if error.
 * called at the end of indexing.
 */
int
attr_dump_names(filename)
	char	*filename;
{
	int	i=0;
	int	ret = -1;
	FILE	*fp = fopen(filename, "w");
	attr_element_t *e;

#if	0
	printf("in dump attr\n");
#endif	/*0*/
	if (fp == NULL) return -1;
	ret = 0;
	for (i=0; i<ATTR_HASH_TABLE_SIZE; i++) {
		e = attr_hash_table[i];
		while (e != NULL) {
			fprintf(fp, "%d,%s ", e->attributeid, e->attribute);
			e = e->next;
			ret ++;
		}
		fputc('\n', fp);
	}
	fflush(fp);
	fclose(fp);
	return ret;
}

/*
 * constructs a hash-table of attributes by reading them from the file.
 * Returns #of attributes read from the file, -1 if error.
 * Does not recompute hash-indices of attributes.
 * called before searching for attr=val pairs.
 */
int
attr_load_names(filename)
	char	*filename;
{
	int	index = 0, ret = 0;
	FILE	*fp = fopen(filename, "r");
	attr_element_t *e;
	int	c = 0;
	char	temp[1024];	/* max attr name */
	char	buffer[1024+32];/* max attr id pair */
	int	i;
	int	id;

	attr_maxid = 0;
	memset(attr_hash_table, '\0', sizeof(attr_element_t *) * ATTR_HASH_TABLE_SIZE);
	if (fp == NULL) return -1;
	while ((c = getc(fp)) != EOF) {
		if (c == '\n') {
			index ++;
			continue;
		}
		ungetc(c, fp);
		/* fscanf screws up fp and skips over trailing space characters (\t,\n, ) */
		i=0;
		while ((c=getc(fp)) != ' ') buffer[i++] = c;
		buffer[i] = '\0';
#if	0
		printf("buffer=%s\n", buffer);
#endif	/*0*/
		sscanf(buffer, "%d,%1023s", &id, temp);
		temp[1023] = '\0';
#if	0
		printf("read attr=%s,%d @ %d\n", temp, id, index);
#endif	/*0*/
		if (id <= 0) continue;
		e = (attr_element_t *)my_malloc(sizeof(attr_element_t));
		e->attributeid = id;
		if (id > attr_maxid) attr_maxid = id;
		e->attribute = (char *)my_malloc(strlen(temp) + 2);
		strcpy(e->attribute, temp);
		e->next = attr_hash_table[index];
		attr_hash_table[index] = e;
		ret ++;
		if (index >= ATTR_HASH_TABLE_SIZE - 1) break;
	}
	fclose(fp);

	attr_name_table = (char **)my_malloc(sizeof(char *) * (ret=(ret >= (attr_maxid + 1) ? ret : (attr_maxid + 1))));
	memset(attr_name_table, '\0', sizeof(char *) * ret);
	for (i=0; i<ATTR_HASH_TABLE_SIZE; i++) {
		e = attr_hash_table[i];
		while (e!=NULL) {
			attr_name_table[e->attributeid] = e->attribute;
			e = e->next;
		}
	}
	return ret;
}

/***************************************************************************/

region_t *current_regions, *nextpos;	/* nextpos is hint into list */

/*
 * Called during indexing before region_create.
 * returns 0.
 */
int
region_initialize()
{
	attr_num = 0;
	attr_name_table = NULL;
	memset(attr_hash_table, '\0', sizeof(attr_element_t *) * ATTR_HASH_TABLE_SIZE);
	current_regions = nextpos = NULL;
	return 0;
}

/*
 * creates a data structure containing the list of attributes
 * which occur at increasing offsets in the given file -- future
 * region_identify() calls use the "current" data structure.
 * returns 0 if success, -1 if it cannot open the file.
 */
int
region_create(name)
	char	*name;
{
	FILE	*fp;
	AVList	*al;
	region_t *prl, *rl, *lastrl;
	Template *t;
	char	temp[1024];

	current_regions = nextpos = NULL;
	if ((fp = my_fopen(name, "r")) == NULL) return -1;
	init_parse_template_file(fp);

	lastrl = NULL;
	while ((t = parse_template()) != NULL) {
		/* do insertion sort of list returned by parse_template using offsets */
		if ((t->url != NULL) && (strlen(t->url) > 0)) {
			rl = (region_t *)my_malloc(sizeof(region_t));
			/* Darren Hardy's Voodo :-) */
                        /* The SOIF looks like this:  @TTYPE { URL\n */
                        /* t->offset points to the @ */
                        /* rl->offset points to the space before URL */
                        /* rl->length includes the entire URL */
                        rl->offset = t->offset + strlen(t->template_type) + 3;
                        rl->length = strlen(t->url) + 1;
			rl->attributeid = attr_insert_name("url", 3);

			if ((lastrl != NULL) && (lastrl->offset <= rl->offset)) {	/* go forward */
				prl = lastrl;
				while (prl->next != NULL) {
					if (prl->next->offset > rl->offset) {
						rl->prev = prl;
						rl->next = prl->next;
						prl->next->prev = rl;
						prl->next = rl;
						lastrl = rl;
						break;
					}
					else prl = prl->next;
				}
				if (prl->next == NULL) {
					rl->next = NULL;
					rl->prev = prl;
					prl->next = rl;
					lastrl = rl;
				}
			}
			else {	/* must go backwards and find the right place to insert */
				prl = lastrl;
				while (prl != NULL) {
					if (prl->offset < rl->offset) {
						rl->prev = prl;
						rl->next = prl->next;
						if (prl->next != NULL)
							prl->next->prev = rl;
						prl->next = rl;
						lastrl = rl;
						break;
					}
					else prl = prl->prev;
				}
				if (prl == NULL) {
					rl->next = current_regions;
					if (current_regions != NULL) current_regions->prev = rl;
					rl->prev = NULL;
					current_regions = rl;
					lastrl = rl;
				}
			}

#if	0
			printf("region url=[%d,%d]\n", rl->offset, rl->offset+rl->length);
#endif	/*0*/
		}

		al = t->list;
		while(al != NULL) {
			rl = (region_t *)my_malloc(sizeof(region_t));
			rl->offset = al->data->offset;
			rl->length = al->data->vsize;
			attr_extract(temp, al->data->attribute);
			rl->attributeid = attr_insert_name(temp, strlen(temp));

			if ((lastrl != NULL) && (lastrl->offset <= rl->offset)) {	/* go forward */
				prl = lastrl;
				while (prl->next != NULL) {
					if (prl->next->offset > rl->offset) {
						rl->prev = prl;
						rl->next = prl->next;
						prl->next->prev = rl;
						prl->next = rl;
						lastrl = rl;
						break;
					}
					else prl = prl->next;
				}
				if (prl->next == NULL) {
					rl->next = NULL;
					rl->prev = prl;
					prl->next = rl;
					lastrl = rl;
				}
			}
			else {	/* must go backwards and find the right place to insert */
				prl = lastrl;
				while (prl != NULL) {
					if (prl->offset < rl->offset) {
						rl->prev = prl;
						rl->next = prl->next;
						if (prl->next != NULL)
							prl->next->prev = rl;
						prl->next = rl;
						lastrl = rl;
						break;
					}
					else prl = prl->prev;
				}
				if (prl == NULL) {
					rl->next = current_regions;
					if (current_regions != NULL) current_regions->prev = rl;
					rl->prev = NULL;
					current_regions = rl;
					lastrl = rl;
				}
			}

#if	0
			printf("region %s=[%d,%d]\n", al->data->attribute, rl->offset, rl->offset+rl->length);
#endif	/*0*/
			al = al->next;
		}
		free_template(t);
	}
	finish_parse_template();
	nextpos = current_regions;
	fclose(fp);
	return 0;
}

/*
 * frees the data structure created for the current file above.
 * returns 0.
 */
int
region_destroy()
{
	region_t *rl = current_regions, *trl;

	while (rl != NULL) {
		trl = rl;
		rl = rl->next;
		free(trl);
	}
	current_regions = nextpos = NULL;
	return 0;
}

/*
 * returns attribute number [1..num_attr] which covers (inclusive)
 * the region * [offset, offset+len] in the "current" file, 0 if none.
 * called during indexing after region_create, and search after
 * attr_load_names. Do not need sophisticated interval trees here!
 */
int
region_identify(offset, len)
	int	offset, len;
{
	region_t *rl;

	if (nextpos == NULL) nextpos = current_regions;
	rl = nextpos;
	while (rl!=NULL) {
		if (rl->offset > offset + len)
			goto backwards;			/* definitely before: can be earlier region OR hole */
		else if ((rl->offset <= offset) && (rl->offset + rl->length >= offset + len))
			return rl->attributeid;		/* definitely within */
		else if (rl->offset + rl->length < offset)
			nextpos = rl = rl->next;	/* definitely after: later region */
		else return 0;				/* overlapping: error */
	}
	return 0;					/* reached end of file */

backwards:
	while (rl!=NULL) {
		if (rl->offset > offset + len)
			nextpos = rl = rl->prev;	/* definitely before: earlier region */
		else if ((rl->offset <= offset) && (rl->length + rl->length >= offset + len))
			return rl->attributeid;		/* definitely within */
		else if (rl->offset + rl->length < offset)
			return 0;			/* hole */
		else return 0;				/* overlapping: error */
	}
	return 0;					/* reached end of file */
}

#else	/*STRUCTURED_QUERIES*/

int attr_num = 0;

char *attr_id_to_name(id)
	int	id;
{
	return NULL;
}

int attr_name_to_id(name)
	char	*name;
{
	return 0;
}

int attr_dump_names(name)
	char	*name;
{
	return 0;
}

int attr_load_names(name)
	char	*name;
{
	return 0;
}

int attr_free_table()
{
	return 0;
}

int region_initialize()
{
	return 0;
}

int region_desrtroy()
{
	return 0;
}

int region_create(name)
	char	*name;
{
	return 0;
}

int region_destroy()
{
	return 0;
}

int region_identify(offset, len)
	int	offset, len;
{
	return 0;
}
#endif	/*STRUCTURED_QUERIES*/
