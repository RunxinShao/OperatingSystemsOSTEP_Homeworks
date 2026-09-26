// Chapter 5, Question 5
// wait() returns the pid of the reaped child, or -1 with errno == ECHILD
// when the caller has no children at all (which is the case in the child).

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    printf("parent (pid:%d) starting\n", getpid());
    fflush(stdout);                 // or the buffered line is duplicated into the child

    int rc = fork();
    if (rc < 0) {
        perror("fork");
        exit(1);
    } else if (rc == 0) {
        printf("child  (pid:%d) running\n", getpid());
        int wc = wait(NULL);            // the child has no children of its own
        printf("child  (pid:%d) wait() returned %d, errno = %d (%s)\n",
               getpid(), wc, errno, strerror(errno));
        exit(42);
    } else {
        int status;
        int wc = wait(&status);
        printf("parent (pid:%d) wait() returned %d (child was %d)\n", getpid(), wc, rc);
        if (WIFEXITED(status))
            printf("parent: child exited normally with status %d\n", WEXITSTATUS(status));
    }
    return 0;
}
