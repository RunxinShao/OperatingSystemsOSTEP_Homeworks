CC     = gcc
CFLAGS = -Wall -Wextra -Wno-unused-parameter -O2 -pthread

PROGS = fork-join rendezvous barrier reader-writer reader-writer-nostarve \
        reader-writer-plain-turnstile mutex-nostarve

all: $(PROGS)

%: %.c ../common_threads.h rw-common.h ns-mutex.h
	$(CC) $(CFLAGS) -o $@ $<

reader-writer-plain-turnstile: reader-writer-nostarve.c ../common_threads.h rw-common.h ns-mutex.h
	$(CC) $(CFLAGS) -DPLAIN_TURNSTILE -o $@ $<

test: all
	./test.sh

clean:
	rm -f $(PROGS)

.PHONY: all test clean
