#include <stdlib.h>

int main(void) {
    int *data = malloc(100 * sizeof(int));
    free(data + 50);        // pointer into the middle of the block
    return 0;
}
