/*
 *  config.h - Master configuration file for the Harvest system.
 * 
 *  Darren Hardy, hardy@cs.colorado.edu, July 1994
 *
 *  $Id: config.h,v 1.1 1999/11/03 21:40:57 golda Exp $
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
#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "autoconf.h"	/* For GNU autoconf variables */
#include "paths.h"	/* For GNU autoconf program/subst variables */

/*
 *  USE_TMPDIR - default temporary directory into which files are extracted.
 */
#ifndef USE_TMPDIR
#define USE_TMPDIR	"/tmp"
#endif

/*
 *  GENERATE_KEYWORDS - Gatherer will automatically generate a case
 *  insenstive, unique, sorted keyword list for each content summary.
 */
#ifndef GENERATE_KEYWORDS
#define GENERATE_KEYWORDS
#endif

/*
 *  USE_LOCAL_CACHE - define if you want to use the Gatherer's local disk cache 
 */
#ifndef USE_LOCAL_CACHE
#define USE_LOCAL_CACHE
#endif

/*
 *  CACHE_TTL - # of seconds after which local disk cache files are invalid
 */
#ifndef CACHE_TTL
#define CACHE_TTL               (1 * 7 * 24 * 60 * 60)  /* 1 week */
#endif

/*
 *  USE_CCACHE - define if you want to the use FTP connection cache for liburl
 */
#ifndef USE_CCACHE
#undef USE_CCACHE
#endif


/****************************************************************************
 *--------------------------------------------------------------------------*
 * DO *NOT* MAKE ANY CHANGES below here unless you know what you're doing...*
 *--------------------------------------------------------------------------*
 ****************************************************************************/

/* 
 *  NO_STRDUP - define if standard C library doesn't have strdup(3).
 */
#ifndef NO_STRDUP
#ifndef HAVE_STRDUP
#define NO_STRDUP
#endif
#endif

/* 
 *  NO_STRERROR - define if standard C library doesn't have strerror(3).
 */
#ifndef NO_STRERROR
#ifndef HAVE_STRERROR
#define NO_STRERROR
#endif
#endif

/*
 *  MAX_TYPES is the max # of types that the type recognition supports.
 */
#ifndef MAX_TYPES
#define MAX_TYPES	512
#endif

/*
 *  CMD_TAR - command for tar
 */
#ifndef CMD_TAR
#define CMD_TAR		"tar"
#endif

/*
 *  USE_BYNAME - name of the configuration file for by name type recog.
 */
#ifndef USE_BYNAME
#define USE_BYNAME	"byname.cf"
#endif 

/*
 *  USE_BYCONTENET - name of the configuration file for file content type recog.
 */
#ifndef USE_BYCONTENT
#define USE_BYCONTENT	"bycontent.cf"
#endif

/*
 *  USE_BYURL - name of the configuration file for by URL type recog.
 */
#ifndef USE_BYURL
#define USE_BYURL	"byurl.cf"
#endif 

/*
 *  USE_MAGIC - default name and location of the magic file.
 */
#ifndef USE_MAGIC
#define USE_MAGIC	"magic"
#endif

/*
 *  USE_STOPLIST - name of the stoplist configuration file
 */
#ifndef USE_STOPLIST
#define USE_STOPLIST 	"stoplist.cf"
#endif

/*
 *  USE_MD5 - generates MD5 (cryptographic checksums) for each retrieved file.
 */
#ifndef USE_MD5
#define USE_MD5
#endif

/*
 *  GDBM_GROWTH_BUG - reorganizes db after to many replaces
 */
#ifndef GDBM_GROWTH_BUG
#undef GDBM_GROWTH_BUG
#endif


/*
 *  REAL_FILE_URLS - causes the Gatherer to interpret 'file' URLs as
 *  specified by Mosaic.  If the hostname field is the same as the
 *  current host, then the URL is treated as a local file, otherwise,
 *  the 'file' URL is treated as an 'ftp' URL.
 */
