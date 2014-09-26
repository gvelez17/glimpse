#ifndef _GIMPSE_DEFS_H_
#define _GIMPSE_DEFS_H_

#include <autoconf.h>		/* autoconf defines */

#define MAX_ARGS	80	/* English alphabets + numbers + pattern + progname + arguments + extras */
#define MAXFILEOPT	1024	/* includes length of args too: #args is <= MAX_ARGS */
#define BLOCKSIZE	8192	/* For compression: what is the optimal unit of disk i/o = n * pagesize */

/*
 * These are some parameters that allow us to switch between offset computation
 * and just index computation when the index is built at a byte-level: since
 * offset computation is a waste if we can't narrow down search enough (since
 * we must look all over and the lists become too long => bottleneck). This may
 * not be needed if we used trees to store intervals --- we'll do it later :-).
 */

#define MAX_DISPARITY	100	/* if least frequent word occurrs in < 1/100 times most frequent word, resort to agrep: don't intersect lists (byte-level) */
#define MIN_OCCURRENCES	20	/* Min no. of occurrences before we check for highly frequent words using MAX_UNION */
#define MAX_UNION	500	/* Don't even perform the Union of offsets if least < 1/500 times most freq word (we are on track of stop list kinda words) */
#define MAX_ABSOLUTE	MAX_SORTLINE_LEN	/* Don't even perform the Union of offsets if a word occurs more than 16K times (independent of #of files) */
#endif
