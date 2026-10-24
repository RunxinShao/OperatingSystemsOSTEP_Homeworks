CC     = gcc
CFLAGS = -Wall -Wextra -O2

PROGS = timer tlb init-options

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

# make data LABEL=vm-orbstack-linux   (see collect.sh)
data: all
	./collect.sh $(LABEL)
	python3 plot.py

clean:
	rm -f $(PROGS)

.PHONY: all data clean
