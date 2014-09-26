/* Copyright (c) 1994 Burra Gopal, Udi Manber.  All Rights Reserved. */
#include "glimpse.h"
#include "defs.h"
#include <errno.h>

#if	BG_DEBUG
extern	FILE	*debug;
#endif	/*BG_DEBUG*/
extern	char	INDEX_DIR[MAX_LINE_LEN];
extern	int	Only_first;
extern	int	PRINTAPPXFILEMATCH;
extern	int	OneFilePerBlock;
extern	int	StructuredIndex;
extern	int	WHOLEFILESCOPE;
extern	unsigned int *dest_index_set;
extern	unsigned char *dest_index_buf;
extern	int	mask_int[32];
extern	int	errno;
extern	int	ByteLevelIndex;
extern  int	RecordLevelIndex;
extern  int	rdelim_len;
extern  char	rdelim[MAX_LINE_LEN];
extern  char	old_rdelim[MAX_LINE_LEN];
extern	int	NOBYTELEVEL;
extern	int	OPTIMIZEBYTELEVEL;
extern	int	RegionLimit;
extern	int	PRINTINDEXLINE;
extern	struct offsets **src_offset_table;
extern	unsigned int	*multi_dest_index_set[MAXNUM_PAT];
extern	struct offsets **multi_dest_offset_table[MAXNUM_PAT];
extern	char	*index_argv[MAX_ARGS];
extern	int	index_argc;
extern	CHAR	GProgname[MAXNAME];
extern	FILE	*indexfp, *minifp;
extern int REAL_PARTITION, REAL_INDEX_BUF, MAX_ALL_INDEX, FILEMASK_SIZE;
extern int p_table[MAX_PARTITION];
extern int GNumpartitions;
extern	int	INVERSE;	/* agrep's global: need here to implement ~ in index-search */
extern int	last_Y_filenumber;

#define USEFREQUENCIES	0	/* set to one if we want to stop collecting offsets sometimes since words "look" like they are in the stop list... */

free_list(p1)
	struct offsets 	**p1;
{
	struct offsets	*tp1;

	while (*p1 != NULL) {
		tp1 = *p1;
		*p1 = (*p1)->next;
		my_free(tp1, sizeof(struct offsets));
	}
}

/* Unions offset lists list2 with list1 sorted in increasing order (deletes elements from list2) => changes both list1 and list2: f += #elems added */
sorted_union(list1, list2, f, pf, cf)
	struct offsets **list1, **list2;
	int	*f, pf, cf;
{
	register struct offsets **p1 = list1, *p2;
	register int	count = *f;	/* don't update *f if setting NOBYTELEVEL */

	if (!RecordLevelIndex && NOBYTELEVEL) {	/* cannot come here! */
		free_list(list1);
		free_list(list2);
		return;
	}
#if	USEFREQUENCIES
	if (!RecordLevelIndex && ( ((pf > MIN_OCCURRENCES)  && (count > MAX_UNION * pf)) || (count > MAX_ABSOLUTE) ||
	     ((count > MIN_OCCURRENCES) && (pf > MAX_UNION * count)) || (pf > MAX_ABSOLUTE) )) {
		/* enough if we check the second condition at the beginning since it won't surely be satisfied after this when count ++ */
		NOBYTELEVEL = 1;
		return;
	}
#endif
	while (*list2 != NULL) {
		/* extract 1st element, update list2 */
		p2 = *list2;
		*list2 = (*list2)->next;
		p2->next = NULL;

		/* find position to insert p2, and do so */
		p1 = list1;
		while (((*p1) != NULL) && ((*p1)->offset < p2->offset)) p1 = &(*p1)->next;
		if (*p1 == NULL) {	/* end of list1: append list2 to it and return */
			*p1 = p2;
			p2->next = *list2;
			*list2 = NULL;
			if (cf > 0)  count = *f + cf;
#if	USEFREQUENCIES
			if (!RecordLevelIndex && ( ((pf > MIN_OCCURRENCES) && (count > MAX_UNION * pf)) || (count > MAX_ABSOLUTE))) {
				NOBYTELEVEL = 1;
				return;
			}
#endif
			*f = count;
			return;
		}
		else if (p2->offset == (*p1)->offset) my_free(p2, sizeof(struct offsets));
		else {
			p2->next = *p1;
			*p1 = p2;
			count ++;
#if	USEFREQUENCIES
			if (!RecordLevelIndex && ( ((pf > MIN_OCCURRENCES)  && (count > MAX_UNION * pf)) || (count > MAX_ABSOLUTE) )) {
				NOBYTELEVEL = 1;
				return;
			}
#endif
			/* update list1 */
			list1 = &(*p1)->next;
		}
	}
	*f = count;
}

/* Intersects offset lists list2 with list1 sorted in increasing order (deletes elements from list2) => changes both list1 and list2 */
sorted_intersection(filenum, list1, list2, f)
	struct offsets **list1, **list2;
	int	*f;
{
	register struct offsets **p1 = list1, *p2, *tp1;
	register int diff;
	struct offsets *tp;

	if (!RecordLevelIndex && NOBYTELEVEL) {	/* cannot come here! */
		free_list(list1);
		free_list(list2);
		return;
	}

	/*
	NOT NECESSARY SINCE done INITIALIZED TO 0 ON CREATION AND MADE 0 BELOW
	tp = *list1;
	while (tp != NULL) {
		tp->done = 0;
		tp = tp->next;
	}
	*/

#if	0
printf("sorted_intersection BEGIN: list1=\n\t");
tp = *list1;
while (tp != NULL) {
	printf("%d ", tp->offset);
	tp = tp->next;
}
printf("\n");
printf("list2=\n\t");
tp = *list2;
while (tp != NULL) {
	printf("%d ", tp->offset);
	tp = tp->next;
}
printf("\n");
#endif

	/* find position to intersect list2, and do so: REMEBER: list1 is in increasing order, and so is list2 !!! */
	p1 = list1;
	while ( ((*p1) != NULL) && (*list2 != NULL) ) {
		diff = (*list2)->offset - (*p1)->offset;
		if ( (diff == 0) || (!RecordLevelIndex && (diff >= -RegionLimit) && (diff <= RegionLimit)) ) {
			(*p1)->done = 1; /* p1 is in */
			p1 = &(*p1)->next;
			/* Can't increment p2 here since it might keep others after p1 also in */
		}
		else {
			if (diff < 0) {
				p2 = *list2;
				*list2 = (*list2)->next;
				my_free(p2, sizeof(struct offsets));
				/* p1 can intersect with list2's next */
			}
			else {
				if((*p1)->done && 0) p1 = &(*p1)->next; /* imposs */	/* THIS CHECK ALWAYS YEILDS 0 FROM 25/08/1996: bgopal@cs.arizona.edu */
				else {
					tp1 = *p1;
					*p1 = (*p1)->next;
					my_free(tp1, sizeof(struct offsets));
					(*f) --;
				}
				/* list2 can intersect with p1's next */
			}
		}
	}

	while (*list2 != NULL) {
		p2 = *list2;
		*list2 = (*list2)->next;
		my_free(p2, sizeof(struct offsets));
	}

	p1 = list1;
	while (*p1 != NULL) {
		if ((*p1)->done == 0) {
			tp1 = *p1;
			*p1 = (*p1)->next;
			my_free(tp1, sizeof(struct offsets));
			(*f) --;
		}
		else {
			(*p1)->done = 0;	/* for the next round! */
			p1 = &(*p1)->next;
		}
	}

#if	0
printf("sorted_intersection END: list1=\n\t");
tp = *list1;
while (tp != NULL) {
	printf("%d ", tp->offset);
	tp = tp->next;
}
printf("\n");
printf("list2=\n\t");
tp = *list2;
while (tp != NULL) {
	printf("%d ", tp->offset);
	tp = tp->next;
}
printf("\n");
#endif
}

purge_offsets(p1)
	struct offsets **p1;
{
	struct offsets *tp1;

	while (*p1 != NULL) {
		if ((*p1)->sign == 0) {
			tp1 = *p1;
			(*p1) = (*p1)->next;
			my_free(tp1, sizeof(struct offsets));
		}
		else p1 = &(*p1)->next;
	}
}

