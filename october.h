/*

Simple threaded HTTP server. Read GET replies from a host and respond appropriately.

Main header file

(c) 2012 Kyle J Aleshire

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

/* response types. HTTP 1.0 since we're not supporting a lot of HTTP 1.0 (specifically Connection: keep-alive) */
#define OK "HTTP/1.0 200 OK\n"
#define NOTFOUND "HTTP/1.0 404 Not Found\n"

/* message end. carriage return + line feed */
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

#endif /* __october_h */