/*

Simple threaded HTTP server. Read GET replies from a host and respond appropriately.

Main program file

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

#include "october.h"

#define PORT 80
#define ADDRESS "127.0.0.1"

int main(int argc, char *argv[]){

	/* TODO implement command line parameter parser */

	int v;
	/* listening socket, sits on the main thread and doesn't move */
	int listen_fd;
	/* connection socket descriptor */
	int conn_fd;
	int listen_address;
	struct sockaddr_in servaddr;
	struct sockaddr_in* conn_info;
	pthread_t thread_id;
	socklen_t sockaddr_in_size;
	threadargs_t* t_args;

	log_level = 4;

	log_fd = stdout;

	/* inet_pton(AF_INET, ADDRESS, &listen_address); */
	listen_address = htonl(INADDR_ANY);

	/* set up a new socket for us to (eventually) listen us */
	if( (listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		october_panic(ERRSYS, "socket create error");
	} else {
		october_log(LOGINFO, "socket create success");
	}

	/* setsocketopt() sets a socket option to allow multiple processes to
	   listen. this is only needed during testing so we can quickly restart
	   the server without waiting for the port to close. */
	v = 1;
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(int)) < 0) {
		october_panic(ERRSYS, "setsockopt() failed");
	} else {
		october_log(LOGINFO, "setsockopt() SO_REUSEADDR success");
	}

	/* zero out the address structures for the listening and accepted sockets */
	memset(&servaddr, 0, sizeof(servaddr));

	/* set the server listening parameters: IPv4, IP address, port number */
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(listen_address);
	servaddr.sin_port = htons(PORT);

	/* bind our socket to the specified address/port */
	if( bind(listen_fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		october_panic(ERRSYS, "socket error binding to address %s", servaddr.sin_addr.s_addr == htonl(INADDR_ANY) ? "INADDR_ANY" : ADDRESS);
	} else {
		october_log(LOGINFO, "socket successfully bound to %s", servaddr.sin_addr.s_addr == htonl(INADDR_ANY) ? "INADDR_ANY" : ADDRESS);
	}

	/* set the socket to passive listening on the already-bound address/port */
	if( listen(listen_fd, LISTENQ) < 0 ) {
		october_panic(ERRSYS, "socket listen error on port %d", PORT);
	} else {
		october_log(LOGINFO, "socket set to listen on port %d", PORT);
	}

	/* save the size of the sockaddr structure since we're going to use it a lot */
	sockaddr_in_size = (socklen_t) sizeof(struct sockaddr_in);

	/* enter our socket listen loop */
	for(;;) {
		/* allocate and zero out client address structure */
		conn_info = malloc(sizeof(*conn_info));
		memset(conn_info, 0, sizeof(*conn_info));

		/* at the call to accept(), the main thread blocks until a client connects */
		if( (conn_fd = accept(listen_fd, (struct sockaddr *) conn_info, &sockaddr_in_size)) < 0) {
			october_panic(ERRSYS, "connection accept failure from client %s", inet_ntoa(conn_info->sin_addr));
		} else {
			october_log(LOGINFO, "accepted connection from client %s", inet_ntoa(conn_info->sin_addr));
		}

		/* make sure terminal output is flushed since we're dealing with different output descriptors */
		fflush(stdout);

		/* set up our thread argument structure. we need to pass the new connection descriptor and client address structure */
		t_args = malloc(sizeof((*t_args)));
		t_args->conn_fd = conn_fd;
		t_args->conn_info = conn_info;

		/* spawn a new thread to handle the accept()'ed connection */
		if( (pthread_create(&thread_id, NULL, (void *(*)(void *)) october_worker_thread, t_args)) < 0) {
			october_panic(ERRSYS, "spawn connection thread failed with ID %d and address %s", thread_id, inet_ntoa(conn_info->sin_addr));
		}

		/* we don't care what the thread does after we spawn it; detach so we're not leaking memory when it stops */
		if( pthread_detach(thread_id) < 0) {
			october_panic(ERRSYS, "thread ID %d failed to detach", thread_id);
		} else {
			october_log(LOGINFO, "thread ID %d detached", thread_id);
		}
	}
}

