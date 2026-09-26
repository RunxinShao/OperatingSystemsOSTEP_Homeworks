// Chapter 5, Question 1
// A variable set before fork() is visible in the child, but each process
// gets its own private copy of the address space: changes do not propagate.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    int x = 100;
    printf("parent (pid:%d) before fork: x = %d (&x = %p)\n", getpid(), x, (void *) &x);
    fflush(stdout);                 // or the buffered line is duplicated into the child

    int rc = fork();
    if (rc < 0) {
        fprintf(stderr, "fork failed\n");
        exit(1);
    } else if (rc == 0) {
        printf("child  (pid:%d) sees x = %d (&x = %p)\n", getpid(), x, (void *) &x);
        x = 200;
        printf("child  (pid:%d) set  x = %d\n", getpid(), x);
        sleep(1);
        printf("child  (pid:%d) after parent's change, x = %d\n", getpid(), x);
    } else {
        sleep(1);                       // let the child write first
        printf("parent (pid:%d) still sees x = %d\n", getpid(), x);
        x = 300;
        printf("parent (pid:%d) set  x = %d\n", getpid(), x);
        wait(NULL);
    }
    return 0;
}
