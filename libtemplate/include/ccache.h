/*
 *  ccache.h - FTP Connection Cache
 *
 *  David Merkel & Mark Peterson, University of Colorado - Boulder, July 1994
 *
 *  $Id: ccache.h,v 1.2 2006/02/03 16:59:14 golda Exp $
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
 *  
 */
#ifndef _CCACHE_H_
#define _CCACHE_H_

#include <stdio.h>	/* for FILE */
#include "url.h"	/* for URL */
#include "config.h"

#ifndef _PARAMS
#define _PARAMS(ARGS) ARGS
#endif /* _PARAMS */   

typedef char Datum;
typedef int Boolean;

#define HASH_SLOTS 	(256)
#define MIN_CONNECTIONS	3	/* Min number of connections to maintain. */
#define MIN_TIMEOUT	5	/* Min timeout in minutes. */

typedef enum {
	INACTIVE, MEMORY_ONLY, FILE_ONLY, OPTIMIZE, TEMP
} BufferStatus;

typedef struct SockCntlRec {
	int timerid;			/* ID of the expiration timer. */
	URL *socketInfo;		/* The URL structure */
	int theSocket;
	unsigned long theHostIP;
	char *incomDataBuf;		/* Incoming data buffer */
	FILE *incomDataFile;		/* Incoming data file buffer */
	int incomDataSize;		/* Number of bytes in buffer */
	BufferStatus sockStateStor;	/* Default status for buffering */
	struct SockCntlRec *hashPrev;	/* Hash table previous */
	struct SockCntlRec *hashNext;	/* Hash table next */
	struct SockCntlRec *listPrev;	/* TimeOut list previous */
	struct SockCntlRec *listNext;	/* TimeOut list next */
} SockCntlRec;


typedef struct InitConfigRec {
	int maxConnections;	/* max # of socks to keep open at one time. */
	long timeOut;		/* TimeOut time in seconds. */
} InitConfigRec;

enum ptype {
        STR, INT, POINTER, MD5
};

typedef enum ptype PType;

#define MAX_LINE_LENGTH			1024
#define MAX_FILENAME_LENGTH		1024
#define HOST_NAME_LENGTH		1024
#define SERV_REPLY_LENGTH		3
#define BUFFER_SIZE			1024	/* buffer size for read calls */
#define INIT_FILE_SIZE			50000	/* bufsz if read files in mem */
#define REALLOC_BLOCK			2048	/* blk sz on realloc calls */
#define BACK_LOG			5
#define ACCEPT_TIMEOUT			5	/* time in seconds to timeout */
#define READ_TIMEOUT			5	/* if no data on line */

#define MULTI_LINE_CODE			'-'

/* 
 *  Controls newly created files permissions, as per chmod() arguments.
 *  Final permissions are determined by process umask settings
 */
#define INIT_PERMISSION			0666	/* 'rw-rw-rw-' */

#define MAX_MESSAGE_LENGTH		50
#define PORT_MESSAGE_LENGTH		12

/* ftp message defines */
#define CONNECT				"CONNECT"
#define USER				"USER"
#define PASSWD				"PASS"
#define MODE				"MODE"
#define TYPE				"TYPE"
#define RETRIEVE			"RETR"
#define PORT				"PORT"
#define REINIT				"REIN"
#define DISCONNECT			"QUIT"

/* numerical defines for previous for fast compares */
#define CONNECT_CHK			0
#define USER_CHK			1
#define PASSWD_CHK			2
#define MODE_CHK			3
#define TYPE_CHK			4
#define RETRIEVE_CHK			5
#define PORT_CHK			6
#define REINIT_CHK			7
#define DISCONNECT_CHK			8


/* ftp mode and type defines */
#define IMAGE				'I'
#define STREAM				'S'

/* ftp server reply codes */
#define DATA_CONN_OPEN			(0x31323500)	/* "125" */
#define START_TRANS			(0x31353000)	/* "150" */
#define CMD_OKAY			(0x32303000)	/* "200" */
#define CLOSING				(0x32323100)	/* "221" */
#define CONNECT_EST			(0x32323000)	/* "220" */
#define TRANS_SUCCESS			(0x32323600)	/* "226" */
#define USER_LOGIN			(0x32333000)	/* "230" */
#define SEND_PASS			(0x33333100)	/* "331" */


typedef struct data_return {
	Boolean inMemory;	/* set if return file in mem */
	Boolean useTempFile;	/* set if use temp file */
	char *buffer;		/* for memory return */
	char fileName[MAX_FILENAME_LENGTH];	/* save if not in memory */
	long fileSize;		/* data size */
} DataReturn;

typedef short ERRCODE;
typedef int CacheErr;

