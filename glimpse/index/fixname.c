#include <stdio.h>

main()
{
	int	offset = 0;
	char	buffer[1024];

	fgets(buffer, 1024, stdin);	/* skip over num. of file names */
	offset += strlen(buffer);
	while (fgets(buffer, 1024, stdin) != NULL) {
		putc((offset & 0xff000000) >> 24, stdout);
		putc((offset & 0xff0000) >> 16, stdout);
		putc((offset & 0xff00) >> 8, stdout);
		putc((offset & 0xff), stdout);
		offset += strlen(buffer);
	}
}
