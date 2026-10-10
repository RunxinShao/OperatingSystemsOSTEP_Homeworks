CC     = gcc
CFLAGS = -Wall -O0 -g

PROGS = memory-user

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGS)

.PHONY: all clean
