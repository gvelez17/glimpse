/* rewritten so that it uses no library routines */

#include <autoconf.h>	/* HAVE_SYS_SELECT_H is defined here */

#if	SFS_COMPAT
#if defined(__NeXT__)
#include <syscall.h>
#else
#include <sys/syscall.h>
#endif
#endif
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
/* #include <sys/uio.h> */
/* #include <sgtty.h> */
#include <signal.h>
#if	1
#if defined(_IBMR2)
#include <sys/select.h>
#endif
#else
#if defined(HAVE_SYS_SELECT_H)
#include <sys/select.h>
#endif
#endif

#include "glimpse.h"
#include "defs.h"

int
mystrlen(str, max)
	char	*str;
	int	max;
{
	int	i=0;

	while ((i<max) && (str[i] != '\0')) i++;
	return i;
}

int
readn(fd, ptr, nbytes)
int	fd;
char	*ptr;
int	nbytes;
{
	int	nleft, nread;

	nleft = nbytes;
	while (nleft > 0) {
#if	SFS_COMPAT
		nread = syscall(SYS_read, fd, ptr, nleft);
#else
		nread = read(fd, ptr, nleft);
#endif
		if (nread < 0) return(nread);
		else if (nread == 0) break;	/* EOF */
		nleft -= nread;
		ptr += nread;
	}
	return (nbytes - nleft);
}

int
writen(fd, ptr, nbytes)
int	fd;
char	*ptr;
int	nbytes;
{
	int	nleft, nwritten;

	nleft = nbytes;
	while (nleft > 0) {
#if	SFS_COMPAT
		nwritten = syscall(SYS_write, fd, ptr, nleft);
#else
		nwritten = write(fd, ptr, nleft);
#endif
		if (nwritten <= 0) return nwritten;
		nleft -= nwritten;
		ptr += nwritten;
	}
	return (nbytes - nleft);
}

int
readline(sockfd, ptr, maxlen)
int	sockfd;
char	*ptr;
int	maxlen;
{
	int	n, rc;
	char	c;

	for (n=1; n<maxlen; n++) {
		if ((rc = readn(sockfd, &c, 1)) == 1) {
			*ptr++ = c;
			if (c == '\n') break;
		} else if (rc == 0) {
			if (n==1) return (0);	/* EOF */
			else break;
		} else return (-1);
	}
	*ptr = 0;
	return n;
}

#if	USE_MSGHDR
/*
 * This piece of code was causing compilation problems.
 * It was not being used anyway. So it has been deleted.
 * -bg, Jan 4th 95
 */

