CC     = gcc
CFLAGS = -Wall -Wextra -O2

PROGS = timer tlb

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

data: all
	mkdir -p data
	for r in 1 2 3; do \
		./run.sh 32768 100000000       > data/stride-$$r.dat; \
		./run.sh 32768 100000000 -s    > data/spread-$$r.dat; \
		./run.sh 32768 30000000  -s -d > data/chase-$$r.dat; \
	done
	python3 plot.py

clean:
	rm -f $(PROGS)

.PHONY: all data clean