/* Returns 1 if it is a Universal set, 0 otherwise. Constraint: WORD_END_MARK/ALL_INDEX_MARK must occur at or after buffer[0] */
get_set(buffer, set, offset_table, patlen, pattern, patattr, outfile, partfp, frequency, prevfreq)
	unsigned char	*buffer;
	unsigned int	*set;
	struct offsets **offset_table;
	int	patlen;
	char	*pattern;
	int	patattr;
	FILE	*outfile;
	FILE	*partfp;
	int	*frequency, prevfreq;
{
	int	bdx2, j;
	int	ret;
	int	x=0, y=0, diff, even_words=1, prevy;
	int	indexattr = 0;
	struct offsets *o, *tailo, *heado;
	int	delim = encode8b(0);
	int	curfreq = 0;
	unsigned char c;

	/* buffer[0] is '\n', search must start from buffer[1] */
	bdx2 = 1;
	if (OneFilePerBlock)
		while((bdx2<REAL_INDEX_BUF+1) && (buffer[bdx2] != WORD_END_MARK) && (buffer[bdx2] != ALL_INDEX_MARK)) bdx2++;
	else while((bdx2<REAL_INDEX_BUF+1) && (buffer[bdx2] != WORD_END_MARK)) bdx2++;
	if (bdx2 >= REAL_INDEX_BUF+1) return 0;
	if (StructuredIndex) {
		if (StructuredIndex < MaxNum8bPartition - 1) {
			indexattr = decode8b(buffer[bdx2+1]);
		}
		else {
			indexattr = decode16b((buffer[bdx2+1] << 8) | (buffer[bdx2 + 2]));
		}
		/* printf("i=%d p=%d\n", indexattr, patattr); */
		if ((patattr > 0) && (indexattr != patattr)) {
#if	BG_DEBUG
			fprintf(debug, "indexattr=%d DOES NOT MATCH patattr=%d\n", indexattr, patattr);
#endif	/*BG_DEBUG*/
			return 0;
		}
	}
	if (PRINTINDEXLINE) {
		c = buffer[bdx2];
		buffer[bdx2] = '\0';
		printf("%s %d", &buffer[1], indexattr);
		buffer[bdx2] = c;
		if (c == ALL_INDEX_MARK) printf(" ! ");
		else printf(" : ");
	}

	if (OneFilePerBlock && (buffer[bdx2] == ALL_INDEX_MARK)) {
		/* A intersection Univ-set = A: so src_index_set won't change; A union Univ-set = Univ-set: so src_index_set = all 1s */
#if	BG_DEBUG
		buffer[bdx2] = '\0';
		fprintf(debug, "All indices search for %s\n", buffer + 1);
		buffer[bdx2] = ALL_INDEX_MARK;
#endif	/*BG_DEBUG*/
		set[REAL_PARTITION - 1] = 1;
		for(bdx2=0; bdx2<round(OneFilePerBlock, 8*sizeof(int)) - 1; bdx2++) {
			set[bdx2] = 0xffffffff;
		}
		set[bdx2] = 0;
		for (j=0; j<8*sizeof(int); j++) {
			if (bdx2*8*sizeof(int) + j >= OneFilePerBlock) break;
			set[bdx2] |= mask_int[j];
		}
		set[REAL_PARTITION - 1] = 1;
		if (ByteLevelIndex && !RecordLevelIndex) NOBYTELEVEL = 1;	/* With RecordLevelIndex, I want NOBYTELEVEL to be unused (i.e., !NOBYTELEVEL is always true) */
		return 1;
	}
	else if (!OneFilePerBlock) {	/* check only if index+partitions are NOT split */
#if	BG_DEBUG
		buffer[bdx2] = '\0';
		fprintf(debug, "memagrep-line: %s\t\tpattern: %s\n", buffer, pattern);
#endif	/*BG_DEBUG*/
		/* ignore if pattern with all its options matches block number sequence: bg+udi: Feb/16/93 */
		buffer[bdx2] = '\n';	/* memagrep needs buffer to end with '\n' */
		if ((ret = memagrep_search(patlen, pattern, bdx2+1, buffer, 0, outfile)) <= 0) return 0;
		else buffer[bdx2] = WORD_END_MARK;
	}
	if ((StructuredIndex > 0) && (StructuredIndex < MaxNum8bPartition - 1)) bdx2 ++;
	else if (StructuredIndex > 0) bdx2 += 2;
	bdx2++;	/* bdx2 now points to the first byte of the offset */

	even_words = 1;
	/* Code identical to that in merge_in() in glimpseindex */
	if (OneFilePerBlock) {
	    get_block_numbers(&buffer[bdx2], &buffer[bdx2], partfp);
	    while((bdx2<REAL_INDEX_BUF) && (buffer[bdx2] != '\n') && (buffer[bdx2] != '\0')) {
		/* First get the file name */
		x = 0;
		if (ByteLevelIndex) {
		    if (OneFilePerBlock <= MaxNum8bPartition) {
			x = decode8b(buffer[bdx2]);
			bdx2 ++;
		    }
		    else if (OneFilePerBlock <= MaxNum16bPartition) {
			x = (buffer[bdx2] << 8) | buffer[bdx2+1];
			x = decode16b(x);
			bdx2 += 2;
		    }
		    else {
			x = (buffer[bdx2] << 16) | (buffer[bdx2+1] << 8) | buffer[bdx2+2];
			x = decode24b(x);
			bdx2 += 3;
		    }
		}
		else if (OneFilePerBlock <= MaxNum8bPartition) {
		    x = decode8b(buffer[bdx2]);
		    bdx2 ++;
		}
		else if (OneFilePerBlock <= MaxNum12bPartition) {
		    if (even_words) {
			x = ((buffer[bdx2+1] & 0x0000000f) << 8) | buffer[bdx2];
			x = decode12b(x);
			bdx2 += 2;
			even_words = 0;
		    }
		    else {	/* odd number of words so far */
			x = ((buffer[bdx2-1] & 0x000000f0) << 4) | buffer[bdx2];
			x = decode12b(x);
			bdx2 ++;
			even_words = 1;
		    }
		}
		else if (OneFilePerBlock <= MaxNum16bPartition) {
		    x = (buffer[bdx2] << 8) | buffer[bdx2+1];
		    x = decode16b(x);
		    bdx2 += 2;
		}
		else {
		    x = (buffer[bdx2] << 16) | (buffer[bdx2+1] << 8) | buffer[bdx2+2];
		    x = decode24b(x);
		    bdx2 += 3;
		}
		if ((last_Y_filenumber > 0) && (x >= last_Y_filenumber)) continue;
		set[block2index(x)] |= block2mask(x);
		if (PRINTINDEXLINE) {
			printf("%d [", x);
		}

		prevy = 0;
		if (ByteLevelIndex) {
		    heado = tailo = NULL;
		    curfreq = 0;
		    while ((bdx2<REAL_INDEX_BUF) && (buffer[bdx2] != '\n') && (buffer[bdx2] != '\0')) {
			y = decode8b(buffer[bdx2]);
			if ((y & 0x000000c0) == 0) {	/* one byte offset */
			    diff = y&0x0000003f;
			    y = prevy + diff;
			    bdx2 ++;
			}
			else if ((y & 0x000000c0) == 0x40) {	/* two byte offset */
			    diff = decode8b(buffer[bdx2+1]);
			    y = prevy + (((y & 0x0000003f) * MaxNum8bPartition) + diff);
			    bdx2 += 2;
			}
			else if ((y & 0x000000c0) == 0x80) {	/* three byte offset */
			    diff = decode16b((buffer[bdx2+1] << 8) | buffer[bdx2+2]);
			    y = prevy + (((y & 0x0000003f) * MaxNum16bPartition) + diff);
			    bdx2 += 3;
			}
			else {	/* four byte offset */
			    diff = decode24b((buffer[bdx2+1] << 16) | (buffer[bdx2+2] << 8) | buffer[bdx2+3]);
			    y = prevy + (((y & 0x0000003f) * MaxNum24bPartition) + diff);
			    bdx2 += 4;
			}
			prevy = y;
			if (PRINTINDEXLINE) {
			    printf(" %d", y);
			}

			curfreq ++;

			if(RecordLevelIndex ||
			   (!(Only_first && !PRINTAPPXFILEMATCH) && !NOBYTELEVEL &&	/* below borrowed from sorted_union */
#if	USEFREQUENCIES
			    !(((prevfreq>MIN_OCCURRENCES)&&(curfreq+*frequency > MAX_UNION*prevfreq)) || (curfreq+*frequency > MAX_ABSOLUTE))
#else
			    1
#endif
			    
			    )
			  ) {
			    /* These o's will be in sorted order. Just collect all of them and merge with &offset_table[x]. */
			    o = (struct offsets *)my_malloc(sizeof(struct offsets));
			    o->offset = y;
			    o->next = NULL;
			    o->sign = o->done = 0;
			    if (heado == NULL) {
				heado = o;
				tailo = o;
			    }
			    else {
				tailo->next = o;
				tailo = o;
			    }
			}
			else if (!RecordLevelIndex) {
			    if (heado != NULL) free_list(&heado);
			    /* printf("1 "); */
			    NOBYTELEVEL = 1;	/* can't return since have to or the bitmasks */
			}
			if ((bdx2<REAL_INDEX_BUF) && (buffer[bdx2] == delim)) {	/* look at offsets corr. to a new file now */
				bdx2 ++;
				break;
			}
		    }

		    if (heado == NULL) *frequency += curfreq;
		    else if (RecordLevelIndex || (!(Only_first && !PRINTAPPXFILEMATCH) && !NOBYTELEVEL)) {
			sorted_union(&offset_table[x], &heado, frequency, prevfreq, curfreq);	/* this will free heado's elements and ++ *frequency */
			if (!RecordLevelIndex && NOBYTELEVEL) *frequency += curfreq;	/* can't return since have to or the bitmasks */
			if (heado != NULL) free_list(&heado);
		    }
		}

		if (PRINTINDEXLINE) {
		    printf("] ");
		}
	    }
	}
	else {
	    while((bdx2<MAX_INDEX_BUF) && (buffer[bdx2] != '\n') && (buffer[bdx2] != '\0') && (buffer[bdx2] < MAX_PARTITION)) {
		if ((last_Y_filenumber > 0) && (p_table[buffer[bdx2]] >= last_Y_filenumber)) {
			bdx2 ++;
			continue;
		}
		if (PRINTINDEXLINE) {
		    for (j=p_table[buffer[bdx2]]; j<p_table[buffer[bdx2] + 1]; j++)
			if ((last_Y_filenumber > 0) && (j >= last_Y_filenumber)) break;
			else printf("%d [] ", j);
		}
		set[buffer[bdx2]] = 1;
		bdx2++;
	    }
	}
	if (PRINTINDEXLINE) {
	    printf("\n");
	}
	return 0;
}

