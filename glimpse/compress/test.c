/* Copyright (c) 1994 Burra Gopal, Udi Manber.  All Rights Reserved. */
#include <stdio.h>
#include <string.h>

#include <autoconf.h>	/* configured defines */

#if	ISO_CHAR_SET
#include <locale.h>
#endif

char	src[256] = "    industrial production because of energy and input shortages and labor\n";

char	dest[256];
char	srcsrc[256];

#include "dummysyscalls.c"

main()
{
	int	i;
	int	srclen = strlen(src);
	int	destlen;
	int	srcsrclen;

#if	ISO_CHAR_SET
	setlocale(LC_ALL, "");
#endif
	printf("going...\n");
	destlen = quick_tcompress(".quick_lookup", ".compress_dictionary", src, srclen, dest, get_endian());
	dest[63] = 0;
	printf("len=%d\n", destlen);
	for (i=0; i<destlen; i++) printf("%x ", *((unsigned char *)&dest[i]) );
	srcsrclen = quick_tuncompress(".quick_lookup", ".uncompress_dictionary", dest, destlen, srcsrc, get_endian());
	printf("len=%d\n", srcsrclen);
	for (i=0; i<srcsrclen; i++) printf("%c", srcsrc[i]);
	printf("\n\n");
	for (i=0; i<srcsrclen; i++) printf("%x ", srcsrc[i]);
}
