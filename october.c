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
	int listen_address;
	struct sockaddr_in servaddr;
	struct sockaddr_in* conn_info;
	pthread_t thread_id;
	socklen_t sockaddr_in_size;
	threadargs_t* t_args;
	pthread_attr_t attr;

	log_level = LOGINFO;

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
		october_panic(ERRSYS, "socket error binding to %s", servaddr.sin_addr.s_addr == htonl(INADDR_ANY) ? "INADDR_ANY" : ADDRESS);
	} else {
		october_log(LOGINFO, "socket bound to %s", servaddr.sin_addr.s_addr == htonl(INADDR_ANY) ? "INADDR_ANY" : ADDRESS);
	}

	/* set the socket to passive listening on the already-bound address/port */
	if( listen(listen_fd, LISTENQ) < 0 ) {
		october_panic(ERRSYS, "socket listen error on port %d", PORT);
	} else {
		october_log(LOGINFO, "socket set to listen on port %d", PORT);
	}

	/* save the size of the sockaddr structure since we're going to use it a lot */
	sockaddr_in_size = (socklen_t) sizeof(struct sockaddr_in);

	/* set up a thread attributes structure so we don't need an extra call to pthread_detach() */
	if( pthread_attr_init(&attr) < 0 || pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) < 0) {
		october_panic(ERRSYS, "error initializing thread attributes structure");
	} else {
		october_log(LOGDEBUG, "initialized thread attributes structure");
	}

	/* enter our socket listen loop */
	for(;;) {
		/* set up our thread argument structure. we need to pass the new connection descriptor and client address structure as well
		   as create read and write buffers */
		t_args = malloc(sizeof(threadargs_t));
		t_args->readindex = 0;
		t_args->writeindex = 0;

		/* at the call to accept(), the main thread blocks until a client connects */
		if( (t_args->conn_fd = accept(listen_fd, (struct sockaddr *) &(t_args->conn_info), &sockaddr_in_size)) < 0) {
			october_panic(ERRSYS, "connection accept failure from client %s", inet_ntoa(t_args->conn_info.sin_addr));
		} else {
			october_log(LOGINFO, "accepted connection from client %s", inet_ntoa(t_args->conn_info.sin_addr));
		}

		/* spawn a new thread to handle the accept()'ed connection */
		if( (pthread_create(&thread_id, &attr, (void *(*)(void *)) october_worker_thread, t_args)) < 0) {
			october_panic(ERRSYS, "spawn connection thread failed with ID %d and address %s", thread_id, inet_ntoa(t_args->conn_info.sin_addr));
		}
	}
}

