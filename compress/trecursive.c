/* Copyright (c) 1994 Burra Gopal, Udi Manber.  All Rights Reserved. */

#include "autoconf.h"	/* ../libtemplate/include */
#include <stdio.h>
#include <sys/types.h>
#if	ISO_CHAR_SET
#include <locale.h>
#endif

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include <sys/stat.h>
#include <fcntl.h>
#define DIRSIZE 14

#ifndef S_ISREG
#define S_ISREG(mode) (0100000&(mode))
#endif

#ifndef S_ISDIR
#define S_ISDIR(mode) (0040000&(mode))
#endif

#if	0
#define FUNCTION(x, y, z)	treewalk(x, y, z)
#define MAX_LINE_LEN		1024

main(argc, argv)
int argc; char **argv;
{
	char buf[MAX_LINE_LEN];
	char outbuf[MAX_LINE_LEN];
	int flags=0;

#if	ISO_CHAR_SET
	setlocale(LC_ALL, "");
#endif
	if (argc == 1) {
		strcpy(buf, ".");
		return treewalk(buf, outbuf, flags);
	}
	else while(--argc > 0) {
		strcpy(buf, *++argv);
		return treewalk(buf, outbuf, flags);
	}
}

int
treewalk(name, outname, flags)
char *name;
char *outname;
int flags;
{
	struct stat stbuf;
	extern int puts();

	if(my_lstat(name, &stbuf) == -1) {
		fprintf(stderr, "permission denied or non-existent: %s\n", name);
		return -1;
	}
	if ((stbuf.st_mode & S_IFMT) == S_IFLNK)
		return -1;
	if ((stbuf.st_mode & S_IFMT) == S_IFDIR) 
		return DIRECTORY(name, outname, flags);
	if ((stbuf.st_mode & S_IFMT) == S_IFREG)
		return puts(name);
}
#endif	/*0*/

int
DIRECTORY(name, outname, flags)
char *name, *outname;
int flags;
{
	struct dirent *dp;
	char *nbp;
	DIR *dirp;

	nbp = name + strlen(name);
	if( nbp+DIRSIZE+2 >= name+MAX_LINE_LEN ) {	/* name too long */
		fprintf(stderr, "name too long: %.32s...\n", name);
		return -1;
	}
	if((dirp = opendir(name)) == NULL) {
		fprintf(stderr, "permission denied: %s\n", name);
		return -1;
	}
	*nbp++ = '/';
	for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
		if (dp->d_name[0] == '\0' || strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..")==0) {
			continue;
		}
		strcpy(nbp, dp->d_name);
		FUNCTION(name, outname, flags);
	}
	closedir (dirp);
	*--nbp = '\0'; /* restore name */
	return 0;
}
