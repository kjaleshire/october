/*

Simple threaded HTTP server. Read GET replies from a host and respond appropriately.

Main header file

(c) 2012 Kyle J Aleshire
All rights reserved

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

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
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>

#define BUFFSIZE 1024
#define LISTENQ 1024
#define TIME 32
#define DOCROOT "site"

/* error types */
#define ERRPROG -1
#define ERRSYS -2
#define THREADERRPROG -10
#define THREADERRSYS -20

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

/* response types. HTTP 1.0 since we're not supporting a lot of HTTP 1.0 (specifically Connection: keep-alive) */
#define OK "HTTP/1.0 200 OK\n"
#define NOTFOUND "HTTP/1.0 404 Not Found\n"
#define NOTFOUNDHTML "<html><body><p>Document not found, error 404</p></body></html>"

/* default filename */
#define DEFAULTFILE "index.html"

/* headers */
#define DATE_H "Date: "
#define CONTENT_T_H "Content-Type: "

/* connection method flags */
#define GET_F	0x00000001
#define HEAD_F	0x00000002

/* HTTP header flags per connection */
#define CONNECTION_F	0x00000100
#define CONTENTTYPE_F	0x00000200

/* special flags */
#define HTTPVERSION_F 	0x01000000
#define FILENAME_F		0x02000000
#define HOSTHEADER_F	0x04000000
#define DATEHEADER_F	0x08000000

/* MIME types */
#define MIME_HTML "text/html; "
#define MIME_JPG "image/jpeg; "
#define MIME_GIF "image/gif; "
#define MIME_PNG "image/png; "
#define MIME_CSS "text/css; "
#define MIME_JS "application/javascript; "
#define MIME_TXT "text/plain; "

/* character set */
#define CHARSET "charset=utf-8\n"

/* temporary headers until we implement the proper handlers */
#define CONNECTION_H "Connection: close"

typedef struct threadargs {
	int conn_fd;
	struct sockaddr_in conn_info;
	char readbuff[BUFFSIZE];
	char writebuff[BUFFSIZE];
	int readindex;
	int writeindex;
} threadargs_t;

typedef struct reqargs {
	uint32_t conn_flags;
	char* method;
	char* file;
	char scratchbuff[BUFFSIZE];
	char* http_ver;
	char* mimetype;
} reqargs_t;

int log_level;
FILE* log_fd;

void october_worker_thread(threadargs_t* t_args);
char* october_detect_type(char*);
void october_worker_cleanup(threadargs_t* t_args);
void october_panic(int error, const char* message, ...);
void october_log(int err_level, const char* message, ...);

#endif /* __october_h */