/* worker thread main thread; to be called when spawned */
void october_worker_thread(threadargs_t *t_args) {
	int v; char *c; /* for miscellaneous values, only used very locally (usually in if(function()) idioms) */
	int alt_index;
	int file_fd;
	reqargs_t request;
	char *token, *linetoken;
	time_t ticks;

	october_log(LOGINFO, "spawned to handle %s",  inet_ntoa(t_args->conn_info.sin_addr));

	pthread_cleanup_push( (void (*) (void *)) october_worker_cleanup, t_args);

	/* read the request into the request buffer. We call read() multiple times in case we don't get the
	   whole request in one packet (who knows?) */
	for(;;) {
		if( (v = read( t_args->conn_fd, &(t_args->readbuff[t_args->readindex]), BUFFSIZE - t_args->readindex )) < 0) {
			october_panic(THREADERRSYS, "connection request read error");
		} else {
			t_args->readindex += v;
		}
		if( v == 0 ) {
			t_args->readbuff[t_args->readindex] = '\0';
		} else if( strnstr(t_args->readbuff, "\r", t_args->readindex) != NULL ) {
			t_args->readbuff[t_args->readindex] = '\0';
			break;
		}
	} october_log(LOGDEBUG, "connection read %d bytes as request: %s", t_args->readindex + v, t_args->readbuff);


	/*strip out the carriage returns so we don't get weird terminal output */
	*(strstr(t_args->readbuff, "\r")) = 0;

	/* we have the request string, read the type of request into the request buffer now for comparison.
	   For now only GET is supported, other types to come later. */
	request.conn_flags = 0x00000000;

	/* begin tokenizing the read buffer. */
	linetoken = t_args->readbuff;
	if((token = strsep(&linetoken, "\n")) == NULL || (request.method = strsep(&token, " ")) == NULL) {
		october_panic(THREADERRPROG, "no request received");
	}

	/* test for GET request and handle appropriately */
	if( strcmp(GET, request.method) == 0){
 
		if( (request.file = strsep(&token, " ")) == NULL) {
			october_panic(THREADERRPROG, "no filename received");
		} else {
			october_log(LOGDEBUG, "file %s requested", request.file);
			request.conn_flags |= FILENAME_F;
		}

		if( (request.http_ver = strsep(&token, " ")) == NULL ) {
			october_log(LOGDEBUG, "no HTTP protocol requested, assuming HTTP/1.0");
		} else {
			october_log(LOGDEBUG, "HTTP protocol %s requested", request.http_ver);
			request.conn_flags |= HTTPVERSION_F;
		}

		c = request.file;
		while( (c = strchr(c, '%')) != NULL ) {
			if( strncmp(c, "%20", 3) == 0) {
				*c = ' ';
				strcpy(request.scratchbuff, &(c[3]));
				strcpy(&(c[1]), request.scratchbuff);
				c = &(c[1]);
			}
			c = &(c[1]);
		}

		/* if the filename ends with '/', assume they're asking for the default file and append our default filename */
		if( strcmp(&(request.file[strlen(request.file) - 1]), "/") == 0) {
			snprintf(t_args->writebuff, BUFFSIZE, "%s%s%s", DOCROOT, request.file, DEFAULTFILE);
		} else {
			snprintf(t_args->writebuff, BUFFSIZE, "%s%s", DOCROOT, request.file);
		} october_log(LOGDEBUG, "absolute path %s requested", t_args->writebuff);

		ticks = time(NULL);
		assert(t_args->writeindex == 0);

		october_log(LOGDEBUG, "detecting if file exists: %s", t_args->writebuff);
		if( stat(t_args->writebuff, NULL) < 0 && errno == ENOENT ) {
			october_log(LOGINFO, "404 file not found: %s", t_args->writebuff);

			t_args->writeindex += snprintf(&(t_args->writebuff[t_args->writeindex]), BUFFSIZE, "%s\n", NOTFOUND);
			t_args->writeindex += snprintf(&(t_args->writebuff[t_args->writeindex]), BUFFSIZE, "%s%s%s\n", CONTENT_T_H, MIME_HTML, CHARSET);
			t_args->writeindex += snprintf(&(t_args->writebuff[t_args->writeindex]), BUFFSIZE, "%s%s\n", DATE_H, ctime(&ticks));
			t_args->writeindex += snprintf(&(t_args->writebuff[t_args->writeindex]), BUFFSIZE, "%s\n", NOTFOUNDHTML);

			assert(t_args->writeindex <= BUFFSIZE);
		} else {
			october_log(LOGINFO, "200 OK file found: %s", t_args->writebuff);
			if( (file_fd = open(t_args->writebuff, O_RDONLY)) < 0 ){
				october_panic(THREADERRSYS, "error opening file: %s", t_args->writebuff);
			} else {
				october_log(LOGDEBUG, "file opened: %s", t_args->writebuff);
			}
			request.mimetype = october_detect_type(t_args->writebuff);

			t_args->writeindex += snprintf(&(t_args->writebuff[t_args->writeindex]), BUFFSIZE, "%s\n", OK);
			t_args->writeindex += snprintf(&(t_args->writebuff[t_args->writeindex]), BUFFSIZE, "%s%s%s\n", CONTENT_T_H, request.mimetype, CHARSET);
			t_args->writeindex += snprintf(&(t_args->writebuff[t_args->writeindex]), BUFFSIZE, "%s%.24s\n", DATE_H, ctime(&ticks));
			t_args->writeindex += snprintf(&(t_args->writebuff[t_args->writeindex]), BUFFSIZE, "%s\n%s\n\n", EXPIRES_H, SERVER_H);

			assert(t_args->writeindex <= BUFFSIZE);

			while( (v = read(file_fd, &(t_args->writebuff[t_args->writeindex]), BUFFSIZE - t_args->writeindex)) != 0 ) {
				october_log(LOGDEBUG, "Read %d bytes from file descriptor", v);
				if( v < 0 ) {
					october_panic(THREADERRSYS, "worker thread read error");
				} else {
					t_args->writeindex += v;
					assert(t_args->writeindex <= BUFFSIZE);
					if( (v = write(t_args->conn_fd, t_args->writebuff, t_args->writeindex)) < 0) {
						october_panic(THREADERRSYS, "connection write error");
					} else {
						october_log(LOGDEBUG, "wrote %d bytes on connection socket", v);
						if(t_args->writeindex == BUFFSIZE) {
							t_args->writeindex = 0;
						}
					}
				}
			} t_args->writeindex = 0;
		}

		if( (v = write(t_args->conn_fd, t_args->writebuff, t_args->writeindex)) < 0 || (v += write(t_args->conn_fd, "\n\r", 2)) < 0) {
			october_panic(THREADERRSYS, "connection write error");
		} else {
			october_log(LOGDEBUG, "wrote %d bytes on connection socket", v);
		}

	/* test for other request types */
	} else if ( ( strcmp(HEAD, request.method) == 0 ) ||
				( strcmp(OPTIONS, request.method) == 0 ) ||
				( strcmp(POST, request.method) == 0 ) ||
				( strcmp(PUT, request.method) == 0 ) ) {
				/* make sure this isn't a malformed request too */
		october_panic(THREADERRPROG, "application does not support POST, HEAD, OPTIONS or PUT");
	} else {
		october_panic(THREADERRPROG, "malformed request:\n%s", t_args->readbuff);
	}

	pthread_cleanup_pop(1);
	pthread_exit(NULL);
}