/*
 * This is a very simple function: it gets the list of matched lines from the index,
 * and sets the block numbers corr. to files that need to be searched in "index_tab".
 * It also sets the file-offsets that have to be searched in "offset_tab" (byte-level).
 */
get_index(infile, index_tab, offset_tab, pattern, patlen, patattr, index_argv, index_argc, outfile, partfp, parse, first_time)
char *infile;
unsigned int  *index_tab;
struct offsets **offset_tab;
char *pattern;
int patlen;
int patattr;
char *index_argv[];
int index_argc;
FILE *outfile;
FILE *partfp;
int parse;
int first_time;
{
	int  i=0, j, iii;
	FILE *f_in;
	struct offsets **offsetptr = multi_dest_offset_table[0];	/* cannot be NULL if ByteLevelIndex: main.c takes care of that */
	int ret=0;
 
	if (OneFilePerBlock && (parse & OR_EXP) && (index_tab[REAL_PARTITION - 1] == 1)) return 0;
	if (((infile == NULL) || !strcmp(infile, "")) /* || (index_tab == NULL) || (offset_tab == NULL) || (pattern == NULL)*/) return -1;
	if((f_in = fopen(infile, "r")) == NULL) {
		fprintf(stderr, "%s: can't open for reading: %s/%s\n", GProgname, INDEX_DIR, infile);
		return -1;
	}
	if (OneFilePerBlock)
	    for(i=0; i<round(OneFilePerBlock, 8*sizeof(int)); i++) {
                dest_index_set[i] = 0;
	    }
	else for(i=0; i<MAX_PARTITION; i++) {
                dest_index_set[i] = 0;
	    }
	dest_index_buf[0] = '\n';	/* memagrep needs buffer to begin with '\n' */

	dest_index_set[REAL_PARTITION - 2] = 0;
	while(fgets(dest_index_buf+1, REAL_INDEX_BUF-1, f_in)) {
#if	BG_DEBUG
		fprintf(debug, "index-line: %s", dest_index_buf+1);
#endif	/*BG_DEBUG*/
		if ((ret = get_set(&dest_index_buf[0], dest_index_set, offsetptr, patlen, pattern, patattr, outfile, partfp, &dest_index_set[REAL_PARTITION - 2], index_tab[REAL_PARTITION - 2])) != 0)
			break;	/* all index mark touched */
	}
	if (!RecordLevelIndex && NOBYTELEVEL) {
	    for (iii=0; iii<OneFilePerBlock; iii++) {
		free_list(&offset_tab[iii]);
		free_list(&offsetptr[iii]);
	    }
	}

	if (INVERSE) {
	    if (OneFilePerBlock) {
		if (ByteLevelIndex && !RecordLevelIndex) NOBYTELEVEL = 1;	/* can't collect all offsets where pattern DOES NOT occur! */
		for (i=0; i<round(OneFilePerBlock, 8*sizeof(int)) - 1; i++) dest_index_set[i] = ~dest_index_set[i];
		for (j=0; j<8*sizeof(int); j++) {
		    if (i*8*sizeof(int) + j >= OneFilePerBlock) break;
		    if (dest_index_set[i] & mask_int[j]) dest_index_set[i] &= ~mask_int[j];
		    else dest_index_set[i] |= mask_int[j];
		}
	    }
	    else {
		for(i=0; i<MAX_PARTITION; i++) {
		    if (i>=GNumpartitions-1) break;	/* STUPID: get_table returns 1 + part_num, where part_num was no. of partitions glimpseindex found */
		    if ((i == 0) || (i == '\n')) continue;
		    if (dest_index_set[i]) dest_index_set[i] = 0;
		    else dest_index_set[i] = 1;
		}
	    }
	}

	/* Take intersection if parse=ANDPAT or 0 (one terminal pattern), union if OR_EXP; Take care of universal sets in index_tab[REAL_PARTITION - 1] */
	if (OneFilePerBlock) {
	    if (parse & OR_EXP) {
		if (ret) {
		ret_is_1:
		    index_tab[REAL_PARTITION - 1] = 1;
		    for(i=0; i<round(OneFilePerBlock, 8*sizeof(int)) - 1; i++) {
			index_tab[i] = 0xffffffff;
		    }
		    index_tab[i] = 0;
		    for (j=0; j<8*sizeof(int); j++) {
			if (i*8*sizeof(int) + j >= OneFilePerBlock) break;
			index_tab[i] |= mask_int[j];
		    }
		    if (ByteLevelIndex && !RecordLevelIndex && !NOBYTELEVEL && !(Only_first && !PRINTAPPXFILEMATCH)) for (i=0; i<OneFilePerBlock; i++) {	/* collect as many offsets as possible with RecordLevelIndex: free offset_tables at the end of process_query() */
			free_list(&offsetptr[i]);
			free_list(&offset_tab[i]);
		    }
		    if (ByteLevelIndex && !RecordLevelIndex) NOBYTELEVEL = 1;
		    fclose(f_in);
		    return 0;
		}
		index_tab[REAL_PARTITION - 1] = 0;
		for (i=0; i<round(OneFilePerBlock, 8*sizeof(int)); i++) index_tab[i] |= dest_index_set[i];
		if (ByteLevelIndex && !NOBYTELEVEL && (RecordLevelIndex || !(Only_first && !PRINTAPPXFILEMATCH))) {
		    for (i=0; i<OneFilePerBlock; i++) {
			sorted_union(&offset_tab[i], &offsetptr[i], &index_tab[REAL_PARTITION - 2], dest_index_set[REAL_PARTITION - 2], 0);
			if (!RecordLevelIndex && NOBYTELEVEL) {	/* collect as many offsets as possible with RecordLevelIndex: free offset_tables at the end of process_query() */
			    for (iii=0; iii<OneFilePerBlock; iii++) {
				free_list(&offset_tab[iii]);
				free_list(&offsetptr[iii]);
			    }
			    break;
			}
		    }
		}
	    }
	    else {
		if (((index_tab[REAL_PARTITION - 1] == 1) || first_time) && (ret)) {
		both_are_1:
		    if (first_time) {
			index_tab[REAL_PARTITION - 1] = 1;
			for(i=0; i<round(OneFilePerBlock, 8*sizeof(int)) - 1; i++) {
			    index_tab[i] = 0xffffffff;
			}
			index_tab[i] = 0;
			for (j=0; j<8*sizeof(int); j++) {
			    if (i*8*sizeof(int) + j >= OneFilePerBlock) break;
			    index_tab[i] |= mask_int[j];
			}
		    }
		    first_time = 0;
		    if (ByteLevelIndex && !RecordLevelIndex && !NOBYTELEVEL && !(Only_first && !PRINTAPPXFILEMATCH)) for (i=0; i<OneFilePerBlock; i++) {	/* collect as many offsets as possible with RecordLevelIndex: free offset_tables at the end of process_query() */
			free_list(&offsetptr[i]);
			free_list(&offset_tab[i]);
		    }
		    if (ByteLevelIndex && !RecordLevelIndex) NOBYTELEVEL = 1;
		    /*
		    fclose(f_in);
		    return 0;
		    */
		}
		else if ((index_tab[REAL_PARTITION - 1] == 1) || first_time) {
		    first_time = 0;
		    index_tab[REAL_PARTITION - 1] = 0;
		    for (i=0; i<round(OneFilePerBlock, 8*sizeof(int)); i++) index_tab[i] = dest_index_set[i];
		    if (ByteLevelIndex && !NOBYTELEVEL && (RecordLevelIndex || !(Only_first && !PRINTAPPXFILEMATCH))) {
			for (i=0; i<OneFilePerBlock; i++) {
			    free_list(&offset_tab[i]);
			    offset_tab[i] = offsetptr[i];
			    offsetptr[i] = NULL;
			}
		    }
		}
		else if (ret) {
		    if (ByteLevelIndex && !RecordLevelIndex && !NOBYTELEVEL && !(Only_first && !PRINTAPPXFILEMATCH)) for (i=0; i<OneFilePerBlock; i++) free_list(&offsetptr[i]);	/* collect as many offsets as possible with RecordLevelIndex: free offset_tables at the end of process_query() */
		}
		else {
		    for (i=0; i<round(OneFilePerBlock, 8*sizeof(int)); i++) index_tab[i] &= dest_index_set[i];
		    if (ByteLevelIndex && !NOBYTELEVEL && (RecordLevelIndex || !(Only_first && !PRINTAPPXFILEMATCH))) {
			if (first_time || WHOLEFILESCOPE) {
			    first_time = 0;
			    for (i=0; i<OneFilePerBlock; i++) {
				sorted_union(&offset_tab[i], &offsetptr[i], &index_tab[REAL_PARTITION - 2], dest_index_set[REAL_PARTITION - 2], 0);
				if (!RecordLevelIndex && NOBYTELEVEL) {	/* collect as many offsets as possible with RecordLevelIndex: free offset_tables at the end of process_query() */
				    for (iii=0; iii<OneFilePerBlock; iii++) {
					free_list(&offset_tab[iii]);
					free_list(&offsetptr[iii]);
				    }
				    break;
				}
			    }
			}
			else {
			    for (i=0; i<OneFilePerBlock; i++) {
				if ((index_tab[block2index(i)] & mask_int[i % (8*sizeof(int))]))
				    sorted_intersection(i, &offset_tab[i], &offsetptr[i], &index_tab[REAL_PARTITION - 2]);
				else free_list(&offsetptr[i]);
				/*
				if (index_tab[REAL_PARTITION - 2] < MIN_OCCURRENCES) {
				    if (!NOBYTELEVEL) {
					    for (iii=0; iii<OneFilePerBlock; iii++) {
						free_list(&offset_tab[iii]);
						free_list(&offsetptr[iii]);
					    }
				    }
				    NOBYTELEVEL = 1;
				    OPTIMIZEBYTELEVEL = 1;
				    break;
				}
				*/
			    }
			}
		    }
		}
	    }
	}
	else {
	    if (parse & OR_EXP)
		for(i=0; i<MAX_PARTITION; i++) index_tab[i] |= dest_index_set[i];
	    else
		for(i=0; i<MAX_PARTITION; i++) index_tab[i] &= dest_index_set[i];
	}

#if	BG_DEBUG
	fprintf(debug, "get_index(): the following partitions are ON\n");
	for(i=0; i<((OneFilePerBlock > 0) ? round(OneFilePerBlock, 8*sizeof(int)) : MAX_PARTITION); i++) {
	      if(index_tab[i]) fprintf(debug, "%d,%x\n", i, index_tab[i]);
	}
#endif	/*BG_DEBUG*/

	fclose(f_in);
	return 0;
}

