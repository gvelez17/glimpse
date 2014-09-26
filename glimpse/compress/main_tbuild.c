/* Copyright (c) 1994 Burra Gopal, Udi Manber.  All Rights Reserved. */

/*
 * main_tbuild.c: calls tbuild.c/compute_dictionary() after reading options.
 */

#include "defs.h"
#if	ISO_CHAR_SET
#include <locale.h>
#endif

extern int compute_dictionary();
extern char **environ;

#include "dummysyscalls.c"

usage(progname)
char	*progname;
{
	fprintf(stderr, "usage: %s [-H directory] [-t threshold] [-l stop-list-size]\n", progname);
	fprintf(stderr, "defaults: %d %d %d ~\n", DEF_SPECIAL_WORDS, DEF_THRESHOLD, DEF_BLOCKSIZE);
	exit(1);
}

int
main(argc, argv)
	int	argc;
	unsigned char	*argv[];
{
	char	comp_dir[MAX_LINE_LEN];
	int	threshold, specialwords;
	int	i = 1;
	char	*home;

#if	ISO_CHAR_SET
	setlocale(LC_ALL, "");
#endif
	/* fill in default options */
	comp_dir[0] = '\0';
	threshold = DEF_THRESHOLD;
	specialwords = DEF_SPECIAL_WORDS;

	while(i < argc) {
		if (argv[i][0] != '-') return usage(argv[0]);
		else if (argv[i][1] == 'H') strcpy(comp_dir, argv[++i]);
		else if (argv[i][1] == 't') threshold = atoi(argv[++i]);
		else if (argv[i][1] == 'l') specialwords = atoi(argv[++i]);
		else if (argv[i][1] == 'V') {
			printf("\nThis is tbuild version %s. Copyright (c) %s, University of Arizona.\n\n", CAST_VERSION, CAST_DATE);
		}
		else return usage(argv[0]);
		i++;
	}
        if (comp_dir[0] == '\0') {
                if ((home = (char *)getenv("HOME")) == NULL) {
                        getcwd(comp_dir, MAX_LINE_LEN-1);
                        fprintf(stderr, "using working-directory '%s' to locate index\n", comp_dir);
                }
                else strncpy(comp_dir, home, MAX_LINE_LEN);
        }

	compute_dictionary(threshold, DISKBLOCKSIZE, specialwords, comp_dir);
        return 0;
}
