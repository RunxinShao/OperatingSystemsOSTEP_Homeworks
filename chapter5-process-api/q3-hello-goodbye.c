// Chapter 5, Question 3
// Force the child to print first WITHOUT calling wait() in the parent.
// A pipe gives us the ordering: the parent blocks on read() until the child
// closes the write end, which happens only after the child has printed.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
    int fds[2];
    if (pipe(fds) < 0) {
        perror("pipe");
        exit(1);
    }

    int rc = fork();
    if (rc < 0) {
        perror("fork");
        exit(1);
    } else if (rc == 0) {
        close(fds[0]);
        printf("hello\n");
        fflush(stdout);                 // flush before signalling
        close(fds[1]);                  // EOF wakes the parent up
        exit(0);
    } else {
        close(fds[1]);
        char c;
        while (read(fds[0], &c, 1) > 0) // returns 0 only at EOF
            ;
        close(fds[0]);
        printf("goodbye\n");
    }
    return 0;
}