#ifndef REAL_FILE_URLS
#undef REAL_FILE_URLS
#endif

/*
 *  TRANSLATE_LOCAL_URLS - causes the Gatherer to intercept certain
 *  URLs and retrieve them through the local file system interface.
 */
#ifndef TRANSLATE_LOCAL_URLS
#define TRANSLATE_LOCAL_URLS
#endif

/*
 *  LOG_TIMES - each log message is prepended with the current time.
 */
#ifndef LOG_TIMES
#define LOG_TIMES
#endif

/*
 *  USE_LOG_SYNC - tries to synchonize multiple processes writing to a log file
 */
#ifndef USE_LOG_SYNC
#define USE_LOG_SYNC
#endif

/*
 *  XFER_TIMEOUT is the number of seconds that liburl will wait on a read()
 *  before giving up.
 */
#ifndef XFER_TIMEOUT
#define XFER_TIMEOUT	120
#endif

/*
 *  USE_CONFIRM_HOST - url_open() will check with DNS (or whatever) 
 *  to confirm that the hostname in the URL is valid if this is defined.
 */
#ifndef USE_CONFIRM_HOST
#undef USE_CONFIRM_HOST
#endif 

/*
 *  USE_PCINDEX - defines .unnest types for the PC software Gatherer
 */
#ifndef USE_PCINDEX
#define USE_PCINDEX
#endif

/*
 *  Define _HARVEST_AIX_ for the RS/6000 AIX port.
 */
#ifndef _HARVEST_AIX_
#undef _HARVEST_AIX_
#endif

#if defined(USE_POSIX_REGEX) || defined(USE_GNU_REGEX)
#include <regex.h>
#elif defined(USE_BSD_REGEX)
extern int re_comp(), re_exec();
#endif

#ifdef USE_POSIX_REGEX
#ifndef USE_RE_SYNTAX
#define USE_RE_SYNTAX	REG_EXTENDED	/* default Syntax */
#endif
#endif
	/* internal quicksum needs good regex support */
#ifdef USE_POSIX_REGEX
#ifndef USE_QUICKSUM
#define USE_QUICKSUM
#endif
#ifndef USE_QUICKSUM_FILE
#define USE_QUICKSUM_FILE	"quick-sum.cf"
#endif
#endif

#ifdef MEMORY_LEAKS
#ifndef NO_STRDUP
#define NO_STRDUP	/* use our version of strdup() */
#endif
#endif

#ifndef BLKDEV_IOSIZE
#include <sys/param.h>		/* try to find it... */
#endif
#ifdef BLKDEV_IOSIZE
#define MIN_XFER BLKDEV_IOSIZE	/* minimum number of bytes per disk xfer */
#else
#define MIN_XFER 512		/* make reasonable guess */
#endif

#ifndef BUFSIZ
#include <stdio.h>		/* try to find it... */
#ifndef BUFSIZ
#define BUFSIZ  4096		/* make reasonable guess */
#endif
#endif

#if defined(SYSTYPE_SYSV) || defined(__svr4__)  /* System V system */
#define _HARVEST_SYSV_
#elif defined(__hpux)                           /* HP-UX - SysV-like? */
#define _HARVEST_HPUX_
#elif defined(__osf__)                          /* OSF/1 */
#define _HARVEST_OSF_
#elif defined(__linux__)                        /* Linux */
#define _HARVEST_LINUX_
#elif defined(_SYSTYPE_SYSV) || defined(__SYSTYPE_SYSV) /* other SysV */
#define _HARVEST_SYSV_
#elif defined(_SYSTYPE_SVR4) || defined(SYSTYPE_SVR4) /* other Sys4 */
#define _HARVEST_SYSV_				/* fake as sysv */
#else
#define _HARVEST_BSD_
#endif


#endif /* _CONFIG_H_ */