int
sendfile(sockfd, fds, num)
int	sockfd, fds[], num;
{
	struct iovec	iov[1];
	struct msghdr	msg;
	int		ret;

	iov[0].iov_base = (char *) NULL;
	iov[0].iov_len = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_name = (caddr_t) NULL;
	msg.msg_namelen = 0;
	msg.msg_accrights = (caddr_t) fds;
	msg.msg_accrightslen = num * sizeof(int);

	errno = 0;
#if	SFS_COMPAT
	if ((ret = syscall(SYS_sendmsg, sockfd, &msg, 0)) < 0) {
#else
	if ((ret = sendmsg(sockfd, &msg, 0)) < 0) {
#endif
#if	0
	printf("sendmsg ret = %x, errno = %d\n", ret, errno);
#endif
		return (-1);
	}
#if	0
	printf("sent fds %x %x %x, ret = %x, errno = %d\n", fds[0], fds[1], fds[2], ret, errno);
#endif
	return (0);
}

int
send_clfds(sockfd, clstdin, clstdout, clstderr)
int	sockfd, clstdin, clstdout, clstderr;
{
	int	fds[3];

	fds[0] = clstdin;
	fds[1] = clstdout;
	fds[2] = clstderr;
	if (sendfile(sockfd, fds, 3) < 0) return -1;
	return 0;
}

int
getfile(sockfd, fds, num)
int	sockfd, fds[], num;
{
	struct iovec	iov[1];
	struct msghdr	msg;
	int		ret;

	iov[0].iov_base = (char *) NULL;
	iov[0].iov_len = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_name = (caddr_t) NULL;
	msg.msg_namelen = 0;
	msg.msg_accrights = (caddr_t)fds;
	msg.msg_accrightslen = num*sizeof(int);

	errno = 0;
#if	SFS_COMPAT
	if ((ret = syscall(SYS_recvmsg, sockfd, &msg, 0)) < 0) {
#else
	if ((ret = recvmsg(sockfd, &msg, 0)) < 0) {
#endif
#if	0
		printf("bad recvmsg: ret = %x, errno = %d\n", ret, errno);
#endif
		return -1;
	}
#if	0
	printf("got fds %x %x %x, ret = %x, errno = %d\n", fds[0], fds[1], fds[2], ret, errno);
#endif
	return 0;
}

int
get_clfds(sockfd, pclstdin, pclstdout, pclstderr)
int	sockfd, *pclstdin, *pclstdout, *pclstderr;
{
	int	fds[3];

	if (getfile(sockfd, fds, 3) < 0) return -1;
	if (((*pclstdin = fds[0]) < 0) || (*pclstdin >= 20)) return -1;
	if (((*pclstdout = fds[1]) < 0) || (*pclstdout >= 20)) return -1;
	if (((*pclstderr = fds[2]) < 0) || (*pclstderr >= 20)) return -1;
	return 0;
}
#endif	/*USE_MSGHDR*/

int
linearize(sockfd, reqbuf, reqlen, argc, argv, pid)
int	sockfd;
int	reqlen, argc;
char	*reqbuf, *argv[];
int	pid;
{
	int	i;
	unsigned char	array[4];
	int	ptr = 0;
	int	len;

	array[0] = (pid & 0xff000000) >> 24;
	array[1] = (pid & 0xff0000) >> 16;
	array[2] = (pid & 0xff00) >> 8;
	array[3] = (pid & 0xff);
	if (sockfd >= 0) {
		if (writen(sockfd, array, 4) < 4) return -1;
	}
	if (reqbuf != NULL) {
		if (ptr + 4 >= reqlen) return -1;
		memcpy(reqbuf+ptr, array, 4);
		ptr += 4;
	}

	array[0] = (argc & 0xff000000) >> 24;
	array[1] = (argc & 0xff0000) >> 16;
	array[2] = (argc & 0xff00) >> 8;
	array[3] = (argc & 0xff);
	if (sockfd >= 0) {
		if (writen(sockfd, array, 4) < 4) return -1;
	}
	if (reqbuf != NULL) {
		if (ptr + 4 >= reqlen) return -1;
		memcpy(reqbuf+ptr, array, 4);
		ptr += 4;
	}

	for (i=0; i<argc; i++) {
		len = strlen(argv[i]);
		if (sockfd >= 0) {
			if (writen(sockfd, argv[i], len + 1) < len + 1) return -1;
			if (writen(sockfd, "\n", 1) < 1) return -1;	/* so that we can do gets */
		}
		if (reqbuf != NULL) {
			if (ptr + len + 2 >= reqlen) return -1;
			strcpy(reqbuf+ptr, argv[i]);
			ptr += len+1;
			reqbuf[ptr++] = '\0';	/* so that we can do strcpy */
		}
#if	0
		printf("sending %s\n", argv[i]);
#endif
	}
	return ptr;
}

int
delinearize(sockfd, reqbuf, reqlen, pargc, pargv, ppid)
int	sockfd;
int	reqlen, *pargc;
char	*reqbuf, **pargv[];
int	*ppid;
{
	int	i;
	char	line[MAXLINE];
	int	len;
	int	ptr = 0;
	unsigned char	array[4];

	*ppid = 0;
	*pargc = 0;
	*pargv = NULL;
	memset(array, '\0', 4);

	if (sockfd >= 0) if (readn(sockfd, array, 4) != 4) return -1;
	if (reqbuf != NULL) {
		if (ptr+4 >= reqlen) return -1;
		memcpy(array, reqbuf+ptr, 4);
		ptr += 4;
	}
	*ppid = (array[0] << 24) + (array[1] << 16) + (array[2] << 8) + array[3];

	memset(array, '\0', 4);
	if (sockfd >= 0) if (readn(sockfd, array, 4) != 4) return -1;
	if (reqbuf != NULL) {
		if (ptr+4 >= reqlen) return -1;
		memcpy(array, reqbuf+ptr, 4);
		ptr += 4;
	}
	*pargc = (array[0] << 24) + (array[1] << 16) + (array[2] << 8) + array[3];
#if	0
	printf("clargc=%x\n", *pargc);
#endif
	/* VERY important, set hard-coded limit to MAX_ARGS*MAX_NAME_LEN; otherwise can cause the server to allocate TONS of memory */
	if (*pargc <= 0 || *pargc >= (MAX_ARGS*MAX_NAME_LEN)) { *pargc = 0; return -1; }

	if ((*pargv = (char **)malloc(sizeof(char *) * *pargc)) == NULL) {
		/* no memory, so discard */
		*pargc = 0;
		return - 1;
	}
	memset(*pargv, '\0', sizeof(char *) * *pargc);
	for (i=0; i<*pargc; i++) {
		if (sockfd >= 0) {
			if (readline(sockfd, line, MAXLINE) <= 0) return -1;
			if ((len = mystrlen(line, MAXLINE)) <= 0) {
				i--;
				continue;
			}
			if (((*pargv)[i] = (char *)malloc(len + 2)) == NULL) return -1;
			line[len] = '\0';	/* overwrite the '\n' */
			strcpy((*pargv)[i], line);
		}
		if (reqbuf != NULL) {
			if ( ((len = mystrlen(reqbuf+ptr, reqlen-ptr)) <= 0) || (len >= MAXLINE) ) return -1;
			if (((*pargv)[i] = (char *)malloc(len + 2)) == NULL) return -1;
			strcpy((*pargv)[i], reqbuf+ptr);
			ptr += len + 2;
		}
#if	0
		printf("clargv[%x]=%s\n", i, (*pargv)[i]);
#endif
	}
	return ptr;
}

int
sendreq(sockfd, reqbuf, clstdin, clstdout, clstderr, clargc, clargv, clpid)
int	sockfd, clstdin, clstdout, clstderr, clargc, clpid;
char	reqbuf[MAX_ARGS*MAX_NAME_LEN], *clargv[];
{
#if	USE_MSGHDR
	struct iovec	iov[1];
	struct msghdr	msg;
	int		ret;
	int		fds[3];
#endif	/*USE_MSGHDR*/

#if	USE_MSGHDR
	if ((ret = linearize(-1, reqbuf, MAX_ARGS*MAX_NAME_LEN, clargc, clargv, clpid)) < 0) return -1;

	fds[2] = clstdin;
	fds[1] = clstdout;
	fds[0] = clstderr;

	iov[0].iov_base = (char *) reqbuf;
	iov[0].iov_len = ret;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_name = (caddr_t) NULL;
	msg.msg_namelen = 0;
	msg.msg_accrights = (caddr_t) fds;
	msg.msg_accrightslen = 2 * sizeof(int);	/* don't send clstdin */

	errno = 0;
#if	SFS_COMPAT
	if ((ret = syscall(SYS_sendmsg, sockfd, &msg, 0)) < 0) {
#else
	if ((ret = sendmsg(sockfd, &msg, 0)) < 0) {
#endif
#if	0
	printf("sendmsg ret = %x, errno = %d\n", ret, errno);
#endif
		return (-1);
	}
#if	0
	printf("sendreq %x %x %x, ret = %x, errno = %d\n", fds[0], fds[1], fds[2], ret, errno);
#endif
#else	/*USE_MSGHDR*/
	if (linearize(sockfd, (char *)NULL, MAX_ARGS*MAX_NAME_LEN, clargc, clargv, clpid) < 0) return -1;
#endif	/*USE_MSGHDR*/
	return (0);
}

int
getreq(sockfd, reqbuf, pclstdin, pclstdout, pclstderr, pclargc, pclargv, pclpid)
int	sockfd, *pclstdin, *pclstdout, *pclstderr, *pclargc, *pclpid;
char	reqbuf[MAX_ARGS*MAX_NAME_LEN], **pclargv[];
{
#if	USE_MSGHDR
	struct iovec	iov[1];
	struct msghdr	msg;
	int		ret;
	int		fds[3];
#endif	/*USE_MSGHDR*/

#if	USE_MSGHDR
	iov[0].iov_base = (char *) reqbuf;
	iov[0].iov_len = MAX_ARGS * MAX_NAME_LEN;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_name = (caddr_t) NULL;
	msg.msg_namelen = 0;
	msg.msg_accrights = (caddr_t)fds;
	msg.msg_accrightslen = 2*sizeof(int);

	errno = 0;
#if	SFS_COMPAT
	if ((ret = syscall(SYS_recvmsg, sockfd, &msg, 0)) < 0) {
#else
	if ((ret = recvmsg(sockfd, &msg, 0)) < 0) {
#endif
#if	0
		printf("bad recvmsg: ret = %x, errno = %d\n", ret, errno);
#endif
		return -1;
	}

	*pclstdin = fds[2];
	*pclstdout = fds[1];
	*pclstderr = fds[0];

	if ((ret == delinearize(-1, reqbuf, MAX_ARGS * MAX_NAME_LEN, pclargc, pclargv, pclpid)) < 0) return -1;
#if	0
	printf("getreq %x %x %x, ret = %x, errno = %d\n", fds[0], fds[1], fds[2], ret, errno);
#endif
#else	/*USE_MSGHDR*/
	if (delinearize(sockfd, (char *)NULL, MAX_ARGS * MAX_NAME_LEN, pclargc, pclargv, pclpid) < 0) return -1;
	*pclstdin = -1;
	*pclstdout = sockfd;
	*pclstderr = sockfd;
#endif	/*USE_MSGHDR*/
	return (0);
}
