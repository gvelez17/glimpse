#include "glimpse.h"
int BigFilenameHashTable = OFF;

#define SIGNIFICANT_HASH_REGION	24

/* n is guaranteed to be < MaxNum4bPartition */

int
encode4b(n)
	int	n;
{
	if (n=='\0') return MaxNum4bPartition;
	if (n=='\n') return MaxNum4bPartition+1;
	return n;
}

int
decode4b(n)
	int n;
{
	if (n==MaxNum4bPartition) return '\0';
	if (n==MaxNum4bPartition+1) return '\n';
	return n;
}

/* n is guaranteed to be < MaxNum8bPartition */

int
encode8b(n)
	int n;
{
	if (n=='\0') return MaxNum8bPartition;
	if (n=='\n') return MaxNum8bPartition+1;
	return n;
}

int
decode8b(n)
	int n;
{
	if (n==MaxNum8bPartition) return '\0';
	if (n==MaxNum8bPartition+1) return '\n';
	return n;
}

/* n is guaranteed to be < MaxNum12bPartition */

int
encode12b(n)
	int n;
{
	unsigned char msb, lsb;

	msb = (n / MaxNum8bPartition);
	lsb = (n % MaxNum8bPartition);
	msb = encode4b(msb);
	lsb = encode8b(lsb);
	return (msb<<8)|lsb;
}

int
decode12b(n)
	int n;
{
	unsigned char msb, lsb;

	msb = ((n&0x00000f00) >> 8);
	lsb = (n&0x000000ff);
	msb = decode4b(msb);
	lsb = decode8b(lsb);
	return (msb * MaxNum8bPartition) + lsb;
}

/* n is guaranteed to be < MaxNum16bPartition */

int
encode16b(n)
	int n;
{
	unsigned char msb, lsb;

	msb = (n / MaxNum8bPartition);
	lsb = (n % MaxNum8bPartition);
	msb = encode8b(msb);
	lsb = encode8b(lsb);
	return (msb<<8)|lsb;
}

int
decode16b(n)
	int n;
{
	unsigned char msb, lsb;

	msb = ((n&0x0000ff00) >> 8);
	lsb = (n&0x000000ff);
	msb = decode8b(msb);
	lsb = decode8b(lsb);
	return (msb * MaxNum8bPartition) + lsb;
}

/* n is guaranteed to be < MaxNum24bPartition */

int
encode24b(n)
	int n;
{
	unsigned short msb, lsb;

	msb = (n / MaxNum16bPartition);
	lsb = (n % MaxNum16bPartition);
	msb = encode8b(msb);
	lsb = encode16b(lsb);
	return (msb<<16)|lsb;
}

int
decode24b(n)
	int n;
{
	unsigned short msb, lsb;

	msb = ((n&0x00ff0000) >> 16);
	lsb = (n&0x0000ffff);
	msb = decode8b(msb);
	lsb = decode16b(lsb);
	return (msb * MaxNum16bPartition) + lsb;
}

/* n is guaranteed to be < MaxNum32bPartition */

int
encode32b(n)
	int n;
{
	unsigned short msb, lsb;

	msb = (n / MaxNum16bPartition);
	lsb = (n % MaxNum16bPartition);
	msb = encode16b(msb);
	lsb = encode16b(lsb);
	return (msb<<16)|lsb;
}

int
decode32b(n)
	int n;
{
	unsigned short msb, lsb;

	msb = ((n&0xffff0000) >> 16);
	lsb = (n&0x0000ffff);
	msb = decode16b(msb);
	lsb = decode16b(lsb);
	return (msb * MaxNum16bPartition) + lsb;
}

/*
 * converts file-names with *,. and ? and converts it to # \. and ? ALL OTHER agrep-special characters are masked off.
 * if the filename NOT a regular expression involving ? or *, it leaves the name untouched and returns the string
 * length of the file name (so that we can avoid memagrep calls): otherwise, it returns the -ve strlength of the name
 * after performing the above conversion: hence we never need to call agrep if the length is +ve.
 */