/* worker thread main thread; to be called when spawned */
void october_worker_thread(threadargs_t *t_args) {
	int v; /* for miscellaneous values, only used very locally (usually in if(function()) idioms) */
	int buff_index;
	int alt_index;
	char c;
	/* read requests and write replies into this buffer */
	char buff[MAXLINE];
	/* read the request in here for comparison */
	char request[REQUEST];
	/* read filename here to pass to connection function */
	char* filename;

	october_log(LOGINFO, "spawned new thread with ID %d to handle connection from %s", pthread_self(),  inet_ntoa(t_args->conn_info->sin_addr));

	pthread_cleanup_push( (void (*) (void *)) october_worker_conn_cleanup, t_args);

	/* read the request into the request buffer. We call read() multiple times in case we don't get the
	   whole request in one packet (who knows?) */
	buff_index = 0;
	for(;;) {
		if( (v = read( t_args->conn_fd, &buff[buff_index], MAXLINE - buff_index - 1 )) < 0) {
			october_worker_panic(ERRSYS, "connection request read error");
		} else {
			buff_index = buff_index + v;
			october_log(LOGDEBUG, "connection read %d bytes as request: %s", v, buff);
		}
		if( strnstr(buff, CRLF, buff_index) != NULL || v == 0) {
			break;
		}
	}
	
	/* we have the request string, read the type of request into the request buffer now for comparison.
	   For now only GET is supported, other types to come later. We can use use buff_index for both buffers
	   since this is the first token we're extracting */
	buff_index = 0;
	while( (c = buff[buff_index]) != ' ' &&
								c != '\0' &&
								c != '\n' &&
								c != '\r' &&
								buff_index < (REQUEST - 1) ) {
		request[buff_index++] = c;
	} request[buff_index] = '\0';

	/* detect what kind of request this is, starting with GET. */
	october_log(LOGDEBUG, "request string NULL terminated");
	if( strcmp(GET, request) == 0 ){
		october_log(LOGDEBUG, "received GET request of sockaddr_in_size %d", strlen(request));
		/* fast foward through any extra spaces */
		while(buff[buff_index] == ' ') {
			buff_index++;
		}

		october_log(LOGDEBUG, "allocating filename buffer");
		filename = malloc(sizeof(char) * MAXLINE);

		/* register the cleanup function to be called if this thread perishes */
		pthread_cleanup_push( (void (*) (void *)) october_worker_get_handler_cleanup, filename);
		october_log(LOGDEBUG, "reading filename from read buffer into filename buffer", buff_index);

		/* read the next token (should be the filename) into the filename buffer */
		alt_index = 0;
		while( (c = buff[buff_index]) != ' ' &&
			 					  c != '\n' &&
			 					  c != '\r' &&
			 					  c != '\0' &&
			buff_index < (FILENAME - strlen(request) - 1) ) {
			filename[alt_index++] = c;
			buff_index++;
		} filename[alt_index] = '\0';
		october_log(LOGDEBUG, "read filename: %s", filename);

		/* TODO implement method for handling paths longer than FILENAME */

		/* call the worker GET handler and hand it the connection descriptor and filename */
		october_worker_get_handler(t_args->conn_fd, filename);
		/* we've returned from the GET request handler, execute the registered cleanup function */
		pthread_cleanup_pop(1);	

		/* make sure this isn't a different kind of request, or malformed request. */
	} else if ( ( strcmp(HEAD, request) == 0 ) ||
				( strcmp(OPTIONS, request) == 0 ) ||
				( strcmp(POST, request) == 0 ) ||
				( strcmp(PUT, request) == 0 ) ) {
		october_worker_panic(ERRPROG, "application does not support POST, HEAD, OPTIONS or PUT");
	} else {
		october_worker_panic(ERRPROG, "malformed request:\n%s", buff);
	}

	pthread_cleanup_pop(1);
	pthread_exit(NULL);
}

