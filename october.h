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

#define BUFFSIZE		1024
#define LISTENQ			1024
#define DOCROOT			"site"

/* error types */
#define ERRPROG			-1
#define ERRSYS			-2
#define THREADERRPROG	-10
#define THREADERRSYS	-20

/* logging levels */
#define LOGNONE			0
#define LOGPANIC		1
#define LOGERR			2
#define LOGINFO			3
#define LOGDEBUG		4

/* request types */
#define GET				"GET"
#define POST			"POST"
#define OPTIONS			"OPTIONS"
#define HEAD			"HEAD"
#define PUT				"PUT"

/* response types. HTTP 1.0 since we're not (yet) supporting a lot of HTTP 1.1 (specifically Connection: keep-alive) */
#define OK_R			"200 OK"
#define NOTFOUND_R		"404 Not Found"
#define NOTFOUNDHTML	"<html><body><p>Error 404, resource not found.</p></body></html>"

/* default filename */
#define DEFAULTFILE		"index.html"

/* response headers */
#define DATE_H			"Date: "
#define CONTENT_T_H		"Content-Type: "
#define EXPIRES_H		"Expires: -1"
#define SERVER_H		"Server: KJAd"

/* MIME types... */
#define MIME_HTML		"text/html; "
#define MIME_JPG		"image/jpeg; "
#define MIME_GIF		"image/gif; "
#define MIME_PNG		"image/png; "
#define MIME_CSS		"text/css; "
#define MIME_JS			"application/javascript; "
#define MIME_TXT		"text/plain; "

/* and character set */
#define CHARSET			"charset=utf-8"

/* request headers for comparison */
#define HOST_H			"Host:"
#define CONNECTION_H	"Connection:"

/* header values */
#define KEEPALIVE_H		"keep-alive"
#define HTTP11_H		"HTTP/1.1"
#define HTTP10_H		"HTTP/1.0"

/* special flags */
#define GET_F			0x00000001
#define OPTIONS_F		0x00000002
#define HEAD_F			0x00000004
#define POST_F			0x00000008
#define PUT_F			0x00000010

#define HTTP11_F		0x00000080

#define HOST_F			0x00000100
#define CONNECTION_F	0x00000200


/* misc. defs */
#define CRLF "\r\n"

typedef struct threadargs {
	int conn_fd;
	struct sockaddr_in conn_info;
	struct reqargs *request;
	char readbuff[BUFFSIZE];
	char writebuff[BUFFSIZE];
	int readindex;
	int writeindex;
} threadargs_t;

typedef struct reqargs {
	uint32_t conn_flags;
	char scratchbuff[BUFFSIZE];
	char* file;
	char* mimetype;
} reqargs_t;

int log_level;
FILE* log_fd;
pthread_mutex_t mtx_term;

void october_worker_thread(threadargs_t*);
void october_get_handler(reqargs_t*, threadargs_t*);
char* october_detect_type(char*);
void october_worker_cleanup(threadargs_t* t_args);
void october_panic(int error, const char* message, ...);
void october_log(int err_level, const char* message, ...);

#endif /* __october_h */