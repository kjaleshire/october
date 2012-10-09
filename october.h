#ifndef __october_h
#define __october_h

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <sys/un.h>
#import <pthread.h>
#import <errno.h>
#import <fcntl.h>

#include <time.h>

#define BUFFSIZE 8192
#define MAXLINE 4896
#define LISTENQ 1024
#define FILENAME 1024
#define REQUEST 8
#define TIME 32

/* error types */
#define ERRPROG -1
#define ERRSYS -2

/* logging levels */
#define LOGNONE 0
#define LOGPANIC 1
#define LOGERR 2
#define LOGINFO 3
#define LOGDEBUG 4

/* request types */
#define GET "GET"
#define POST "POST"
#define OPTIONS "OPTIONS"
#define HEAD "HEAD"
#define PUT "PUT"

/* response types */
#define OK "HTTP/1.1 200 OK"
#define NOTFOUND "HTTP/1.1 404 Not Found"

/* message end */
#define CRLF "\r\n"

/* default filename */
#define DEFAULTFILE "index.html"

/* Date header */
#define DATE "Date: "

typedef struct threadargs {
	int conn_fd;
	struct sockaddr_in* conn_info;
} threadargs_t;

int log_level;
FILE* log_fd;

void october_worker_thread(threadargs_t* t_args);
void october_worker_get_handler(int conn_fd, char* filename);
void october_file_write(int fd, char* buff, int buff_length);
void october_worker_conn_cleanup(threadargs_t* t_args);
void october_worker_get_handler_cleanup(char* filename);
void october_worker_panic(int error, const char* message, ...);
void october_panic(int error, const char* message, ...);
void october_log(int err_level, const char* message, ...);

#endif