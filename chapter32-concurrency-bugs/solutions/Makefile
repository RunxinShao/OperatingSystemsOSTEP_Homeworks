# Builds all five variants into solutions/. Four come unchanged from the
# upstream sources in ..; vector-nolock comes from the aarch64 port here.
ALL = vector-deadlock vector-global-order vector-try-wait \
      vector-avoid-hold-and-wait vector-nolock
COMMON = ../vector-header.h ../main-common.c ../main-header.h ../common.h \
         ../common_threads.h
CFLAGS = -Wall -pthread -O -I..

HELPERS = self-add-deadlock self-add-global-order check-atomic-nolock \
          check-atomic-global-order check-atomic-try-wait \
          check-atomic-avoid-hold-and-wait

all: $(ALL) $(HELPERS)

clean:
	rm -f $(ALL) $(HELPERS) *~

vector-deadlock vector-global-order vector-try-wait vector-avoid-hold-and-wait: %: ../%.c $(COMMON)
	gcc -o $@ $< $(CFLAGS)

vector-nolock: vector-nolock.c $(COMMON)
	gcc -o $@ $< $(CFLAGS)

# helper: calls vector_add(&v[0], &v[0]) with a given variant (question 4)
self-add-deadlock: self-add.c ../vector-deadlock.c $(COMMON)
	gcc -o $@ $< $(CFLAGS) -DVARIANT='"../vector-deadlock.c"'

self-add-global-order: self-add.c ../vector-global-order.c $(COMMON)
	gcc -o $@ $< $(CFLAGS) -DVARIANT='"../vector-global-order.c"'

# helper: checks whether vector_add() is atomic as a whole (question 9)
check-atomic-%: check-atomic.c $(COMMON)
	gcc -o $@ $< $(CFLAGS) -DVARIANT='"$(if $(filter nolock,$*),,../)vector-$*.c"'