int
convert2agrepregexp(buf, len)
	char	*buf;
	int	len;
{
	char	tbuf[MAX_PAT+2];
	int	i=0, j=0;

	/* Ignore '*' at the beginning and '*' at the end */
	if (len < 1) return 0;
	if ( ((len == 1) && (buf[len-1] == '*')) || ((len >= 2) && (buf[len-1] == '*') && (buf[len-1] != '\\')) ) {
		buf[len-1] = '\0';
		len--;
	}
	if (buf[0] == '*') {
		for (i=0; i<len; i++)
			buf[i] = buf[i+1];
		len--;
	}
	if (len < 1) {
		buf[0] = '.';
		buf[1] = '*';
		buf[2] = '\0';
		return -2;
	}

	for (i=0; i<len; i++)
		if (buf[i] == '\\') i++;
		else if ((buf[i] == '?') || (buf[i] == '*')
			|| (buf[i] == '$') || (buf[i] == '^')) break;
	if (i >= len) return len;

	i = j = 0;
	while ((i<len) && (j<MAX_PAT) && (buf[i] != '\0')) {
		/* Consider all special characters interpreted by agrep */
		if (buf[i] == '\\') {
			/* copy two things without interpreting them */
			tbuf[j++] = buf[i++];
			tbuf[j++] = buf[i++];
		}
		else if ((buf[i] == '-') || (buf[i] == ',') || (buf[i] == ';')||
			 (buf[i] == '.') || (buf[i] == '#') || (buf[i] == '|')||
			 (buf[i] == '[') || (buf[i] == ']') || (buf[i] == '(')||
			 (buf[i] == ')') || (buf[i] == '>') || (buf[i] == '<')||
			 /* (buf[i] == '^') || (buf[i] == '$') || */
			 (buf[i] == '+')||
			 (buf[i] == '{') || (buf[i] == '}') || (buf[i] == '~')){
			tbuf[j++] = '\\';
			tbuf[j++] = buf[i];
			i++;
		}
		/* Interpret ONLY ? and * in file-names */
		else if (buf[i] == '?') {
			tbuf[j++] = '.';
			i++;
		}
		else if (buf[i] == '*') {
			tbuf[j++] = '.';
			tbuf[j++] = '*';
			i++;
		}
		else tbuf[j++] = buf[i++];
	}

	if (j >= MAX_PAT) {
		tbuf[MAX_PAT-1] = '\0';
		fprintf(stderr, "glimpseindex: pattern '%s' too long\n", buf);
		j = MAX_PAT - 1;	
	}
	else {
		tbuf[j] = '\0';
	}

	strcpy(buf, tbuf);
#if	0
	printf("%s=%d\n", buf, j);
#endif	/*0*/
	return -j;	/* strlen-compatible, -ve to indicate memagrep must be called */
}

/* -----------------------------------------------------------------
input: a word (a string of ascii character terminated by NULL)
output: a hash_value of the input word.
hash function: if the word has length <= 4
        the hash value is just a concatenation of the last four bits
        of the characters.
        if the word has length > 4, then after the above operation,
        the hash value is updated by adding each remaining character.
        (and AND with the 16-bits mask).
bug-fixes in all hashing functions: Chris Dalton
---------------------------------------------------------------- */
int
hash64k(word, len)
char *word;
int len;
{
    unsigned int hash_value=0;
    unsigned int mask_4=017;
    unsigned int mask_16=0177777;
    int i;

    if(len<=4) {
	for(i=0; i<len; i++) {
       	    hash_value = (hash_value << 4) | (word[i]&mask_4);
	    /* hash_value = hash_value  & mask_16; */ 
	}
    }
    else {
	for(i=0; i<4; i++) {
       	    hash_value = (hash_value << 4) | (word[i]&mask_4);
	    /* hash_value = hash_value & mask_16;  */
	}
	for(i=4; i<len; i++) 
	    hash_value = mask_16 & (hash_value + word[i]);
    }
    return(hash_value & mask_16);
}

/*
 * Explicitly used with -B option
 */
int
hash256k(word, len)
char *word;
int len;
{
    unsigned int hash_value=0;
    unsigned int mask_4=017;
    unsigned int mask_5=037;
    unsigned int mask_18=0x3ffff;
    int i;

    if(len<=4) {
	for(i=0; i<len; i++) {
       	    if ((i % 2) == 0) hash_value = (hash_value << 5) | (word[i]&mask_5);
	    else hash_value = (hash_value << 4) | (word[i]&mask_4);
	    /* hash_value = hash_value  & mask_18; */ 
	}
    }
    else {
	for(i=0; i<4; i++) {
       	    if ((i % 2) == 0) hash_value = (hash_value << 5) | (word[i]&mask_5);
       	    else hash_value = (hash_value << 4) | (word[i]&mask_4);
	    /* hash_value = hash_value & mask_18;  */
	}
	for(i=4; i<len; i++) 
	    hash_value = mask_18 & (hash_value + word[i]);
    }
    return(hash_value & mask_18);
}

