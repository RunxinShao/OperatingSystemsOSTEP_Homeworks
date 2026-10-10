CC     = gcc
CFLAGS = -Wall -O0 -g

PROGS = null leak overflow use-after-free bad-free vector

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGS)

.PHONY: all clean
