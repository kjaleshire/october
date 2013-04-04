/*

Simple threaded HTTP server. Read GET replies from a host and respond appropriately.

Logging & panic facilities

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

/* error handler. whole program dies on main thread panic, worker dies on worker thread panic. */
void oct_panic(int error, const char* message, ...) {
    if( (error % 10 ? LOGPANIC : LOGERR) <= log_level) {
        if (pthread_mutex_lock(&mtx_term) != 0 ) {
            printf("%u:\terror acquiring terminal mutex", (unsigned int) pthread_self());
            exit(ERRSYS);
        }
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
        printf("%u:\t", (unsigned int) pthread_self());
        va_list arglist;
        va_start(arglist, message);
        vfprintf(log_fd, message, arglist);
        printf("\n");
        va_end(arglist);
        if (pthread_mutex_unlock(&mtx_term) != 0 ) {
            printf("%u:\terror releasing terminal mutex", (unsigned int) pthread_self());
            exit(ERRSYS);
        }
    }
    if(error % 10) {
        exit(error);
    } else {
        pthread_exit(NULL);
    }
}

/* log stuff */
void oct_log(int err_level, const char* message, ...) {
    if(err_level <= log_level) {
        if (pthread_mutex_lock(&mtx_term) != 0 ) {
            oct_panic(ERRSYS, "error acquiring terminal mutex");
        }
        printf("%u:\t", (unsigned int) pthread_self());
        va_list arglist;
        va_start(arglist, message);
        vfprintf(log_fd, message, arglist);
        va_end(arglist);
        printf("\n");
        if (pthread_mutex_unlock(&mtx_term) != 0 ) {
            oct_panic(ERRSYS, "error releasing terminal mutex");
        }
    }
}