/*
 * Same as above, but uses mgrep to search the index for many patterns at one go,
 * and interprets the output obtained from the -M and -P options (set in main.c).
 */
mgrep_get_index(infile, index_tab, offset_tab, pat_list, pat_lens, pat_attr, mgrep_pat_index, num_mgrep_pat, patbufpos, index_argv, index_argc, outfile, partfp, parse, first_time)
char *infile;
unsigned int  *index_tab;
struct offsets **offset_tab;
char *pat_list[];
int pat_lens[];
int pat_attr[];
int mgrep_pat_index[];
int num_mgrep_pat;
int patbufpos;
char *index_argv[];
int index_argc;
FILE *outfile;
FILE *partfp;
int parse;
int first_time;
{
	int  i=0, j, temp, iii, jjj;
	FILE *f_in;
	int ret;
	int x=0, y=0, even_words=1;
	int patnum;
	unsigned int *setptr;
	struct offsets **offsetptr;
	CHAR dummypat[MAX_PAT];
	int  dummylen=0;
	char  allindexmark[MAXNUM_PAT];
	int k;
	int sorted[MAXNUM_PAT], min, max;
 
	if (OneFilePerBlock && (parse & OR_EXP) && (index_tab[REAL_PARTITION - 1] == 1)) return 0;
	/* Do the mgrep() */
	if ((f_in = fopen(infile, "w")) == NULL) {
		fprintf(stderr, "%s: run out of file descriptors!\n", GProgname);
		return -1;
	}
	errno = 0;
	if ((ret = fileagrep(index_argc, index_argv, 0, f_in)) < 0) {
		fprintf(stderr, "%s: error in searching index\n", HARVEST_PREFIX);
		fclose(f_in);
		return -1;
	}
	fflush(f_in);
	fclose(f_in);
	f_in = NULL;

	index_argv[patbufpos] = NULL;
	/* For index-search with memgrep and get-filenames */
	dummypat[0] = '\0';
	if ((dummylen = memagrep_init(index_argc, index_argv, MAX_PAT, dummypat)) <= 0) {
		fclose(f_in);
		return -1;
	}

	/* Interpret the result */
	if((f_in = fopen(infile, "r")) == NULL) {
		fprintf(stderr, "%s: can't open for reading: %s/%s\n", GProgname, INDEX_DIR, infile);
		return -1;
	}
	if (OneFilePerBlock) {
	    for (patnum=0; patnum<num_mgrep_pat; patnum ++) {
		for(i=0; i<round(OneFilePerBlock, 8*sizeof(int)); i++) {
			multi_dest_index_set[patnum][i] = 0;
		}
		if (ByteLevelIndex) for(i=0; i<OneFilePerBlock; i++) {
			free_list(&multi_dest_offset_table[patnum][i]);
			/* multi_dest_offset_table[patnum][i] = NULL; bg, 28/9/1995 */
		}
		multi_dest_index_set[patnum][REAL_PARTITION - 1] = 0;
		multi_dest_index_set[patnum][REAL_PARTITION - 2] = 0;
	    }
	}
	else {
	    for (patnum=0; patnum<num_mgrep_pat; patnum ++)
		for(i=0; i<MAX_PARTITION; i++) {
                	multi_dest_index_set[patnum][i] = 0;
		}
	}
	dest_index_buf[0] = '\n';	/* memagrep needs buffer to begin with '\n' */

	memset(allindexmark, '\0', num_mgrep_pat);
	min = (index_tab[REAL_PARTITION - 1] == 1) ? 0 : index_tab[REAL_PARTITION - 2];
	while(fgets(dest_index_buf+1, REAL_INDEX_BUF, f_in)) {
		patnum=0;
		sscanf(&dest_index_buf[1], "%d-", &patnum);
#if	BG_DEBUG
		fprintf(debug, "patnum=%d len=%d pat=%s attr=%d index-line: %s\n", patnum, pat_lens[mgrep_pat_index[patnum-1]], pat_list[mgrep_pat_index[patnum-1]], pat_attr[mgrep_pat_index[patnum-1]], dest_index_buf+1);
#endif	/*BG_DEBUG*/
		if ((patnum < 1) || (patnum > num_mgrep_pat)) continue;	/* error! */
		setptr = multi_dest_index_set[patnum - 1];
		offsetptr = multi_dest_offset_table[patnum - 1];
		for(k=0; dest_index_buf[k] != ' '; k++);
		dest_index_buf[k] = '\n';
		if (!allindexmark[patnum - 1])
			allindexmark[patnum - 1] = (char)get_set(&dest_index_buf[k], setptr, offsetptr, pat_lens[mgrep_pat_index[patnum-1]],
								pat_list[mgrep_pat_index[patnum-1]], pat_attr[mgrep_pat_index[patnum-1]], outfile, partfp,
								&setptr[REAL_PARTITION - 2], min);
		/* To test the maximum disparity to stop unions within above */
		if (!allindexmark[patnum-1]) min = setptr[REAL_PARTITION - 2];
		for (patnum=0; patnum<num_mgrep_pat; patnum ++) {
			if ((multi_dest_index_set[patnum][REAL_PARTITION - 2] < min) && (multi_dest_index_set[patnum][REAL_PARTITION - 1] != 1))
				min = multi_dest_index_set[patnum][REAL_PARTITION - 2];
		}
		min += (index_tab[REAL_PARTITION - 1] == 1) ? 0 : index_tab[REAL_PARTITION - 2];
	}
#if	0
	for (patnum=0; patnum<num_mgrep_pat; patnum++)
		printf("%d=%d,%d\n", patnum, multi_dest_index_set[patnum][REAL_PARTITION - 1], multi_dest_index_set[patnum][REAL_PARTITION - 2]);
#endif	/*0*/

	for (patnum=0; patnum<num_mgrep_pat; patnum++)
		sorted[patnum] = patnum;
	if (ByteLevelIndex && !NOBYTELEVEL && (RecordLevelIndex || !(Only_first && !PRINTAPPXFILEMATCH))) {
		max = 0;
		for (patnum=1; patnum<num_mgrep_pat; patnum++) {
		    if (multi_dest_index_set[patnum][REAL_PARTITION - 2] > multi_dest_index_set[max][REAL_PARTITION - 2])
			max = patnum;
		}
		/* Sort them according to the lengths of the lists in increasing order: min first */
		for (patnum=0; patnum<num_mgrep_pat; patnum++) {
		    min = patnum;
		    for (j=patnum+1; j<num_mgrep_pat; j++)
			if (multi_dest_index_set[sorted[j]][REAL_PARTITION - 2] < multi_dest_index_set[sorted[min]][REAL_PARTITION - 2])
			    min = j;
		    if (min != patnum) {
			temp = sorted[patnum];
			sorted[patnum] = sorted[min];
			sorted[min] = temp;
		    }
		}
#if	USEFREQUENCIES
		if (!RecordLevelIndex && (multi_dest_index_set[sorted[max]][REAL_PARTITION - 2] > MAX_DISPARITY * multi_dest_index_set[sorted[0]][REAL_PARTITION - 2])) {
		    NOBYTELEVEL = 1;
		    /* printf("4 "); */
		    for (iii=0; iii<OneFilePerBlock; iii++) {
			for (jjj=0; jjj<num_mgrep_pat; jjj++)
			    free_list(&multi_dest_offset_table[jjj][iii]);
			free_list(&offset_tab[iii]);
		    }
		}
#endif
	}
	else if (!RecordLevelIndex && NOBYTELEVEL) {
	    for (iii=0; iii<OneFilePerBlock; iii++) {
		for (jjj=0; jjj<num_mgrep_pat; jjj++)
		    free_list(&multi_dest_offset_table[jjj][iii]);
		free_list(&offset_tab[iii]);
	    }
	}

	/* Take intersection if parse=ANDPAT or 0 (one terminal pattern), union if OR_EXP; Take care of universal sets in offset_tab[REAL_PARTITION - 1] */
	for (patnum=0; patnum<num_mgrep_pat; patnum++) {
		if (OneFilePerBlock) {
		    if (parse & OR_EXP) {
			if (allindexmark[sorted[patnum]]) {
			ret_is_1:
			    index_tab[REAL_PARTITION - 1] = 1;
			    for(i=0; i<round(OneFilePerBlock, 8*sizeof(int)) - 1; i++) {
				index_tab[i] = 0xffffffff;
			    }
			    index_tab[i] = 0;
			    for (j=0; j<8*sizeof(int); j++) {
				if (i*8*sizeof(int) + j >= OneFilePerBlock) break;
				index_tab[i] |= mask_int[j];
			    }
			    if (ByteLevelIndex && !RecordLevelIndex && !NOBYTELEVEL && !(Only_first && !PRINTAPPXFILEMATCH))	/* collect as many offsets as possible with RecordLevelIndex: free offset_tables at the end of process_query() */
			      for (i=0; i<OneFilePerBlock; i++) {
			        for (patnum=0;patnum<num_mgrep_pat;patnum++)
				  free_list(&multi_dest_offset_table[sorted[patnum]][i]);
				free_list(&offset_tab[i]);
			      }
			    if (ByteLevelIndex && !RecordLevelIndex) NOBYTELEVEL = 1;
			    fclose(f_in);
			    return 0;
			}
			index_tab[REAL_PARTITION - 1] = 0;
			for (i=0; i<round(OneFilePerBlock, 8*sizeof(int)); i++) index_tab[i] |= multi_dest_index_set[sorted[patnum]][i];
			if (ByteLevelIndex && !NOBYTELEVEL && (RecordLevelIndex || !(Only_first && !PRINTAPPXFILEMATCH))) {
			    for (i=0; i<OneFilePerBlock; i++) {
				sorted_union(&offset_tab[i], &multi_dest_offset_table[sorted[patnum]][i], &index_tab[REAL_PARTITION - 2], multi_dest_index_set[sorted[patnum]][REAL_PARTITION - 2], 0);

				if (!RecordLevelIndex && NOBYTELEVEL) {	/* collect as many offsets as possible with RecordLevelIndex: free offset_tables at the end of process_query() */
				    for (iii=0; iii<OneFilePerBlock; iii++) {
					for (jjj=0; jjj<num_mgrep_pat; jjj++)
					    free_list(&multi_dest_offset_table[jjj][iii]);
					free_list(&offset_tab[iii]);
				    }
				    break;
				}
			    }
			}
		    }
		    else {
			if (((index_tab[REAL_PARTITION - 1] == 1) || first_time) && (allindexmark[sorted[patnum]])) {
			both_are_1:
			    if (first_time) {
				index_tab[REAL_PARTITION - 1] = 1;
				for(i=0; i<round(OneFilePerBlock, 8*sizeof(int)) - 1; i++) {
				    index_tab[i] = 0xffffffff;
				}
				index_tab[i] = 0;
				for (j=0; j<8*sizeof(int); j++) {
				    if (i*8*sizeof(int) + j >= OneFilePerBlock) break;
				    index_tab[i] |= mask_int[j];
				}
			    }
			    first_time = 0;
			    if (ByteLevelIndex && !RecordLevelIndex && !NOBYTELEVEL && !(Only_first && !PRINTAPPXFILEMATCH))	/* collect as many offsets as possible with RecordLevelIndex: free offset_tables at the end of process_query() */
			      for (i=0; i<OneFilePerBlock; i++) {
			        for (patnum=0;patnum<num_mgrep_pat;patnum++)
			 	  free_list(&multi_dest_offset_table[sorted[patnum]][i]);
			 	free_list(&offset_tab[i]);
			      }
			    if (ByteLevelIndex && !RecordLevelIndex) NOBYTELEVEL = 1;
			    /*
			    fclose(f_in);
			    return 0;
			    */
			}
			else if ((index_tab[REAL_PARTITION - 1] == 1) || first_time) {
			    first_time = 0;
			    index_tab[REAL_PARTITION - 1] = 0;
			    for (i=0; i<round(OneFilePerBlock, 8*sizeof(int)); i++) index_tab[i] = multi_dest_index_set[sorted[patnum]][i];
			    if (ByteLevelIndex && !NOBYTELEVEL && (RecordLevelIndex || !(Only_first && !PRINTAPPXFILEMATCH))) {
				for (i=0; i<OneFilePerBlock; i++) {
				    free_list(&offset_tab[i]);
				    offset_tab[i] = multi_dest_offset_table[sorted[patnum]][i];
				    multi_dest_offset_table[sorted[patnum]][i] = NULL;
				}
			    }
			}
			else if (allindexmark[sorted[patnum]]) {
			    if (ByteLevelIndex && !RecordLevelIndex && !NOBYTELEVEL && !(Only_first && !PRINTAPPXFILEMATCH))	/* collect as many offsets as possible with RecordLevelIndex: free offset_tables at the end of process_query() */
			      for (i=0; i<OneFilePerBlock; i++) free_list(&multi_dest_offset_table[sorted[patnum]][i]);
			}
			else {
			    for (i=0; i<round(OneFilePerBlock, 8*sizeof(int)); i++) index_tab[i] &= multi_dest_index_set[sorted[patnum]][i];
			    if (ByteLevelIndex && !NOBYTELEVEL && (RecordLevelIndex || !(Only_first && !PRINTAPPXFILEMATCH))) {
				if (first_time || WHOLEFILESCOPE) {
				    first_time = 0;
				    for (i=0; i<OneFilePerBlock; i++) {
					sorted_union(&offset_tab[i], &multi_dest_offset_table[sorted[patnum]][i], &index_tab[REAL_PARTITION - 2], multi_dest_index_set[sorted[patnum]][REAL_PARTITION - 2], 0);
					if (!RecordLevelIndex && NOBYTELEVEL) {
					    for (iii=0; iii<OneFilePerBlock; iii++) {
						for (jjj=0; jjj<num_mgrep_pat; jjj++)
						    free_list(&multi_dest_offset_table[jjj][iii]);
						free_list(&offset_tab[iii]);
					    }
					    break;
					}
				    }
				}
				else {
				    for (i=0; i<OneFilePerBlock; i++) {
					if ((index_tab[block2index(i)] & mask_int[i % (8*sizeof(int))]))
					    sorted_intersection(i, &offset_tab[i], &multi_dest_offset_table[sorted[patnum]][i], &index_tab[REAL_PARTITION - 2]);
					else free_list(&multi_dest_offset_table[sorted[patnum]][i]);
					/*
					if (index_tab[REAL_PARTITION - 2] < MIN_OCCURRENCES) {
					    if (!NOBYTELEVEL) {
						    for (iii=0; iii<OneFilePerBlock; iii++) {
							for (jjj=0; jjj<num_mgrep_pat; jjj++)
							    free_list(&multi_dest_offset_table[jjj][iii]);
							free_list(&offset_tab[iii]);
						    }
					    }
					    NOBYTELEVEL = 1;
					    OPTIMIZEBYTELEVEL = 1;
					    break;
					}
					*/
				    }
				}
			    }
			}
		    }
		}
		else {
		    if (parse & OR_EXP) {
			for (patnum=0; patnum<num_mgrep_pat; patnum++)
				for(i=0; i<MAX_PARTITION; i++) index_tab[i] |= multi_dest_index_set[patnum][i];
		    }
		    else {
			for (patnum=0; patnum<num_mgrep_pat; patnum++)
				for(i=0; i<MAX_PARTITION; i++) index_tab[i] &= multi_dest_index_set[patnum][i];
		    }
		}
	}

#if	BG_DEBUG
	fprintf(debug, "get_index(): the following partitions are ON\n");
	for(i=0; i<((OneFilePerBlock > 0) ? round(OneFilePerBlock, 8*sizeof(int)) : MAX_PARTITION); i++) {
	      if(index_tab[i]) fprintf(debug, "%d,%x\n", i, index_tab[i]);
	}
#endif	/*BG_DEBUG*/

	fclose(f_in);
	return 0;
}