/*
 * Explicitly used for veryfastsearch without WORD_SORTED
 * Using > 5 bits is waste since there are only 26 lower case letters
 */
int
hash32k(word, len)
	char 	*word;
	int	len;
{
    unsigned int hash_value=0;
    unsigned int mask_5=037;
    unsigned int mask_15=077777;
    int i;

    if(len<=3) {
	for(i=0; i<len; i++) {
       	    hash_value = (hash_value << 5) | (word[i]&mask_5);
	}
    }
    else {
	for(i=0; i<3; i++) {
       	    hash_value = (hash_value << 5) | (word[i]&mask_5);
	}
	for(i=3; i<len; i++) 
	    hash_value = mask_15 & (hash_value + word[i]);
    }
    return(hash_value & mask_15);
}

/* This function is utterly disgraceful */
int
hash16k(word, len)
	char	*word;
	int	len;
{
	return hash32k(word, len) & 0x3fff;
}

/*
 * Explicitly used for -f and -a options: has low collisions (<=2) for filenames
 */
int
hash4k(word, len)
	char 	*word;
	int	len;
{
    unsigned int hash_value=0;
    unsigned int mask_3=07;
    unsigned int mask_12=07777;
    int i;

    if(len<=4) {
	for(i=0; i<len; i++) {
       	    hash_value = (hash_value << 3) | (word[i]&mask_3);
	}
    }
    else {
	for(i=0; i<4; i++) {
       	    hash_value = (hash_value << 3) | (word[i]&mask_3);
	}
	for(i=4; i<len; i++) 
	    hash_value = mask_12 & (hash_value + word[i]);
    }
    return(hash_value & mask_12);
}

/* These 2 are especially useful for filenames that have long similar-looking pathname-prefixes */
int
hash64k_file(file, len)
char *file;
int len;
{
    unsigned int hash_value=0;
    unsigned int mask_4=017;
    unsigned int mask_16=0177777;
    int i;

    if(len < SIGNIFICANT_HASH_REGION + 2) return hash64k(file, len);
#if	1
    else {
	for(i=len-3; i>=len-6; i--) {
       	    hash_value = (hash_value << 4) | (file[i]&mask_4);
	    /* hash_value = hash_value & mask_16;  */
	}
	for(i=len-7; i>=len-SIGNIFICANT_HASH_REGION-2; i--) 
	    hash_value = mask_16 & (hash_value + file[i]);
	return(hash_value & mask_16);
    }
#else
    else {
	for(i=len-SIGNIFICANT_HASH_REGION-2; i<len-SIGNIFICANT_HASH_REGION+2; i++) {
       	    hash_value = (hash_value << 4) | (file[i]&mask_4);
	    /* hash_value = hash_value & mask_16;  */
	}
	for(i=len-SIGNIFICANT_HASH_REGION+2; i<len-2; i++) 
	    hash_value = mask_16 & (hash_value + file[i]);
	return(hash_value & mask_16);
    }
#endif
}

int
hash4k_file(file, len)
	char 	*file;
	int	len;
{
    unsigned int hash_value=0;
    unsigned int mask_3=07;
    unsigned int mask_12=07777;
    int i;

    if (len < SIGNIFICANT_HASH_REGION + 2) return hash4k(file, len);
#if	1
    else {
	for(i=len-3; i>=len-6; i--) {
       	    hash_value = (hash_value << 3) | (file[i]&mask_3);
	    /* hash_value = hash_value & mask_16;  */
	}
	for(i=len-7; i>=len-SIGNIFICANT_HASH_REGION-2; i--) 
	    hash_value = mask_12 & (hash_value + file[i]);
	return(hash_value & mask_12);
    }
#else
    else {
	for(i=len-SIGNIFICANT_HASH_REGION-2; i<len-SIGNIFICANT_HASH_REGION+2; i++) {
       	    hash_value = (hash_value << 3) | (file[i]&mask_3);
	}
	for(i=len-SIGNIFICANT_HASH_REGION+2; i<len-2; i++) 
	    hash_value = mask_12 & (hash_value + file[i]);
	return(hash_value & mask_12);
    }
#endif
}

hashNk(name, len)
	char	*name;
	int	len;
{
	if (BigFilenameHashTable) return hash64k_file(name, len);
	else return hash4k_file(name, len);
}
