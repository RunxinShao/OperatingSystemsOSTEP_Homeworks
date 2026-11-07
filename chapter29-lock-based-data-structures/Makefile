CC     = gcc
CFLAGS = -Wall -Wextra -O2 -pthread

PROGS = timer counter approx list btree

all: $(PROGS)

%: %.c common.h
	$(CC) $(CFLAGS) -o $@ $<

# runs every experiment (about 10 minutes), then draws the graphs
data: all
	./collect.sh
	python3 plot.py

clean:
	rm -f $(PROGS)

.PHONY: all data clean
