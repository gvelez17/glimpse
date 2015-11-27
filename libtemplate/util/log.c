static char rcsid[] = "$Id: log.c,v 1.5 2006/02/03 16:53:26 golda Exp $";
/*
 *  log.c - Logging facilities for Essence system.
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
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/file.h>
#include <stdarg.h>
#include "util.h"

/* Local functions */
static void standard_msg();
static void log_flush();

/* Local variables */
static FILE *fp_log = NULL;
static FILE *fp_errs = NULL;
static int pid;
static char *pname = NULL;

#if defined(USE_LOG_SYNC) && defined(HAVE_FLOCK)
static void lock_file(fp)
FILE *fp;
{
	if (flock(fileno(fp), LOCK_EX) < 0)
		log_errno("lockf");
	if (fseek(fp, 0, SEEK_END) < 0)
		log_errno("fseek");
}

static void unlock_file(fp)
FILE *fp;
{
	if (flock(fileno(fp), LOCK_UN) < 0)
		log_errno("lockf");
}
#else
#define lock_file(fp)		/* nops */
#define unlock_file(fp)
#endif

/*
 *  init_log() - Initializes the logging routines.  log() prints to 
 *  FILE *a, and errorlog() prints to FILE *b;
 */
void init_log(a, b)
FILE *a, *b;
{
	fp_log = a;
	fp_errs = b;
	pid = getpid();
	pname = NULL;
}

void init_log3(pn, a, b)
char *pn;
FILE *a, *b;
{
	fp_log = a;
	fp_errs = b;
	pid = getpid();
	pname = strdup(pn);
}

/*
 *  glimpselog() - used like printf(3).  Prints message to stdout.
 */

void glimpselog(char *fmt,...)
{
	va_list ap;

	if (fp_log == NULL)
		return;

	va_start(ap, fmt);

	if (fp_log == NULL)
		return;

	lock_file(fp_log);
	standard_msg(fp_log);
	vfprintf(fp_log, fmt, ap);
	va_end(ap);
	log_flush(fp_log);
	unlock_file(fp_log);
}

/*
 *  errorlog() - used like printf(3).  Prints error message to stderr.
 */
void errorlog(char *fmt,...)
{
	va_list ap;

	if (fp_errs == NULL)
		return;

	va_start(ap, fmt);

	if (fp_errs == NULL)
		return;

	lock_file(fp_errs);
	standard_msg(fp_errs);
	fprintf(fp_errs, "ERROR: ");
	vfprintf(fp_errs, fmt, ap);
	va_end(ap);
	log_flush(fp_errs);
	unlock_file(fp_errs);
}

/*
 *  fatal() - used like printf(3).  Prints error message to stderr and exits
 */
void fatal(char *fmt,...)
{
	va_list ap;

	if (fp_errs == NULL)
		exit(1);

	va_start(ap, fmt);

	if (fp_errs == NULL)
		exit(1);

	lock_file(fp_errs);
	standard_msg(fp_errs);
	fprintf(fp_errs, "FATAL: ");
	vfprintf(fp_errs, fmt, ap);
	va_end(ap);
	log_flush(fp_errs);
	unlock_file(fp_errs);
	exit(1);
}

/*
 *  log_errno() - Same as perror(); doesn't print when errno == 0
 */
void log_errno(s)
char *s;
{
	if (errno != 0)
		errorlog("%s: %s\n", s, strerror(errno));
}


/*
 *  fatal_errno() - Same as perror()
 */
void fatal_errno(s)
char *s;
{
	fatal("%s: %s\n", s, strerror(errno));
}

/*
 *  standard_msg() - Prints the standard pid and timestamp
 */
static void standard_msg(fp)
FILE *fp;
{
	if (pname != NULL)
		fprintf(fp, "%7s: ", pname);
	else
		fprintf(fp, "%7d: ", pid);
#ifdef LOG_TIMES
	{
		time_t t = time(NULL);
		char buf[BUFSIZ];

		strftime(buf, BUFSIZ - 1, "%y%m%d %H:%M:%S:", localtime(&t));
		fprintf(fp, "%s ", buf);
	}
#endif
}

static void log_flush(fp)
FILE *fp;
{
	fflush(fp);
}
