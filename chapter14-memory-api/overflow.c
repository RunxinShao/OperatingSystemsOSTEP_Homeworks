#include <stdlib.h>

int main(void) {
    int *data = malloc(100 * sizeof(int));
    data[100] = 0;          // one past the end
    free(data);
    return 0;
}