/* All borrowed from main.c and are needed for searching the index */
extern	CHAR	*pat_list[MAXNUM_PAT];  /* complete words within global pattern */
extern	int	pat_lens[MAXNUM_PAT];   /* their lengths */
extern	int	pat_attr[MAXNUM_PAT];   /* set of attributes */
extern	int	num_pat;
extern	CHAR	pat_buf[(MAXNUM_PAT + 2)*MAXPAT];
extern	int	pat_ptr;
extern	int	is_mgrep_pat[MAXNUM_PAT];
extern	int	mgrep_pat_index[MAXNUM_PAT];
extern	int	num_mgrep_pat;
extern	unsigned int	*src_index_set;
extern	struct offsets **src_offset_table;
extern	char	tempfile[];
extern	int	patindex;
extern	int	patbufpos;
extern	ParseTree terminals[MAXNUM_PAT];
extern	int	GBESTMATCH;		/* Should I change -B to -# where # = no. of errors? */
extern	int	bestmatcherrors;	/* set during index search, used later on */
extern	FILE	*partfp;		/* glimpse partitions */
extern	FILE	*nullfp;		/* to discard output: agrep -s doesn't work properly */
extern	int	ComplexBoolean;
extern	int	num_terminals;
#if	0
extern  struct token *hash_table[MAX_64K_HASH];
#else	/*0*/
extern	int	mini_array_len;
#endif	/*0*/
extern  int	WORDBOUND, NOUPPER, D, LINENUM;

