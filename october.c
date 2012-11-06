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
	int v;
	char c; /* for miscellaneous values, only used very locally (usually in if(function()) idioms) */
	int alt_index;
	char* readbufftoken, *request;

	october_log(LOGINFO, "spawned new thread with ID %d to handle connection from %s", pthread_self(),  inet_ntoa(t_args->conn_info.sin_addr));

	pthread_cleanup_push( (void (*) (void *)) october_worker_cleanup, t_args);

	/* read the request into the request buffer. We call read() multiple times in case we don't get the
	   whole request in one packet (who knows?) */
	sleep(5);
	for(;;) {
		if( (v = read( t_args->conn_fd, &(t_args->readbuff[t_args->readindex]), BUFFSIZE - t_args->readindex )) < 0) {
			october_panic(THREADERRSYS, "connection request read error");
		} else {
			t_args->readindex += v;
		}
		if( v == 0 ) {
			t_args->readbuff[t_args->readindex] = '\0';
		} else if( strnstr(t_args->readbuff, CRLF, t_args->readindex) != NULL ) {
			t_args->readbuff[t_args->readindex] = 0;
			break;
		}
	} october_log(LOGDEBUG, "connection read %d bytes as request: %s", t_args->readindex + v, t_args->readbuff);

	/* we have the request string, read the type of request into the request buffer now for comparison.
	   For now only GET is supported, other types to come later. We can use use buff_index for both buffers
	   since this is the first token we're extracting */
	

	/* begin tokenizing the read buffer. */
	readbufftoken = t_args->readbuff;
	request = strsep(&readbufftoken, " ");

	
	if( strcmp(GET, request) == 0){
		october_log(LOGDEBUG, "GET found"); 	
	} else if ( ( strcmp(HEAD, t_args->readbuff) == 0 ) ||
				( strcmp(OPTIONS, t_args->readbuff) == 0 ) ||
				( strcmp(POST, t_args->readbuff) == 0 ) ||
				( strcmp(PUT, t_args->readbuff) == 0 ) ) {
				/* make sure this isn't a different kind of request, or malformed request. */
		october_panic(THREADERRPROG, "application does not support POST, HEAD, OPTIONS or PUT");
	} else {
		october_panic(THREADERRPROG, "malformed request:\n%s", t_args->readbuff);
	}

	pthread_cleanup_pop(1);
	pthread_exit(NULL);
}

void october_worker_cleanup(threadargs_t *t_args) {
	if(close(t_args->conn_fd) < 0) {
		october_panic(ERRSYS, "connection socket close error");
	} else {
		october_log(LOGINFO, "connection socket closed");
	}

	free(t_args);
}

/* thread panic, not mission-critical, like malformed request */

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
		va_list arglist;
		va_start(arglist, message);
		vfprintf(log_fd, message, arglist);
		printf("\n");
		va_end(arglist);
	}
}