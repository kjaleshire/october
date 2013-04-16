CC=clang
CFLAGS=

all: october

october: october.c october_worker.c october_logging.c
	$(CC) $(CFLAGS) october.c october_worker.c october_logging.c -o october

clean:
	rm october