/* function called when GET request is received */
void october_worker_get_handler(int conn_fd, char* filename) {
	int v;
	char buff[MAXLINE];
	char timebuff[TIME];
	int file_fd;
	time_t ticks;

	october_log(LOGDEBUG, "GET worker function called");

	/* if the filename ends with '/', assume they're asking for the default file and append our default filename */
	if( strcmp(&filename[strlen(filename) - 1], "/") == 0 && strlen(filename) + strlen(DEFAULTFILE) + 1 < FILENAME) {
		buff[0] = '.';
		strcat(&buff[1], filename);
		strcpy(filename, buff);
		strcat(filename, DEFAULTFILE);
	}


	october_log(LOGDEBUG, "detecting if file exists: %s", filename);
	if( stat(filename, NULL) < 0 && errno == ENOENT ) {
		october_log(LOGDEBUG, "file not found: %s", filename);
		october_file_write(conn_fd, NOTFOUND, strlen(NOTFOUND));
	} else {
		october_log(LOGDEBUG, "file found: %s", filename);

		october_file_write(conn_fd, OK, strlen(OK));

		/* write the Date: header */
		october_file_write(conn_fd, DATE, strlen(DATE));
		ticks = time(NULL);
		snprintf(timebuff, sizeof(timebuff), "%.24s", ctime(&ticks));
		strcat(timebuff, "\n");
		october_file_write(conn_fd, timebuff, strlen(timebuff));

		/* open file for reading */
		file_fd = open(filename, O_RDONLY);

		/* TODO detect file type and send MIME header */

		while( (v = read(file_fd, buff, MAXLINE - 1)) != 0 ) {
			october_log(LOGDEBUG, "Read %d bytes from file descriptor", v);
			if( v < 0 ) {
				october_worker_panic(ERRSYS, "worker thread read error");
			} else {
				buff[v] = '\0';
				october_file_write(conn_fd, buff, v);
			}
		}
	}
	
	october_file_write(conn_fd, CRLF, strlen(CRLF));
}

void october_worker_conn_cleanup(threadargs_t *t_args) {
	if(close(t_args->conn_fd) < 0) {
		october_panic(ERRSYS, "connection socket close error");
	} else {
		october_log(LOGINFO, "connection socket closed");
	}

	free(t_args->conn_info);
	free(t_args);
}

void october_worker_get_handler_cleanup(char* filename) {
	free(filename);
}

void october_file_write(int fd, char* buff, int buff_sockaddr_in_size) {
	october_log(LOGDEBUG, "fd write function called");
	int v;
	if( (v = write(fd, buff, buff_sockaddr_in_size)) < 0) {
		october_worker_panic(ERRSYS, "connection write error");
	} else {
		october_log(LOGDEBUG, "wrote %d bytes on connection socket: %s", v, buff);
	}
}

/* panic button. print an error message and quit */
void october_panic(int error, const char* message, ...) {
	if(LOGPANIC <= log_level) {
		switch(error) {
			case ERRPROG:
				printf("Program error: ");
				break;
			case ERRSYS:
				printf("System error: ");
				break;
			default:
				;
		}
		va_list arglist;
		va_start(arglist, message);
		vfprintf(log_fd, message, arglist);
		printf("\n");
		va_end(arglist);
	}
	exit(error);
}

/* thread panic, not mission-critical, like malformed request */

void october_worker_panic(int error, const char* message, ...) {
	if(LOGERR <= log_level) {
		switch(error) {
			case ERRPROG:
				printf("Program error: ");
				break;
			case ERRSYS:
				printf("System error: ");
				break;
			default:
				;
		}
		va_list arglist;
		va_start(arglist, message);
		vfprintf(log_fd, message, arglist);
		printf("\n");
		va_end(arglist);
	}
	pthread_exit(NULL);
}

/* log stuff */
void october_log(int err_level, const char* message, ...) {
	if(err_level <= log_level) {
		va_list arglist;
		va_start(arglist, message);
		vfprintf(log_fd, message, arglist);
		printf("\n");
		va_end(arglist);
	}
}