int
veryfastsearch(argc, argv, num_pat, pat_list, pat_lens, minifp)
	int	argc;
	char	*argv[];
	int	num_pat;
	CHAR	*pat_list[MAXNUM_PAT];
	int	pat_lens[MAXNUM_PAT];
	FILE	*minifp;
{
	/*
	 * Figure out from options if very fast search is possible.
	 */
	if (minifp == NULL) return 0;
	if (!OneFilePerBlock) return 0;	/* you did not build index for speed anyway */
	if (!(WORDBOUND && NOUPPER && (D<=0))) return 0;
	if (LINENUM) return 0;
	return 1;
	/* if ((num_mgrep_pat == num_pat) || ((1 == num_pat) && (1 == checksg(pat_list[0], D, 0)))) return 1; */
	/* either all >= 2 patterns are mgrep-able (simple) or there is just one simple pattern: i.e., "cast" can be used! */
	/* return 0; */
}

int
mini_agrep(inword, inlen, outfp)
	CHAR	*inword;
	int	inlen;
	FILE	*outfp;
{
	static struct stat st;
	static	int statted = 0;
	unsigned char	s[MAX_LINE_LEN], word[MAX_NAME_LEN];
	long	beginoffset, endoffset, curroffset;
	unsigned char c;
	int	j, num = 0, cmp, len;

	if (!statted) {
		sprintf((char*)s, "%s/%s", INDEX_DIR, INDEX_FILE);
		if (stat(s, &st) == -1) {
			fprintf(stderr, "Can't stat file: %s\n", s);
			exit(2);
		}
		statted = 1;
	}

	j = 0;
	while (*inword) {
		if (*inword == '\\') {
			inword++;
			continue;
		}
		if (isupper(*(unsigned char *)inword)) word[j] = tolower(*(unsigned char *)inword);
		else word[j] = *inword;
		j++;
		inword ++;
	}
	word[j] = '\0';
	len = j;

	if (!get_mini(word, len, &beginoffset, &endoffset, 0, mini_array_len, minifp)) return 0;
	if (endoffset == -1) endoffset = st.st_size;
	if (endoffset <= beginoffset) return 0;

	/* We must find all occurrences of the word (in all attributes) so can't quit when we find the first match */
	fseek(indexfp, beginoffset, 0);
	curroffset = ftell(indexfp);	/* = beginoffset */
	while ((curroffset < endoffset) && (fgets(s, MAX_LINE_LEN, indexfp) != NULL)) {
		j = 0;
		while ((j < MAX_LINE_LEN) && (s[j] != WORD_END_MARK) && (s[j] != ALL_INDEX_MARK) && (s[j] != '\0') && (s[j] != '\n')) j++;
		if ((j >= MAX_LINE_LEN) || (s[j] == '\0') || (s[j] == '\n')) {
			curroffset = ftell(indexfp);
			continue;
		}
		/* else it is WORD_END_MARK or ALL_INDEX_MARK */
		c = s[j];
		s[j] = '\0';
		cmp = strcmp(word, s);
#if	WORD_SORTED
		if (cmp < 0) break;	/* since index is sorted by word */
		else
#endif	/* WORD_SORTED */
		if (cmp != 0) {	/* not IDENTICALLY EQUAL */
			s[j] = c;
			curroffset = ftell(indexfp);
			continue;
		}
		s[j] = c;
		fputs(s, outfp);
		num++;
		curroffset = ftell(indexfp);
	}
	return num;
}

