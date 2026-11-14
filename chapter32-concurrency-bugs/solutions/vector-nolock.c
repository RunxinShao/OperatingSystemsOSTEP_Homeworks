#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "common_threads.h"

#include "main-header.h"
#include "vector-header.h"

// Port of ../vector-nolock.c. The original uses x86 inline asm
// (lock; xaddl), which does not assemble on aarch64. On non-x86 targets
// use the GCC builtin instead; on ARMv8.1+ with LSE it becomes `ldadd`.
#if defined(__x86_64__)
// taken from https://en.wikipedia.org/wiki/Fetch-and-add
int fetch_and_add(int * variable, int value) {
    asm volatile("lock; xaddl %%eax, %2;"
		 :"=a" (value)                  
		 :"a" (value), "m" (*variable)  
		 :"memory");
    return value;
}
#else
int fetch_and_add(int * variable, int value) {
    return __atomic_fetch_add(variable, value, __ATOMIC_SEQ_CST);
}
#endif

void vector_add(vector_t *v_dst, vector_t *v_src) {
    int i;
    for (i = 0; i < VECTOR_SIZE; i++) {
	fetch_and_add(&v_dst->values[i], v_src->values[i]);
    }
}

void fini() {}


#include "main-common.c"
