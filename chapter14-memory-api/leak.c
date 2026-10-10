#include <stdlib.h>

int main(void) {
    int *p = malloc(10 * sizeof(int));
    p[0] = 1;
    return 0;               // forgot free(p)
}
