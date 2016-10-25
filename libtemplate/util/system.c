static char rcsid[] = "$Id: system.c,v 1.2 2003/11/13 05:17:39 golda Exp $";
/*
 *  system.c - system(3) routines for Essence system.
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
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include "util.h"
#ifdef HAVE_SETRLIMIT
#include <sys/time.h>
#include <sys/resource.h>
#endif

static void redirect_stdout();

/*
 *  do_system() - calls system(3).
 */
int do_system(cmd)
char *cmd;
{
#ifdef DEBUG
	glimpselog("RUNNING as shell: %s\n", cmd);
#endif
	return (system(cmd));
}

/*
 *  run_cmd() - simplified system(3).  Parses the command, will redirect
 *  stdout and then fork/exec() to save a sh process.
 */
int run_cmd(cmd)
char *cmd;
{
	int pid, status = 0;

#ifdef DEBUG
	glimpselog("RUNNING: %s\n", cmd);
#endif

/*
   ** PURFIY:
   ** use fork() here instead of vfork().  With vfork parent and child
   ** share memory space.  In the child we strdup a bunch of argv's
   ** which would otherwise never get free'd causing a memory leak in
   ** the parent.
 */
	if ((pid = fork()) < 0) {
		log_errno("run_cmd: fork");
		return (1);
	}
	if (pid == 0) {		/* child */
		char *argv[64], buf[BUFSIZ];
		int i;

		memset(argv, '\0', sizeof(char *) * 64);
		parse_argv(argv, cmd);
		for (i = 0; argv[i] != NULL; i++) {
			if (argv[i][0] == '>' && argv[i + 1] != NULL) {
				argv[i] = NULL;
				redirect_stdout(argv[++i]);
				break;
			}
		}
		execvp(argv[0], argv);
		sprintf(buf, "execvp: %s", argv[0]);
		log_errno(buf);
		_exit(1);
	}
	/* parent */
	(void) waitpid(pid, &status, (int) NULL);
	return (WEXITSTATUS(status));
}

static void redirect_stdout(filename)
char *filename;
{
	int fd;

	if (filename == NULL)
		return;
	if ((fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) {
		log_errno(filename);
		return;
	}
	close(1);
	dup2(fd, 1);		/* make stdout */
}

static int dsl_pid = -1;
static void alarm_handler()
{
#ifdef DEBUG
	glimpselog("do_system_lifetime: Caught signal.  Killing %d.\n", dsl_pid);
#endif
	(void) kill(dsl_pid, SIGTERM);
	sleep(1);
	(void) kill(dsl_pid, SIGKILL);
}

/*
 *  do_system_lifetime() - calls system(3).  Only lives for lifetime seconds.
 */
void do_system_lifetime(cmd, lifetime)
char *cmd;
int lifetime;
{
#ifdef DEBUG
	glimpselog("RUNNING: %s\n", cmd);
	glimpselog("do_system_lifetime: Lifetime is %d seconds.\n", lifetime);
#endif
	signal(SIGALRM, alarm_handler);
	alarm(lifetime);
/*
   ** PURFIY:
   ** use fork() here instead of vfork().  With vfork parent and child
   ** share memory space.  In the child we strdup a bunch of argv's
   ** which would otherwise never get free'd causing a memory leak in
   ** the parent.
 */
	if ((dsl_pid = fork()) < 0) {
		log_errno("fork");
		return;
	}
	if (dsl_pid) {		/* parent */
		(void) waitpid(dsl_pid, (int *) NULL, (int) NULL);
		alarm(0);
		return;
	} else {		/* child */
		char *argv[64];	
		char buf[BUFSIZ], i;

		alarm(0);
		memset(argv, '\0', sizeof(char *) * 64);
		parse_argv(argv, cmd);
		for (i = 0; argv[i] != NULL; i++) {
			if (argv[i][0] == '>' && argv[i + 1] != NULL) {
				argv[i] = NULL;
				redirect_stdout(argv[++i]);
				break;
			}
		}
#if defined(HAVE_SETRLIMIT) && defined(RLIMIT_CPU)
		{
			struct rlimit rlp;
			rlp.rlim_cur = rlp.rlim_max = lifetime;
			(void) setrlimit(RLIMIT_CPU, &rlp);
		}
#endif
		execvp(argv[0], argv);
		sprintf(buf, "execvp: %s", argv[0]);
		log_errno(buf);
		_exit(1);
	}
}

/*
 *  close_all_fds() - closes all of the file descriptors starting with start.
 */
void close_all_fds(start)
int start;
{
	int i;

#if   defined(HAVE_GETDTABLESIZE)
	for (i = start; i < getdtablesize(); i++) {
#elif defined(HAVE_SYSCONF) && defined(_SC_OPEN_MAX)
	for (i = start; i < sysconf(_SC_OPEN_MAX); i++) {
#elif defined(OPEN_MAX)
	for (i = start; i < OPEN_MAX; i++) {
#else
	for (i = start; i < 64; i++) {
#endif
		(void) close(i);
	}
}
