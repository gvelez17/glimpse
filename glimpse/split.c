/* Copyright (c) 1994 Burra Gopal, Udi Manber.  All Rights Reserved. */

#include "glimpse.h"

extern CHAR *getword();
extern int checksg();
extern int D;
extern CHAR GProgname[MAXNAME];
extern FILE *debug;
extern int StructuredIndex;
extern int WHOLEFILESCOPE;
extern int ByteLevelIndex;
extern int ComplexBoolean;
extern int foundattr;
extern int foundnot;

/* returns where it found the distinguishing token: until that from prev value of begin is the current pattern (not just the "words" in it) */
CHAR *
parse_flat(begin, end, prev, next)
	CHAR	*begin;
	CHAR	*end;
	int	prev;
	int	*next;
{
	if (begin > end) {
		*next = prev;
		return end;
	}

	if (prev & ENDSUB_EXP) prev &= ~ATTR_EXP;
	if ((prev & ATTR_EXP) && !(prev & VAL_EXP)) prev |= VAL_EXP;

	while (begin <= end) {
		if (*begin == ',') {
			prev |= OR_EXP;
			prev |= VAL_EXP;
			prev |= ENDSUB_EXP;
			if (prev & AND_EXP) {
				fprintf(stderr, "%s: parse error at character '%c'\n", GProgname, *begin);
				return NULL;
			}
			*next = prev;
			return begin;
		}
		else if (*begin == ';') {
			prev |= AND_EXP;
			prev |= VAL_EXP;
			prev |= ENDSUB_EXP;
			if (prev & OR_EXP) {
				fprintf(stderr, "%s: parse error at character '%c'\n", GProgname, *begin);
				return NULL;
			}
			*next = prev;
			return begin;
		}
		else if (*begin == '=') {
			if (StructuredIndex <= 0) begin++;	/* don't care about = since just another character */
			else {
				if (prev & ATTR_EXP) {
					fprintf(stderr, "%s: syntax error: only ',' and ';' can follow 'attribute=value'\n", GProgname);
					return NULL;
				}
				prev |= ATTR_EXP;	/* remains an ATTR_EXP until a new ',' OR ';' */
				prev &= ~VAL_EXP;
				*next = prev;
				return begin;
			}
		}
		else if (*begin == '\\') begin ++;	/* skip two things */
		begin++;
	}

	*next = prev;
	return begin;
}

