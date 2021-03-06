/*

Midnight worker thread

(c) 2012 Kyle J Aleshire
All rights reserved

*/

#include "october.h"

void oct_worker_thread(threadargs_t *t_args) {

    reqargs_t *request = t_args->request;
    /* request tokenizer pointers */
    char *token, *nexttoken, *line;

    oct_log(LOGINFO, "spawned to handle %s",  inet_ntoa(t_args->conn_info.sin_addr));

    pthread_cleanup_push( (void (*) (void *)) oct_worker_cleanup, t_args);

    /* initialize our buffer indices & flags */
    t_args->readindex = 0;
    t_args->writeindex = 0;
    request->conn_flags = 0x00000000;

    /* read the request into the request buffer. */
    if ( (t_args->readindex = read(t_args->conn_fd, t_args->readbuff, BUFFSIZE - t_args->readindex)) < 0) {
        oct_panic(THREADERRSYS, "failed to read request");
    } else {
        assert( t_args->readindex <= BUFFSIZE );
        t_args->readbuff[t_args->readindex] = '\0';
        oct_log(LOGDEBUG, "connection read %d bytes as request: %s", t_args->readindex, t_args->readbuff);
    }

    /* we have the request string, read the type of request into the request buffer now for comparison.
       For now only GET is supported, other types to come later. */

    /* begin tokenizing the read buffer. */
    line = t_args->readbuff;

    if((nexttoken = strsep(&line, "\r")) == NULL || (token = strsep(&nexttoken, " ")) == NULL) {
        oct_panic(THREADERRPROG, "no request received");
    }

    /* test for GET request */
    if( strcmp(GET, token) == 0){
        request->conn_flags |= GET_F;

        if( (request->file = strsep(&nexttoken, " ")) == NULL) {
            oct_log(THREADERRPROG, "no filename received, assuming \"/\"");
            request->file = "/";
        } else {
            oct_log(LOGDEBUG, "file %s requested", request->file);
        }

        if( (token = strsep(&nexttoken, " ")) == NULL ) {
            oct_log(LOGDEBUG, "no protocol requested, assuming HTTP/1.0");
        } else {
            if( strcmp(HTTP11_H, token) == 0 ) {
                request->conn_flags |= HTTP11_F;
            }
            oct_log(LOGDEBUG, "protocol %s requested", token);
        }
    /* test for other request types */
    } else if( strcmp(HEAD, token) == 0 ) {
        request->conn_flags |= HEAD_F;
    } else if( strcmp(OPTIONS, token) == 0 ) {
        request->conn_flags |= OPTIONS_F;
    } else if( strcmp(POST, token) == 0 ) {
        request->conn_flags |= POST_F;
    } else if( strcmp(PUT, token) == 0 ) {
        request->conn_flags |= PUT_F;
    }

    /* having tokenized the request method line, we can start pulling the headers we (may) need */
    while( (nexttoken = strsep(&line, CRLF)) != NULL ) {
        token = strsep(&nexttoken, " ");
        if ( strcmp(HOST_H, token) == 0 ) {
            request->conn_flags |= HOST_F;
            oct_log(LOGDEBUG, "Host header found: %s", strsep(&nexttoken, " "));
        } else if ( strcmp(CONNECTION_H, token)  == 0 ) {
            oct_log(LOGDEBUG, "Connection header found");
            if( strcmp(KEEPALIVE_H, strsep(&nexttoken, " ")) == 0 && (request->conn_flags | HTTP11_F) ) {
                request->conn_flags |= CONNECTION_F;
                oct_log(LOGDEBUG, "keep-alive set");
            }
        }
    }

    if(GET_F & request->conn_flags) {
        oct_get_handler(request, t_args);
    } else if ( request->conn_flags & OPTIONS_F ||
                request->conn_flags & HEAD_F ||
                request->conn_flags & POST_F ||
                request->conn_flags & PUT_F ) {
        oct_log(LOGDEBUG, "application does not support POST, HEAD, OPTIONS or PUT");
    } else {
        oct_panic(THREADERRPROG, "malformed request method:\n%s", t_args->readbuff);
    }

    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}

