/* Copyright (c) 1994 Sun Wu, Udi Manber, Burra Gopal.  All Rights Reserved. */
/* ./glimpse/index/newmemlook.c */

/* by Christian Vogler, Jan 9, 2000 */

/* I completely rewrote memlook, because in its original form the function
   has too many problems. It is prone to several buffer overruns. In addition,
   it puts a sentinel value past the end of the buffer, which may corrupt
   other data structures.

   The purpose of this function is to match a string in an area of text.
   If the string starts with a newline, it is matched as well, *except*
   at the very beginning of the text area. In this latter case, the newline
   is ignored.

   Returns the index of the first character of the matching string in
   the text area (excluding beginning newline), or -2 if it cannot be found.  
*/

#include <autoconf.h>
#include <string.h>

int memlook(const unsigned char *pattern, const unsigned char *text,
	    int length)
{
    int pattern_length;
    const unsigned char *text_end = text + length;
    const unsigned char *pat_compare = pattern; /* next pattern character to be
						   compared */
    const unsigned char *text_ptr = text;  /* next text location to be searched
					      for pattern */
    const unsigned char *text_compare; /* next text character to be compared 
				          in a pattern comparision */
    int found = 0;

    pattern_length = strlen((char *) pattern);

    /* First check if pattern starts with a newline. If yes, ignore
       newline and try to match rest of pattern at beginning of text. */
    if (pattern[0] == '\n')
	pat_compare++;

    while (text_ptr + pattern_length <= text_end) {
	text_compare = text_ptr;
	while (pat_compare < pattern + pattern_length) {
	    if (*pat_compare != *text_compare)
		break;
	    pat_compare++;
	    text_compare++;
	}

	found = (pat_compare == pattern + pattern_length);
	if (found)
	    break;                           /* pattern has been found */
	
	/* not found? then reset pattern comparison character pointer
	   and try next text character */
	pat_compare = pattern;
	text_ptr++;
    }

    if (found) {
	/* ignore beginning newline in pattern when returning position */
	if (pattern[0] 
	    == '\n' && *text_ptr == '\n')
	    text_ptr++;
	return text_ptr - text;
    }
    else
	return (-2);
}