int
split_pattern_flat(GPattern, GM, APattern, terminals, pnum_terminals, pGParse, num_attr)
	CHAR	*GPattern;
	int	GM;
	CHAR	*APattern;
	ParseTree terminals[];
	int	*pnum_terminals;
	int	*pGParse;	/* doesn't interpret it as a tree */
	int	num_attr;
{
	int   j, k = 0, l = 0, len = 0;
	int   current_attr;
	CHAR  *buffer;
	CHAR  *buffer_pat;
	CHAR  *buffer_end;
	char  tempbuf[MAX_LINE_LEN];

	memset(APattern, '\0', MAXPAT);
	buffer = GPattern;
	buffer_end = buffer + GM;
	j=0;
	*pGParse = 0;
	current_attr = 0;
	foundattr = 0;

	/*
	 * buffer is the runnning pointer, buffer_pat is the place where
	 * the distinguishing delimiter was found, buffer_end is the end.
	 */
	 while (buffer_pat = parse_flat(buffer, buffer_end, *pGParse, pGParse)) {
		/* there is no pattern until after the distinguishing delimiter position: some agrep garbage */
		if (buffer_pat <= buffer) {
			buffer = buffer_pat+1;
			if (buffer_pat >= buffer_end) break;
			continue;
		}
		if ((*pGParse & ATTR_EXP) && !(*pGParse & VAL_EXP)) {	/* fresh attribute */
			foundattr=1;
			memcpy(tempbuf, buffer, buffer_pat - buffer);
			tempbuf[buffer_pat - buffer] = '\0';
			len = strlen(tempbuf);
			for (k = 0; k<len; k++) {
				if (tempbuf[k] == '\\') {
					for (l=k; l<len; l++)
						tempbuf[l] = tempbuf[l+1];
					len --;
				}
			}
			if ( ((current_attr = attr_name_to_id(tempbuf, len)) <= 0) || (current_attr >= num_attr)) {
				buffer[buffer_pat - buffer] = '\0';
				fprintf(stderr, "%s: unknown attribute name '%s'\n", GProgname, buffer);
				return -1;
			}

			buffer = buffer_pat+1;	/* immediate next character after distinguishing delimiter */
			if (buffer_pat >= buffer_end) break;
			continue;
		}
		else {	/* attribute's value OR raw-value */
			if (*pnum_terminals >= MAXNUM_PAT) {
				fprintf(stderr, "%s: boolean expression has too many terms\n", GProgname);
				return -1;
			}
			terminals[*pnum_terminals].op = 0;
			terminals[*pnum_terminals].type = LEAF;
			terminals[*pnum_terminals].terminalindex = *pnum_terminals;
			terminals[*pnum_terminals].data.leaf.attribute = current_attr;	/* default is no structure */
			terminals[*pnum_terminals].data.leaf.value = (CHAR *)malloc(buffer_pat - buffer + 2);
			memcpy(terminals[*pnum_terminals].data.leaf.value, buffer, buffer_pat - buffer);	/* without distinguishing delimiter */
			terminals[*pnum_terminals].data.leaf.value[buffer_pat - buffer] = '\0';
			if (foundattr || WHOLEFILESCOPE) {
				memcpy(&APattern[j], buffer, buffer_pat - buffer);
				j += buffer_pat - buffer;	/* NOT including the distinguishing delimiter at buffer_pat, or '\0' */
				APattern[j++] = (*(buffer_pat + 1) == '\0' ? '\0' : ',');	/* always search for OR, do filtering at the end */
#if	BG_DEBUG
				fprintf(debug, "current_attr = %d, val = %s\n", current_attr, terminals[*pnum_terminals].data.leaf.value);
#endif	/*BG_DEBUG*/
			}
			else {
				memcpy(&APattern[j], buffer, buffer_pat + 1 - buffer);
				j += buffer_pat + 1 - buffer;	/* including the distinguishing delimiter at buffer_pat, or '\0' */
			}
			(*pnum_terminals)++;
		}
		if (*pGParse & ENDSUB_EXP) current_attr = 0;	/* remains 0 until next fresh attribute */
		if (buffer_pat >= buffer_end) break;
		buffer = buffer_pat+1;
	}
	if (buffer_pat == NULL) return -1;	/* got out of while loop because of NULL rather than break */
	APattern[j] = '\0';

	if (foundattr || WHOLEFILESCOPE)	/* then search must always be OR since scope is over whole files */
		for (j=0; APattern[j] != '\0'; j++)
			if (APattern[j] == '\\') j++;
			else if (APattern[j] == ';') APattern[j] = ',';
	return(*pnum_terminals);
}

extern int is_complex_boolean();	/* use the one in agrep/asplit.c */
extern int get_token_bool();	/* use the one in agrep/asplit.c */

/* Spaces ARE significant: 'a1=v1' and 'a1=v1 ' and 'a1 =v1' etc. are NOT identical */
int
get_attribute_value(pattr, pval, tokenbuf, tokenlen, num_attr)
	int	*pattr, tokenlen;
	CHAR	**pval, *tokenbuf;
{
	CHAR	tempbuf[MAXNAME];
	int	i = 0, j = 0, k = 0, l = 0;

	while (i < tokenlen) {
		if (tokenbuf[i] == '\\') {
			tempbuf[j++] = tokenbuf[i++];
			tempbuf[j++] = tokenbuf[i++];
		}
		else if (StructuredIndex) {
			if (tokenbuf[i] == '=') {
				i++;	/* skip over = : now @ 1st char of value */
				tempbuf[j] = '\0';
				for (k=0; k<j; k++) {
					if (tempbuf[k] == '\\') {
						for (l=k; l<j; l++)
							tempbuf[l] = tempbuf[l+1];
						j --;
					}
				}
				if ( ((*pattr = attr_name_to_id(tempbuf, j)) <= 0) || (*pattr >= num_attr) ) {	/* named a non-existent attribute */
					fprintf(stderr, "%s: unknown attribute name '%s'\n", GProgname, tempbuf);
					return 0;
				}
				*pval = (CHAR *)malloc(tokenlen - i + 2);
				memcpy(*pval, &tokenbuf[i], tokenlen - i);
				(*pval)[tokenlen - i] = '\0';
				foundattr = 1;
				return 1;
			}
			else tempbuf[j++] = tokenbuf[i++];	/* consider = as just another char */
		}
		else tempbuf[j++] = tokenbuf[i++];	/* no attribute parsing */
	}
	/* Not a structured expression */
	tempbuf[j] = '\0';
	*pval = (CHAR *)malloc(j + 2);
	memcpy(*pval, tempbuf, j);
	(*pval)[j] = '\0';
	return 1;
}