void oct_get_handler(reqargs_t *request, threadargs_t *t_args) {
    int v; char *c;
    int file_fd;
    time_t ticks;

    /* strip out encoded spaces, e.g. '%20' */
        c = request->file;
        while( (c = strchr(c, '%')) != NULL ) {
            if( strncmp(c, "%20", 3) == 0) {
                *c = ' ';
                strcpy(request->scratchbuff, &(c[3]));
                strcpy(&(c[1]), request->scratchbuff);
                c = &(c[1]);
            } else if ( strncmp(c, "%34", 3) == 0) {
                *c = '"';
                strcpy(request->scratchbuff, &(c[3]));
                strcpy(&(c[1]), request->scratchbuff);
                c = &(c[1]);
            } else if ( strncmp(c, "%60", 3) == 0) {
                *c = '<';
                strcpy(request->scratchbuff, &(c[3]));
                strcpy(&(c[1]), request->scratchbuff);
                c = &(c[1]);
            } else if ( strncmp(c, "%62", 3) == 0) {
                *c = '>';
                strcpy(request->scratchbuff, &(c[3]));
                strcpy(&(c[1]), request->scratchbuff);
                c = &(c[1]);
            } else if ( strncmp(c, "%35", 3) == 0) {
                *c = '#';
                strcpy(request->scratchbuff, &(c[3]));
                strcpy(&(c[1]), request->scratchbuff);
                c = &(c[1]);
            } else if ( strncmp(c, "%37", 3) == 0) {
                *c = '%';
                strcpy(request->scratchbuff, &(c[3]));
                strcpy(&(c[1]), request->scratchbuff);
                c = &(c[1]);
            }

            c = &(c[1]);
        }

        /* if the filename ends with '/', assume they're asking for the default file and append our default filename */
        if( strcmp(&(request->file[strlen(request->file) - 1]), "/") == 0) {
            snprintf(t_args->writebuff, BUFFSIZE, "%s%s%s", DOCROOT, request->file, DEFAULTFILE);
        } else {
            snprintf(t_args->writebuff, BUFFSIZE, "%s%s", DOCROOT, request->file);
        }

        ticks = time(NULL);
        assert(t_args->writeindex == 0);

        oct_log(LOGDEBUG, "detecting if file exists: %s", t_args->writebuff);
        if( stat(t_args->writebuff, NULL) < 0 && errno == ENOENT ) {
            oct_log(LOGINFO, "404 File not Found: %s", t_args->writebuff);

            t_args->writeindex += snprintf(&(t_args->writebuff[t_args->writeindex]), BUFFSIZE, "%s ", request->conn_flags | HTTP11_F ? HTTP11_H : HTTP10_H);
            t_args->writeindex += snprintf(&(t_args->writebuff[t_args->writeindex]), BUFFSIZE, "%s%s", NOTFOUND_R, CRLF);
            t_args->writeindex += snprintf(&(t_args->writebuff[t_args->writeindex]), BUFFSIZE, "%s%s%s%s", CONTENT_T_H, MIME_HTML, CHARSET, CRLF);
            t_args->writeindex += snprintf(&(t_args->writebuff[t_args->writeindex]), BUFFSIZE, "%s%s%s%s", DATE_H, ctime(&ticks), CRLF, CRLF);
            t_args->writeindex += snprintf(&(t_args->writebuff[t_args->writeindex]), BUFFSIZE, "%s%s", NOTFOUNDHTML, CRLF);

            assert(t_args->writeindex <= BUFFSIZE);
        } else {
            oct_log(LOGINFO, "200 OK: %s", t_args->writebuff);
            if( (file_fd = open(t_args->writebuff, O_RDONLY)) < 0 ){
                oct_panic(THREADERRSYS, "error opening file: %s", t_args->writebuff);
            } else {
                oct_log(LOGDEBUG, "file opened: %s", t_args->writebuff);
            }
            request->mimetype = oct_detect_type(t_args->writebuff);

            t_args->writeindex += snprintf(&(t_args->writebuff[t_args->writeindex]), BUFFSIZE, "%s ", request->conn_flags | HTTP11_F ? HTTP11_H : HTTP10_H);
            t_args->writeindex += snprintf(&(t_args->writebuff[t_args->writeindex]), BUFFSIZE, "%s%s", OK_R, CRLF);
            t_args->writeindex += snprintf(&(t_args->writebuff[t_args->writeindex]), BUFFSIZE, "%s%s%s%s", CONTENT_T_H, request->mimetype, CHARSET, CRLF);
            t_args->writeindex += snprintf(&(t_args->writebuff[t_args->writeindex]), BUFFSIZE, "%s%.24s%s", DATE_H, ctime(&ticks), CRLF);
            t_args->writeindex += snprintf(&(t_args->writebuff[t_args->writeindex]), BUFFSIZE, "%s%s%s%s%s", EXPIRES_H, CRLF, SERVER_H, CRLF, CRLF);

            assert(t_args->writeindex <= BUFFSIZE);

            while( (v = read(file_fd, &(t_args->writebuff[t_args->writeindex]), BUFFSIZE - t_args->writeindex)) != 0 ) {
                oct_log(LOGDEBUG, "read %d bytes from file descriptor", v);
                if( v < 0 ) {
                    oct_panic(THREADERRSYS, "worker thread read error");
                } else {
                    t_args->writeindex += v;
                    assert(t_args->writeindex <= BUFFSIZE);
                    if( (v = write(t_args->conn_fd, t_args->writebuff, t_args->writeindex)) < 0) {
                        oct_panic(THREADERRSYS, "connection write error");
                    } else {
                        oct_log(LOGDEBUG, "wrote %d bytes on connection socket", v);
                        if(t_args->writeindex == BUFFSIZE) {
                            t_args->writeindex = 0;
                        }
                    }
                }
            } t_args->writeindex = 0;
        }

        if( (v = write(t_args->conn_fd, t_args->writebuff, t_args->writeindex)) < 0 || (v += write(t_args->conn_fd, CRLF, 2)) < 0) {
            oct_panic(THREADERRSYS, "connection write error");
        } else {
            oct_log(LOGDEBUG, "wrote %d bytes on connection socket", v);
        }
}

char* oct_detect_type(char* filename) {
    char *c;
    if( (c = strrchr(filename, '.')) == NULL || strcmp(c, ".txt") == 0 ) {
        c = MIME_TXT;
    } else if( strcmp(c, ".html") == 0 || strcmp(c, ".htm") == 0) {
        c = MIME_HTML;
    } else if( strcmp(c, ".jpeg") == 0 || strcmp(c, ".jpg") == 0) {
        c = MIME_JPG;
    } else if( strcmp(c, ".gif") == 0 ) {
        c = MIME_GIF;
    } else if( strcmp(c, ".png") == 0 ) {
        c = MIME_PNG;
    } else if( strcmp(c, ".css") == 0 ) {
        c = MIME_CSS;
    } else if( strcmp(c, ".js") == 0 ) {
        c = MIME_JS;
    } else {
        c = MIME_TXT;
    }
    oct_log(LOGDEBUG, "detected MIME type: %s", c);
    return c;
}

void oct_worker_cleanup(threadargs_t *t_args) {
    if(close(t_args->conn_fd) < 0) {
        oct_panic(ERRSYS, "connection socket close error");
    } else {
        oct_log(LOGINFO, "connection socket closed");
    }

    free(t_args->request);
    free(t_args);
}