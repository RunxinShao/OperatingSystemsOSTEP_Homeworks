// Chapter 5, Question 6
// waitpid() lets the parent name *which* child to wait for, and can poll
// without blocking via WNOHANG -- neither is possible with plain wait().

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    int pids[3];
    for (int i = 0; i < 3; i++) {
        int rc = fork();
        if (rc < 0) {
            perror("fork");
            exit(1);
        }
        if (rc == 0) {
            sleep(3 - i);               // child 0 is the slowest to finish
            printf("child %d (pid:%d) exiting\n", i, getpid());
            exit(i);
        }
        pids[i] = rc;
    }

    // WNOHANG polls: it returns 0 at once because no child has exited yet,
    // where plain wait() would have blocked here.
    int status;
    printf("parent: waitpid(-1, WNOHANG) = %d (nobody has exited yet)\n",
           waitpid(-1, &status, WNOHANG));
    fflush(stdout);

    // Reap in creation order, not completion order: only waitpid() can do this.
    for (int i = 0; i < 3; i++) {
        int wc = waitpid(pids[i], &status, 0);
        printf("parent: waitpid(%d) returned %d, exit status %d\n",
               pids[i], wc, WEXITSTATUS(status));
    }
    return 0;
}