/* Returns the number of times a successful search was conducted: unused info at present. */
fillup_target(result_index_set, result_offset_table, parse)
	unsigned int	*result_index_set;
	struct offsets **result_offset_table;
	long	parse;
{
	int	i=0;
	FILE	*tmpfp;
	int	dummylen = 0;
	char	dummypat[MAX_PAT];
	int	successes = 0, ret;
	int	first_time = 1;
	extern int	veryfast;
	int	prev_INVERSE = INVERSE;

	veryfast = veryfastsearch(index_argc, index_argv, num_pat, pat_list, pat_lens, minifp);
	while (i < num_pat)  {
		if (!veryfast) {
		if (is_mgrep_pat[i] && (num_mgrep_pat > 1)) {	/* do later */
			i++;
			continue;
		}
		strcpy(index_argv[patindex], pat_list[i]);	/* i-th pattern in its right position */
		}
		/* printf("pat_list[%d] = %s\n", i, pat_list[i]); */

		if ((tmpfp = fopen(tempfile, "w")) == NULL) {
			fprintf(stderr, "%s: cannot open for writing: %s, errno=%d\n", GProgname, tempfile, errno);
			return(-1);
		}
		errno = 0;

/* do we need to check is_mgrep_pat[i] here? */
		if (veryfast && is_mgrep_pat[i]) {
			ret = mini_agrep(pat_list[i], pat_lens[i], tmpfp);
		}
		/* If this is the glimpse server, since the process doesn't die, most of its data pages might still remain in memory */
		else if ((ret = fileagrep(index_argc, index_argv, 0, tmpfp)) < 0) {
			/* reinitialization here takes care of agrep_argv changes AFTER split_pattern */
			fprintf(stderr, "%s: error in searching index\n", HARVEST_PREFIX);
			fclose(tmpfp);
			return(-1);
		}
		/* Now, the output of index search is in tempfile: need to use files here since index is too large */
		fflush(tmpfp);
		fclose(tmpfp);
		tmpfp = NULL;

		/* Keep track of the maximum number of errors: will never enter veryfast */
		if (GBESTMATCH) {
			if (errno > bestmatcherrors)
				bestmatcherrors = errno;
		}

		/* At this point, all index-search options are properly set due to the above fileagrep */
		INVERSE = prev_INVERSE;
		if (-1 ==get_index(tempfile, result_index_set, result_offset_table, pat_list[i], pat_lens[i], pat_attr[i], index_argv, index_argc, nullfp, partfp, parse, first_time))
			return(-1);
		successes ++;
		first_time = 0;
		i++;
	}
	fflush(stderr);
	if (veryfast) return successes;

	/* For index-search with memgrep in mgrep_get_index, and get-filenames */
	dummypat[0] = '\0';
	if ((dummylen = memagrep_init(index_argc, index_argv, MAX_PAT, dummypat)) <= 0) return(-1);
	if (num_mgrep_pat > 1) {
		CHAR *old_buf = (CHAR *)index_argv[patbufpos];	/* avoid my_free and re-my_malloc */
		index_argv[patbufpos] = (char*)pat_buf;	/* this contains all the patterns with the right -m and -M options */
#if	BG_DEBUG
		fprintf(debug, "pat_buf = %s\n", pat_buf);
#endif	/*BG_DEBUG*/
		strcpy(index_argv[patindex], "-z");	/* no-op: patterns are in patbufpos; also avoid shift-left of index_argv */
		if (-1 == mgrep_get_index(tempfile, result_index_set, result_offset_table,
				pat_list, pat_lens, pat_attr, mgrep_pat_index, num_mgrep_pat, patbufpos,
				index_argv, index_argc, nullfp, partfp, parse, first_time)) {
			index_argv[patbufpos] = (char *)old_buf;	/* else will my_free array! */
			fprintf(stderr, "%s: error in searching index\n", HARVEST_PREFIX);
			return(-1);
		}
		successes ++;
		first_time = 0;
		index_argv[patbufpos] = (char *)old_buf;
	}

	return successes;
}

/*
 * Now, I search the index by doing an in-order traversal of the boolean parse tree starting at GParse.
 * The results at each node are stored in src_offset_table and src_index_set. Before the right child is
 * evaluated, results of the left child are stored in curr_offset_table and curr_index_set (accumulators)
 * and are unioned/intersected/noted with the right child's results (which get stored in src_...) and
 * passed on above. The accumulators are allocated at each internal node and freed after evaluation.
 * Left to right evaluation is good since number of curr_offset_tables that exist simultaneously depends
 * entirely on the maximum depth of a right branch (REAL_PARTITION is small so it won't make a difference).
 */
