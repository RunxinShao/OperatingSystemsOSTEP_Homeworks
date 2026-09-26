// Chapter 5, Question 7
// After close(STDOUT_FILENO) the child's printf() silently produces nothing:
// stdio buffers the bytes and the write() at flush time fails with EBADF.

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    int rc = fork();
    if (rc < 0) {
        perror("fork");
        exit(1);
    } else if (rc == 0) {
        close(STDOUT_FILENO);
        int n = printf("child: can you see this?\n");
        int err = fflush(stdout);       // this is where the write() actually fails
        fprintf(stderr, "child (pid:%d): printf returned %d, fflush returned %d, errno = %d (%s)\n",
                getpid(), n, err, errno, strerror(errno));
        exit(0);
    } else {
        wait(NULL);
        printf("parent (pid:%d): my stdout still works fine\n", getpid());
    }
    return 0;
}