#define SERV_NOT_RDY  	(0x31323000)	/*"120" */
#define NEED_ACCOUNT   	(0x33333200)	/*"332" */
#define SERV_NOT_AVAIL 	(0x34323100)	/*"421" */
#define SYNTAX_ERR     	(0x35303000)	/*"500" */
#define SYNTAX_ERR_PARM	(0x35303100)	/*"501" */
#define CMD_NOT_IMPL   	(0x35303200)	/*"502" */
#define BAD_CMD_SEQ    	(0x35303300)	/*"503" */
#define CMD_UNIMP_PARM 	(0x35303400)	/*"504" */
#define NOT_LOGD_IN    	(0x35333000)	/*"530" */
#define FILE_NOT_FOUND	(0x35353000)	/*"550" */

#define noErr		0
#define srvNotRdy	1
#define needAccnt	2
#define srvNotAvl	3
#define syntaxErr	4
#define cmdNotImp	5
#define badCmdSeq	6
#define cmdNImpParam	7
#define notLogdIn	8

#define initSockErr	9
#define readSockErr	10
#define getSockErr	11
#define getHostErr	12
#define connectErr	13
#define memoryErr	14
#define writeSockErr	15
#define setSockOptErr	16
#define getHNameErr	17
#define getHBNameErr	18
#define getSNameErr	19
#define bindErr		20
#define fileOpenErr	22
#define writeFileErr	23
#define tmpNameErr	24
#define acceptTOut	25
#define readTOut	26
#define argInvalid	27
#define noReply		28
#define badParam	29
#define urlErr		30
#define badurlType	31
#define fileNotFound	32

#define NO_ERROR		0
#define INIT_SOCKET_ERR		-1
#define READ_FROM_SOCK_ERR	-2
#define GET_SOCKET_ERR		-3
#define GETHOST_ERR		-4
#define CONNECT_ERR		-5
#define MEMORY_ERROR		-6
#define WRITE_TO_SOCK_ERR	-7
#define SET_SOCKOPT_ERR		-8
#define GET_HOSTNAME_ERR	-9
#define GET_HOSTBYNAME_ERR	-10
#define GET_SOCKNAME_ERR	-11
#define BIND_ERR		-12
#define FILE_OPEN_ERR		-14
#define WRITE_FILE_ERR		-15
#define CANT_GET_TMPNAME	-16

#define SERV_REPLY_ERROR	-20
#define NO_PASS_REQ		-21
#define ACCEPT_TIMEOUT_ERR	-22
#define READ_TIMEOUT_ERR	-23
#define ARGUMENT_INVALID	-24
#define NO_REPLY_PRESENT	-25

#define BAD_PARAM_ERR	-50
#define URL_ERR		-51
#define BAD_URL_TYPE	-52


#ifndef DEBUG
#undef DEBUG			4	/* debug level */
#endif
#ifndef TRUE
#define TRUE			1
#endif
#ifndef FALSE
#define FALSE 			0
#endif

#define BACK_LOG        	5
#define INIT_URL_LEN    	256
#define INIT_PARAM_LEN		50
#define REALLOC_BLK		20

#define LINE_FEED               '\n'
#define CARRG_RET               '\r'
#define PARAM_END               '.'
#define BLOCK_END               '!'
#define TERM_LEN                3
#define MD5_LEN			32

/* timeouts for read calls on sockets */

/* timeout for server receiving client request messages */
#define SERVER_TIMEOUT		5	

/* timeout for util calls reading params from socket */
#define PARAM_TIMEOUT		5	

/* timeout for client waiting for server response */
#define CLIENT_TIMEOUT		120	


/* ftp.c */
int FTPInit _PARAMS((u_long, int, char *));
int Login _PARAMS((char *, char *, int, Boolean, char *));
int Disconnect _PARAMS((int, char *));
int Retrieve _PARAMS((char *, int,int,Boolean,Boolean, char *, DataReturn *));

/* ftp_util.c */
int InitSocket _PARAMS((u_long, int ));
int ReadServReply _PARAMS((int, char *));
int SendMessage _PARAMS((char *, char *, int));
void ReadOutText _PARAMS((int, Boolean, char *));
int CheckServReply _PARAMS((int, char *));
int PrepareDataConnect _PARAMS((int, Boolean));
int RetrieveFile _PARAMS((int, DataReturn *));

Boolean DisconnectOne _PARAMS(());
CacheErr GetError _PARAMS(());
char *GetFTPError _PARAMS(());
void DoError _PARAMS((CacheErr, char *));

/* ccache_util.c */
int MyRead _PARAMS((int, char *, int, int));
int AddURL _PARAMS((URL *, char **, int, int, Boolean));
int GetURL _PARAMS((URL *, int));
int GetParam _PARAMS((char **, PType, int));
int AddParam _PARAMS((char *, PType,  char **, int *, int, Boolean));
int SocketWrite _PARAMS((int, char *, int));
void PrintURL _PARAMS((URL *));
unsigned long gethostinhex _PARAMS((char *));

void SockInit _PARAMS((InitConfigRec *));
DataReturn *SockGetData _PARAMS((URL *, BufferStatus, char *));
void ShutDownCache _PARAMS(());
void DestroyDataReturn _PARAMS((DataReturn *));


#endif /* _CCACHE_H_ */
