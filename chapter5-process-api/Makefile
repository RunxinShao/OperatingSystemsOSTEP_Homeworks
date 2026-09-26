CC     = gcc
CFLAGS = -Wall -Wextra -O0 -g

PROGS = q1-variable q2-filedesc q3-hello-goodbye q4-exec \
        q5-wait q6-waitpid q7-close-stdout q8-pipe

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGS) q2-output.txt

.PHONY: all clean