extern destroy_tree();	/* use the one in agrep/asplit.c */

/*
 * Recursive descent; C-style => AND + OR have equal priority => must bracketize expressions appropriately or will go left->right.
 * Also strips out attribute names since agrep doesn't understand them: copies resulting pattern for agrep-ing into apattern.
 * Grammar:
 * 	E = {E} | ~a | ~{E} | E ; E | E , E | a
 * Parser:
 *	One look ahead at each literal will tell you what to do.
 *	~ has highest priority, ; and , have equal priority (left to right associativity), ~~ is not allowed.
 */
ParseTree *
parse_tree(buffer, len, bufptr, apattern, apatptr, terminals, pnum_terminals, num_attr)
	CHAR	*buffer;
	int	len;
	int	*bufptr;
	CHAR	*apattern;
	int	*apatptr;
	ParseTree terminals[];
	int	*pnum_terminals;
	int	num_attr;
{
	int	token, tokenlen;
	CHAR	tokenbuf[MAXNAME];
	int	oldtokenlen;
	CHAR	oldtokenbuf[MAXNAME];
	ParseTree *t, *n, *leftn;

	token = get_token_bool(buffer, len, bufptr, tokenbuf, &tokenlen);
	switch(token)
	{
	case	'{':	/* (exp) */
		apattern[(*apatptr)++] = '{';
		if ((t = parse_tree(buffer, len, bufptr, apattern, apatptr, terminals, pnum_terminals, num_attr)) == NULL) return NULL;
		if ((token = get_token_bool(buffer, len, bufptr, tokenbuf, &tokenlen)) != '}') {
			fprintf(stderr, "%s: parse error at offset %d\n", GProgname, *bufptr);
			destroy_tree(t);
			return (NULL);
		}
		apattern[(*apatptr)++] = '}';
		if ((token = get_token_bool(buffer, len, bufptr, tokenbuf, &tokenlen)) == 'e') return t;
		switch(token)
		{
		/* must find boolean infix operator */
		case ',':
		case ';':
			apattern[(*apatptr)++] = token;
			leftn = t;
			if ((t = parse_tree(buffer, len, bufptr, apattern, apatptr, terminals, pnum_terminals, num_attr)) == NULL) return NULL;
			n = (ParseTree *)malloc(sizeof(ParseTree));
			n->op = (token == ';') ? ANDPAT : ORPAT ;
			n->type = INTERNAL;
			n->data.internal.left = leftn;
			n->data.internal.right = t;
			return n;

		/* or end of parent sub expression */
		case '}':
			unget_token_bool(bufptr, tokenlen);	/* part of someone else who called me */
			return t;

		default:
			destroy_tree(t);
			fprintf(stderr, "%s: parse error at offset %d\n", GProgname, *bufptr);
			return NULL;
		}

	/* Go one level deeper */
	case	'~':	/* not exp */
		foundnot = 1;
		apattern[(*apatptr)++] = '~';
		if ((token = get_token_bool(buffer, len, bufptr, tokenbuf, &tokenlen)) == 'e') return NULL;
		switch(token)
		{
		case 'a':
			if (*pnum_terminals >= MAXNUM_PAT) {
				fprintf(stderr, "%s: pattern expression too long (> %d terms)\n", GProgname, MAXNUM_PAT);
				return NULL;
			}
			n = &terminals[*pnum_terminals];
			n->op = 0;
			n->type = LEAF;
			n->terminalindex = (*pnum_terminals);
			n->data.leaf.value = NULL;
			n->data.leaf.attribute = 0;
			if (!get_attribute_value((int *)&n->data.leaf.attribute, &n->data.leaf.value, tokenbuf, tokenlen, num_attr)) return NULL;
			strcpy(&apattern[*apatptr], n->data.leaf.value);
			*apatptr += strlen(n->data.leaf.value);
			(*pnum_terminals)++;
			n->op |= NOTPAT;
			t = n;
			break;

		case '{':
			apattern[(*apatptr)++] = token;
			if ((t = parse_tree(buffer, len, bufptr, apattern, apatptr, terminals, pnum_terminals, num_attr)) == NULL) return NULL;
			if (t->op & NOTPAT) t->op &= ~NOTPAT;
			else t->op |= NOTPAT;
			if ((token = get_token_bool(buffer, len, bufptr, tokenbuf, &tokenlen)) != '}') {
				fprintf(stderr, "%s: parse error at offset %d\n", GProgname, *bufptr);
				destroy_tree(t);
				return NULL;
			}
			apattern[(*apatptr)++] = '}';
			break;

		default:
			fprintf(stderr, "%s: parse error at offset %d\n", GProgname, *bufptr);
			return NULL;
		}
		/* The resulting tree is in t. Now do another lookahead at this level */

		if ((token = get_token_bool(buffer, len, bufptr, tokenbuf, &tokenlen)) == 'e') return t;
		switch(token)
		{
		/* must find boolean infix operator */
		case ',':
		case ';':
			apattern[(*apatptr)++] = token;
			leftn = t;
			if ((t = parse_tree(buffer, len, bufptr, apattern, apatptr, terminals, pnum_terminals, num_attr)) == NULL) return NULL;
			n = (ParseTree *)malloc(sizeof(ParseTree));
			n->op = (token == ';') ? ANDPAT : ORPAT ;
			n->type = INTERNAL;
			n->data.internal.left = leftn;
			n->data.internal.right = t;
			return n;

		case '}':
			unget_token_bool(bufptr, tokenlen);
			return t;

		default:
			destroy_tree(t);
			fprintf(stderr, "%s: parse error at offset %d\n", GProgname, *bufptr);
			return NULL;
		}

	case	'a':	/* individual term (attr=val) */
		if (tokenlen == 0) return NULL;
		memcpy(oldtokenbuf, tokenbuf, tokenlen);
		oldtokenlen = tokenlen;
		oldtokenbuf[oldtokenlen] = '\0';
		token = get_token_bool(buffer, len, bufptr, tokenbuf, &tokenlen);
		switch(token)
		{
		case '}':	/* part of case '{' above: else syntax error not detected but semantics ok */
			unget_token_bool(bufptr, tokenlen);
		case 'e':	/* endof input */
		case ',':
		case ';':
			if (*pnum_terminals >= MAXNUM_PAT) {
				fprintf(stderr, "%s: pattern expression too long (> %d terms)\n", GProgname, MAXNUM_PAT);
				return NULL;
			}
			n = &terminals[*pnum_terminals];
			n->op = 0;
			n->type = LEAF;
			n->terminalindex = (*pnum_terminals);
			n->data.leaf.value = NULL;
			n->data.leaf.attribute = 0;
			if (!get_attribute_value((int *)&n->data.leaf.attribute, &n->data.leaf.value, oldtokenbuf, oldtokenlen, num_attr)) return NULL;
			strcpy(&apattern[*apatptr], n->data.leaf.value);
			*apatptr += strlen(n->data.leaf.value);
			(*pnum_terminals)++;
			if ((token == 'e') || (token == '}')) return n;	/* nothing after terminal in expression */

			leftn = n;
			apattern[(*apatptr)++] = token;
			if ((t = parse_tree(buffer, len, bufptr, apattern, apatptr, terminals, pnum_terminals, num_attr)) == NULL) return NULL;
			n = (ParseTree *)malloc(sizeof(ParseTree));
			n->op = (token == ';') ? ANDPAT : ORPAT ;
			n->type = INTERNAL;
			n->data.internal.left = leftn;
			n->data.internal.right = t;
			return n;

		default:
			fprintf(stderr, "%s: parse error at offset %d\n", GProgname, *bufptr);
			return NULL;
		}

	case	'e':	/* can't happen as I always do a lookahead above and return current tree if e */
	default:
		fprintf(stderr, "%s: parse error at offset %d\n", GProgname, *bufptr);
		return NULL;
	}
}

