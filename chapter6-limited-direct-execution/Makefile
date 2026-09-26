CC     = gcc
CFLAGS = -Wall -Wextra -O2

PROGS = timer-precision syscall-cost context-switch

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	./timer-precision
	@echo
	./syscall-cost
	@echo
	./context-switch

clean:
	rm -f $(PROGS)

.PHONY: all run clean