char* october_detect_type(char* filename) {
	char *c;
	if( (c = strrchr(filename, '.')) == NULL || strcmp(c, ".txt") == 0 ) {
		return MIME_TXT;
	} else if( strcmp(c, ".html") == 0 || strcmp(c, ".htm") == 0) {
		return MIME_HTML;
	} else if( strcmp(c, ".jpeg") == 0 || strcmp(c, ".jpg") == 0) {
		return MIME_JPG;
	} else if( strcmp(c, ".gif") == 0 ) {
		return MIME_GIF;
	} else if( strcmp(c, ".png") == 0 ) {
		return MIME_PNG;
	} else if( strcmp(c, ".css") == 0 ) {
		return MIME_CSS;
	} else if( strcmp(c, ".js") == 0 ) {
		return MIME_JS;
	} else {
		return MIME_TXT;
	}
}

void october_worker_cleanup(threadargs_t *t_args) {
	if(close(t_args->conn_fd) < 0) {
		october_panic(ERRSYS, "connection socket close error");
	} else {
		october_log(LOGINFO, "connection socket closed");
	}

	free(t_args);
}

/* thread & process error handler */

void october_panic(int error, const char* message, ...) {
	if( error % 10 ? LOGPANIC : LOGERR <= log_level) {
		switch(error) {
			case ERRPROG:
			case THREADERRPROG:
				printf("Program error: ");
				break;
			case ERRSYS:
			case THREADERRSYS:
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
	if(error % 10) {
		exit(error);
	} else {
		pthread_exit(NULL);
	}
}

/* log stuff */
void october_log(int err_level, const char* message, ...) {
	if(err_level <= log_level) {
		char buff[BUFFSIZE];
		snprintf(buff, BUFFSIZE, "%u:\t", (unsigned int) pthread_self());
		va_list arglist;
		va_start(arglist, message);
		vsprintf(&(buff[strlen(buff)]), message, arglist);
		va_end(arglist);
		printf("%s\n", buff);
	}
}