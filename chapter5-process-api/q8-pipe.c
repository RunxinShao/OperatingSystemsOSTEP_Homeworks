// Chapter 5, Question 8
// Two children joined by a pipe: child 1's stdout becomes child 2's stdin,
// which is exactly what the shell builds for "ls -1 | wc -l".

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    int fds[2];
    if (pipe(fds) < 0) {
        perror("pipe");
        exit(1);
    }

    int rc1 = fork();
    if (rc1 == 0) {                     // writer: stdout -> pipe
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        execlp("ls", "ls", "-1", (char *) NULL);
        perror("execlp");
        exit(1);
    }

    int rc2 = fork();
    if (rc2 == 0) {                     // reader: pipe -> stdin
        close(fds[1]);
        dup2(fds[0], STDIN_FILENO);
        close(fds[0]);
        execlp("wc", "wc", "-l", (char *) NULL);
        perror("execlp");
        exit(1);
    }

    // The parent must close both ends, or the reader never sees EOF.
    close(fds[0]);
    close(fds[1]);
    waitpid(rc1, NULL, 0);
    waitpid(rc2, NULL, 0);
    printf("parent: pipeline finished\n");
    return 0;
}