int
search_index(tree)
	ParseTree *tree;
{
	int	prev_INVERSE;
	int	i, j, iii;
	int	first_time = 0;	/* since it is used AFTER left child has been computed */
	unsigned int	*curr_index_set = NULL;
	struct offsets **curr_offset_table = NULL;

	if (ComplexBoolean) {	/* recursive */
		if (tree == NULL) return -1;
		if (tree->type == LEAF) {
			/* always AND pat of individual words at each term: initialize accordingly */
			if (OneFilePerBlock) {
				for(i=0; i<round(OneFilePerBlock, 8*sizeof(int)); i++) src_index_set[i] = 0xffffffff;
			}
			else for(i=0; i<MAX_PARTITION; i++) src_index_set[i] = 1;
			src_index_set[REAL_PARTITION - 1] = 0;
			src_index_set[REAL_PARTITION - 2] = 0;

			if (split_terminal(tree->terminalindex, tree->terminalindex+1) <= 0) return -1;
			prev_INVERSE = INVERSE;	/* agrep's global to implement NOT */
			if (tree->op & NOTPAT) INVERSE = 1;
			if (fillup_target(src_index_set, src_offset_table, AND_EXP) <= 0) return -1;
			INVERSE = prev_INVERSE;
			return 1;
		}
		else if (tree->type == INTERNAL) {
			/* Search the left node and see if the right node can be searched */
			if (search_index(tree->data.internal.left) <= 0) return -1;
			if (OneFilePerBlock && ((tree->op & OPMASK) == ORPAT) && (src_index_set[REAL_PARTITION - 1] == 1)) goto quit;	/* nothing to do */
			if ((tree->data.internal.right == NULL) || (tree->data.internal.right->type == 0)) return -1;	/* uninitialized: see main.c */

			curr_index_set = (unsigned int *)my_malloc(sizeof(int)*REAL_PARTITION);
			memset(curr_index_set, '\0', sizeof(int)*REAL_PARTITION);
			/* Save previous src_index_set and src_offset_table in fresh accumulators */
			if (OneFilePerBlock) {
				memcpy(curr_index_set, src_index_set, sizeof(int)*REAL_PARTITION);
				curr_index_set[REAL_PARTITION - 1] = src_index_set[REAL_PARTITION - 1];
				src_index_set[REAL_PARTITION - 1] = 0;
				curr_index_set[REAL_PARTITION - 2] = src_index_set[REAL_PARTITION - 2];
				src_index_set[REAL_PARTITION - 2] = 0;
			}
			else memcpy(curr_index_set, src_index_set, MAX_PARTITION * sizeof(int));
			if (ByteLevelIndex && !NOBYTELEVEL && (RecordLevelIndex || !(Only_first && !PRINTAPPXFILEMATCH))) {
				if ((curr_offset_table = (struct offsets **)my_malloc(sizeof(struct offsets *) * OneFilePerBlock)) == NULL) {
					fprintf(stderr, "%s: malloc failure at: %s:%d\n", GProgname, __FILE__, __LINE__);
					my_free(curr_index_set, REAL_PARTITION*sizeof(int));
					return -1;
				}
				memcpy(curr_offset_table, src_offset_table, OneFilePerBlock * sizeof(struct offsets *));
				memset(src_offset_table, '\0', sizeof(struct offsets *) * OneFilePerBlock);
			}

			/* Now evaluate the right node which automatically put the results in src_index_set/src_offset_table */
			if (search_index(tree->data.internal.right) <= 0) {
				if (curr_offset_table != NULL) free(curr_offset_table);
				my_free(curr_index_set, REAL_PARTITION*sizeof(int));
				return -1;
			}

			/*
			 * Alpha substitution of the code in get_index():
			 * index_tab <- src_index_set
			 * dest_index_table <- curr_index_set
			 * offset_tab <- src_offset_table
			 * dest_offset_table <- curr_offset_table
			 * ret <- src_index_set[REAL_PARTITION - 1] for ORPAT, curr_index_set for ANDPAT
			 * frequency = src_index_set[REAL_PARTITION - 2] in both ORPAT and ANDPAT
			 * first_time <- 0
			 * return 0 <- goto quit
			 * Slight difference since we want the results to go to src rather than curr.
			 */
			if (OneFilePerBlock) {
			    if ((tree->op & OPMASK) == ORPAT) {
				if (src_index_set[REAL_PARTITION - 1] == 1) {	/* curr..[..] can never be 1 since we would have quit above itself */
				ret_is_1:
				    src_index_set[REAL_PARTITION - 1] = 1;
				    for(i=0; i<round(OneFilePerBlock, 8*sizeof(int)) - 1; i++) {
					src_index_set[i] = 0xffffffff;
				    }
				    src_index_set[i] = 0;
				    for (j=0; j<8*sizeof(int); j++) {
					if (i*8*sizeof(int) + j >= OneFilePerBlock) break;
					src_index_set[i] |= mask_int[j];
				    }
				    if (ByteLevelIndex && !NOBYTELEVEL && !(Only_first && !PRINTAPPXFILEMATCH)) for (i=0; i<OneFilePerBlock; i++) {
					free_list(&curr_offset_table[i]);
					if (!RecordLevelIndex) free_list(&src_offset_table[i]);	/* collect as many offsets as possible with RecordLevelIndex: free src_offset_table at the end of process_query() */
				    }
				    if (ByteLevelIndex && !RecordLevelIndex) NOBYTELEVEL = 1;
				    goto quit;
				}
				src_index_set[REAL_PARTITION - 1] = 0;
				for (i=0; i<round(OneFilePerBlock, 8*sizeof(int)); i++) src_index_set[i] |= curr_index_set[i];
				if (ByteLevelIndex && !NOBYTELEVEL && (RecordLevelIndex || !(Only_first && !PRINTAPPXFILEMATCH))) {
				    for (i=0; i<OneFilePerBlock; i++) {
					sorted_union(&src_offset_table[i], &curr_offset_table[i], &src_index_set[REAL_PARTITION - 2], curr_index_set[REAL_PARTITION - 2], 0);
					if (!RecordLevelIndex && NOBYTELEVEL) {	/* collect as many offsets as possible with RecordLevelIndex: free offset_tables at the end of process_query() */
					    for (iii=0; iii<OneFilePerBlock; iii++) {
						free_list(&src_offset_table[iii]);
						free_list(&curr_offset_table[iii]);
					    }
					    break;
					}
				    }
				}
			    }
			    else {
				if (((src_index_set[REAL_PARTITION - 1] == 1) || first_time) && (curr_index_set[REAL_PARTITION - 1] == 1)) {
				both_are_1:
				    if (first_time) {
					src_index_set[REAL_PARTITION - 1] = 1;
					for(i=0; i<round(OneFilePerBlock, 8*sizeof(int)) - 1; i++) {
					    src_index_set[i] = 0xffffffff;
					}
					src_index_set[i] = 0;
					for (j=0; j<8*sizeof(int); j++) {
					    if (i*8*sizeof(int) + j >= OneFilePerBlock) break;
					    src_index_set[i] |= mask_int[j];
					}
				    }
				    first_time = 0;
				    if (ByteLevelIndex && !NOBYTELEVEL && !(Only_first && !PRINTAPPXFILEMATCH)) for (i=0; i<OneFilePerBlock; i++) {	/* collect as many offsets as possible with RecordLevelIndex: free offset_tables at the end of process_query() */
					free_list(&curr_offset_table[i]);
					if (!RecordLevelIndex) free_list(&src_offset_table[i]);	/* collect as many offsets as possible with RecordLevelIndex: free src_offset_table at the end of process_query() */
				    }
				    if (ByteLevelIndex && !RecordLevelIndex) NOBYTELEVEL = 1;
				    /*
				    goto quit;
				    */
				}
				else if ((src_index_set[REAL_PARTITION - 1] == 1) || first_time) {
				    first_time = 0;
				    src_index_set[REAL_PARTITION - 1] = 0;
				    for (i=0; i<round(OneFilePerBlock, 8*sizeof(int)); i++) src_index_set[i] = curr_index_set[i];
				    if (ByteLevelIndex && !NOBYTELEVEL && (RecordLevelIndex || !(Only_first && !PRINTAPPXFILEMATCH))) {
					for (i=0; i<OneFilePerBlock; i++) {
					    free_list(&src_offset_table[i]);
					    src_offset_table[i] = curr_offset_table[i];
					    curr_offset_table[i] = NULL;
					}
				    }
				}
				else if (curr_index_set[REAL_PARTITION - 1] == 1) {
				    if (ByteLevelIndex && !NOBYTELEVEL && !(Only_first && !PRINTAPPXFILEMATCH))	/* collect as many offsets as possible with RecordLevelIndex: free offset_tables at the end of process_query() */
				      for (i=0; i<OneFilePerBlock; i++) free_list(&curr_offset_table[i]);
				}
				else {
				    for (i=0; i<round(OneFilePerBlock, 8*sizeof(int)); i++) src_index_set[i] &= curr_index_set[i];
				    if (ByteLevelIndex && !NOBYTELEVEL && (RecordLevelIndex || !(Only_first && !PRINTAPPXFILEMATCH))) {
					if (first_time || WHOLEFILESCOPE) {
					    first_time = 0;
					    for (i=0; i<OneFilePerBlock; i++) {
						sorted_union(&src_offset_table[i], &curr_offset_table[i], &src_index_set[REAL_PARTITION - 2], curr_index_set[REAL_PARTITION - 2], 0);
						if (!RecordLevelIndex && NOBYTELEVEL) {	/* collect as many offsets as possible with RecordLevelIndex: free offset_tables at the end of process_query() */
						    for (iii=0; iii<OneFilePerBlock; iii++) {
							free_list(&src_offset_table[iii]);
							free_list(&curr_offset_table[iii]);
						    }
						    break;
						}
					    }
					}
					else {
					    for (i=0; i<OneFilePerBlock; i++) {
						if ((src_index_set[block2index(i)] & mask_int[i % (8*sizeof(int))]))
						    sorted_intersection(i, &src_offset_table[i], &curr_offset_table[i], &src_index_set[REAL_PARTITION - 2]);
						else free_list(&curr_offset_table[i]);
						/*
						if (src_index_set[REAL_PARTITION - 2] < MIN_OCCURRENCES) {
						    if (!NOBYTELEVEL) {
							    for (iii=0; iii<OneFilePerBlock; iii++) {
								free_list(&src_offset_table[iii]);
								free_list(&curr_offset_table[iii]);
							    }
						    }
						    NOBYTELEVEL = 1;
						    OPTIMIZEBYTELEVEL = 1;
						    break;
						}
						*/
					    }
					}
				    }
				}
			    }
			}
			else {
			    if ((tree->op & OPMASK) == ORPAT)
				for(i=0; i<MAX_PARTITION; i++) src_index_set[i] |= curr_index_set[i];
			    else
				for(i=0; i<MAX_PARTITION; i++) src_index_set[i] &= curr_index_set[i];
			}

		quit:
			if (curr_offset_table != NULL) free(curr_offset_table);
			/* Now if it is a NOT expression, complement the indices stuff knowing that it cannot be ByteLevelIndex */
			if (tree->op & NOTPAT) {
				if (ByteLevelIndex) {
					/* Can't recover the discarded offsets */
					fprintf(stderr, "%s: can't handle NOT of AND/OR terms with ByteLevelIndex: please simplify the query\n", HARVEST_PREFIX);
					my_free(curr_index_set, REAL_PARTITION*sizeof(int));
					return -1;
				}
				if (OneFilePerBlock)
					for (i=0; i<round(OneFilePerBlock, 8*sizeof(int)); i++) src_index_set[i] = ~src_index_set[i];
				else
					for(i=0; i<MAX_PARTITION; i++) src_index_set[i] = (src_index_set[i] ? 0 : 1);
			}
		}
		else return -1;
	}
	else {	/* iterative just as it is now: initialize */
		if ((long)tree & OR_EXP) {
			if (OneFilePerBlock) for(i=0; i<round(OneFilePerBlock, 8*sizeof(int)); i++) src_index_set[i] = 0;
			else for(i=0; i<MAX_PARTITION; i++) src_index_set[i] = 0;
		}
		else {
			if (OneFilePerBlock) for(i=0; i<round(OneFilePerBlock, 8*sizeof(int)); i++) src_index_set[i] = 0xffffffff;
			else for(i=0; i<MAX_PARTITION; i++) src_index_set[i] = 1;
		}
		src_index_set[REAL_PARTITION - 1] = 0;
		src_index_set[REAL_PARTITION - 2] = 0;

		if (split_terminal(0, num_terminals) <= 0) return -1;
		prev_INVERSE = INVERSE;	/* agrep's global to implement NOT */
		INVERSE = 0;
		if (fillup_target(src_index_set, src_offset_table, (long)tree) <= 0) return -1;
		INVERSE = prev_INVERSE;
	}

	return 1;
}

