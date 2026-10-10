#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int *data = malloc(100 * sizeof(int));
    for (int i = 0; i < 100; i++)
        data[i] = i;
    free(data);
    printf("data[5] = %d\n", data[5]);   // read freed memory
    return 0;
}
