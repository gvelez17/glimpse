/* Copyright (c) 1994 Burra Gopal, Udi Manber.  All Rights Reserved. */

/*
 * main_uncast.c: uses functions in hash.c and uncast.c to implement "uncast".
 */

#include "defs.h"
#if	ISO_CHAR_SET
#include <locale.h>
#endif

#include "dummysyscalls.c"

extern char **environ;
usage(progname)
char	*progname;
{
	fprintf(stderr, "\nThis is uncast version %s. Copyright (c) %s, University of Arizona.\n\n", GLIMPSE_VERSION, GLIMPSE_DATE);
	fprintf(stderr, "usage: %s [-help] [-F] [-H dir] [-V] [-d] [-o] [-r] [-s] [-y] sourcefiles\n", progname);
	fprintf(stderr, "summary of options (for a more detailed version, see 'man cast'):\n");
	fprintf(stderr, "-help: output this menu\n");
	fprintf(stderr, "-d: DO NOT remove source files after uncompress\n");
	/* fprintf(stderr, "-e: is used with files NOT compressed for easy search\n"); */
	fprintf(stderr, "-o: DO NOT overwrite the existing clear file (if any)\n");
	fprintf(stderr, "-r: uncompress recursively\n");
	fprintf(stderr, "-s: proceed silently and output the names of the uncompressible files\n");
	fprintf(stderr, "-y: DO NOT prompt and always overwrite when used without -o\n");
	fprintf(stderr, "-F: expect NAMES of files on stdin rather than data if there are no sourcefiles\n");
	fprintf(stderr, "-H dir: the directory for dictionaries is 'dir' (default ~)\n");
	fprintf(stderr, "\n");
	exit(1);
}

main(argc, argv)
	int	argc;
	char	*argv[];
{
	char	**filev;
	int	filec;
	int	i;	/* counter on argc, and later, filec */
	char	freq_file[MAX_LINE_LEN];
	char	string_file[MAX_LINE_LEN];
	char	comp_dir[MAX_LINE_LEN];
	char	name[MAX_LINE_LEN];
	char	outname[MAX_LINE_LEN];
	char	*home;
	int	FLAGS;
	int	num_read;
	char	buffer[SIGNATURE_LEN];

#if	ISO_CHAR_SET
	setlocale(LC_ALL, "");
#endif
	filev = (char **)malloc(sizeof(char *) * argc);
	memset(filev, '\0', sizeof(char *)*argc);
	filec = 0;
	comp_dir[0] = '\0';
	freq_file[0] = '\0';
	string_file[0] = '\0';
	FLAGS = 0;
	FLAGS |= TC_ERRORMSGS | TC_REMOVE | TC_OVERWRITE | TC_EASYSEARCH;

	/* Look at options */
	i=1;
	while (i < argc) {
		if (filec == 0 && !strcmp(argv[i], "-d")) {
			FLAGS &= ~TC_REMOVE;
			i ++;
		}
		else if (filec == 0 && !strcmp(argv[i], "-V")) {
			printf("\nThis is uncast version %s, %s.\n\n", CAST_VERSION, CAST_DATE);
			return 0;
		}
		/*
		else if (filec == 0 && !strcmp(argv[i], "-e")) {
			FLAGS &= ~TC_EASYSEARCH;
			i ++;
		}
		*/
		if (filec == 0 && !strcmp(argv[i], "-help")) {
			return usage(argv[0]);
		}
		else if (filec == 0 && !strcmp(argv[i], "-o")) {
			FLAGS &= ~TC_OVERWRITE;
			i ++;
		}
		else if (filec == 0 && !strcmp(argv[i], "-r")) {
			FLAGS |= TC_RECURSIVE;
			i ++;
		}
		else if (filec == 0 && !strcmp(argv[i], "-s")) {
			FLAGS |= TC_SILENT;
			i ++;
		}
		else if (filec == 0 && !strcmp(argv[i], "-y")) {
			FLAGS |= TC_NOPROMPT;
			i ++;
		}
		else if(filec == 0 && !strcmp(argv[1], "-F")) {
			FLAGS |= TC_FILENAMESONSTDIN;
			i ++;
		}
		else if (filec == 0 && !strcmp(argv[i], "-H")) {
			if (i + 1 >= argc) {
				fprintf(stderr, "directory not specified after -H\n");
				return usage(argv[0]);
			}
			strcpy(comp_dir, argv[i+1]);
			i+=2;
		}
		else {
			filev[filec++] = argv[i++];
		}
	}

	if (comp_dir[0] == '\0') {
		if ((home = (char *)getenv("HOME")) == NULL) {
			getcwd(comp_dir, MAXNAME-1);
			fprintf(stderr, "using working-directory '%s' to locate index\n", comp_dir);
		}
		else strncpy(comp_dir, home, MAXNAME);
	}
	strcpy(string_file, comp_dir);
	strcat(string_file, "/");
	strcat(string_file, DEF_STRING_FILE);
	strcpy(freq_file, comp_dir);
	strcat(freq_file, "/");
	strcat(freq_file, DEF_FREQ_FILE);
	if (!initialize_tuncompress(string_file, freq_file, FLAGS)) exit(2);

	if (FLAGS & TC_SILENT) FLAGS &= ~TC_ERRORMSGS;

	/* now compress each file in filev array: if no files specified, compress stdin and put it on stdout */
	if (filec == 0) {
		if (FLAGS & TC_FILENAMESONSTDIN) {
			while (fgets(name, MAX_LINE_LEN, stdin) == name) {
				tuncompress_file(name, outname, FLAGS);
			}
		}
		else {
			num_read = fread(buffer, 1, SIGNATURE_LEN - 1, stdin);
			if (!tuncompressible(buffer, num_read)) {
				fprintf(stderr, "signature does not match -- cannot uncompress %s\n", filev[i]);
				return -1;
			}
			tuncompress(stdin, -1, stdout, -1, FLAGS);
		}
	}
	else for (i=0; i<filec; i++) {
		strcpy(name, filev[i]);
		tuncompress_file(name, outname, FLAGS);
	}
	return 0;
}