int
split_pattern(GPattern, GM, APattern, terminals, pnum_terminals, pGParse, num_attr)
	CHAR	*GPattern;
	int	GM;
	CHAR	*APattern;
	ParseTree terminals[];
	int	*pnum_terminals;
	ParseTree **pGParse;
	int	num_attr;
{
	int	bufptr = 0, apatptr = 0, ret, i, j;

	foundattr = 0;
	if (is_complex_boolean(GPattern, GM)) {
		ComplexBoolean = 1;
		*pnum_terminals = 0;
		if ((*pGParse = parse_tree(GPattern, GM, &bufptr, APattern, &apatptr, terminals, pnum_terminals, num_attr)) == NULL)
			return -1;
		/* print_tree(*pGParse, 0); */
		APattern[apatptr] = '\0';
		if (foundattr || WHOLEFILESCOPE) {	/* Search in agrep must always be OR since scope is whole file */
			int	i, j;

			for (i=0; i<apatptr; i++) {
				if (APattern[i] == '\\') i++;
				else if (APattern[i] == ';') APattern[i] = ',';
				else if ((APattern[i] == '~') || (APattern[i] == '{') || (APattern[i] == '}')) {
					/* eliminate it from pattern by shifting (including '\0') since agrep must essentially do a flat search */
					if (APattern[i] == '~') foundnot = 1;
					for (j=i; j<apatptr; j++)
						APattern[j] = APattern[j+1];
					apatptr --;
					i--;	/* to counter the ++ on top */
				}
			}
		}
		return *pnum_terminals;
	}
	else {
		for (i=0; i<GM; i++) {
			if (GPattern[i] == '\\') i++;
			else if ((GPattern[i] == '{') || (GPattern[i] == '}')) {
				/* eliminate it from pattern by shifting (including '\0') since agrep must essentially do a flat search */
				for (j=i; j<GM; j++)
					GPattern[j] = GPattern[j+1];
				GM --;
				i--;	/* counter the ++ on top */
			}
		}

		ComplexBoolean = 0;
		*pnum_terminals = 0;
		if ((ret = split_pattern_flat(GPattern, GM, APattern, terminals, pnum_terminals, (int *)pGParse, num_attr)) == -1)
			return -1;
		return ret;
	}
}

int eval_tree();	/* use the one in agrep/asplit.c */

/* MUST CHANGE ALL memgreps TO USE EXEC DIRECTLY IF POSSIBLE (LAST PRIORITY) */

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
extern	struct offsets **src_offset_table;
extern	struct offsets **curr_offset_table;
extern	char	tempfile[];
extern	int	patindex;
extern	int	patbufpos;
extern	ParseTree terminals[MAXNUM_PAT];
extern	int	num_terminals;
extern int INVERSE;	/* agrep's global: need here to implement ~ in index-search */

/* [first, last) = C-style range for which we want the words in terminal-values' patterns: 0..num_terminals for !ComplexBoolean, term/term otherwise */
split_terminal(first, last)
	int	first, last;
{
        CHAR	*buffer;
        CHAR	*buffer_new;
        CHAR	*buffer_end;
        CHAR	*bp;
        CHAR	word[MAXNAME];
        int	word_length;
        int	type;
	int	i, j, k, attribute;
	char	*substring;	/* used with strstr(superstring, substring) */

	pat_ptr = 0;
	num_mgrep_pat = 0;
	num_pat = 0;

	for (; first<last; first++) {
		attribute = (int)terminals[first].data.leaf.attribute;
		buffer = terminals[first].data.leaf.value;
		buffer_end = buffer + strlen(terminals[first].data.leaf.value);

                /* Now find out which; are the "words" we can search for in the index: each attr_val can have many words in it: e.g., "AUTHOR=Udi Manber" */
                while ((buffer_new = getword("stdin", word, buffer, buffer_end, NULL, NULL)) <= buffer_end) {
                        word_length = strlen(word);
                        if (word_length <= 0) {
                                buffer = buffer_new;
                                if (buffer_new >= buffer_end) break;
                                continue;
                        }
                        if ((type = checksg(word, D, 0)) == -1) return -1;
			if (!type && ComplexBoolean) {
				fprintf(stderr, "%s: query has complex patterns (like '.*') or options (like -n)\n... cannot search for arbitrary booleans\n", GProgname);
				return -1;
			}

#if	0
DISABLED IN GLIMPSE NOW SINCE MGREP HANDLES DUPLICATES -- IT WAS ALWAYS ABLE TO HANDLE SUPERSTRINGS/SUBSTRINGS...: bgopal, Nov 19, 1996
			if (type) {
				/* Check if superstring: if so, ditch word */
				for (i=0; i<num_pat; i++) {
					if (!is_mgrep_pat[i]) continue;
					substring = strstr(word, pat_list[i]);
					if ((substring != NULL) && (substring[0] != '\0')) break;
				}
				if (i < num_pat) {	/* printf("%s superstring of %s\n", word, pat_list[i]); */
					if (pat_attr[i] != attribute) pat_attr[i] = 0;	/* union of two unequal attributes is all attributes */
					buffer = buffer_new;
					if (buffer_new >= buffer_end) break;
					continue;
				}
				/* Check if substring: delete all superstrings */
				for (i=0; i<num_pat; i++) {
					if (!is_mgrep_pat[i]) continue;
					substring = strstr(pat_list[i], word);
					if ((substring != NULL) && (substring[0] != '\0')) {	/* printf("%s substring of %s\n", word, pat_list[i]); */
						if (pat_attr[i] != attribute) attribute = 0;	/* union of two unequal attributes is all attributes */
						free(pat_list[i]);
						for (j=i; j<num_pat; j++) {
							pat_list[j] = pat_list[j+1];
							pat_lens[j] = pat_lens[j+1];
							is_mgrep_pat[j] = is_mgrep_pat[j+1];
							pat_attr[j] = pat_attr[j+1];
						}
						num_pat --;
						for (j=0; j<num_mgrep_pat; j++) {
							if (mgrep_pat_index[j] == i) {
								for (k=j; k<num_mgrep_pat; k++) {
									mgrep_pat_index[k] = mgrep_pat_index[k+1] - 1;
								}
								num_mgrep_pat --;
								break;
							}
						}
						i--;	/* to counter the ++ on top */
					}
				}
			}
#endif

                        buffer = buffer_new;
                        bp = (CHAR *) malloc(sizeof(CHAR) * word_length + 2);
                        if(bp == NULL) {
                                fprintf(stderr, "%s: malloc failed in %s:%d\n", GProgname, __FILE__, __LINE__);
                                return -1;
                        }
                        pat_list[num_pat] = bp;
                        pat_lens[num_pat] = word_length;
                        is_mgrep_pat[num_pat] = type;
                        pat_attr[num_pat] = attribute;
                        strcpy(pat_list[num_pat], word);
			pat_list[num_pat][word_length] = '\0';
			num_pat ++;
#if     BG_DEBUG
                        fprintf(debug, "word=%s\n", word);
#endif  /*BG_DEBUG*/
                        if(buffer_new >= buffer_end) break;
                        if(num_pat >= MAXNUM_PAT) {
                                fprintf(stderr, "%s: Warning! too many words in pattern (> %d): ignoring...\n", GProgname, MAXNUM_PAT);
                                break;
                        }
                }
	}
	for (i=0; i<num_pat; i++) {
		strcpy(&pat_buf[pat_ptr], pat_list[i]);
		pat_buf[pat_ptr + pat_lens[i]] = '\n';
		pat_buf[pat_ptr + pat_lens[i] + 1] = '\0';
		pat_ptr += (pat_lens[i] + 1);
	}
	return num_pat;
}
