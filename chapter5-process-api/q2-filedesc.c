// Chapter 5, Question 2
// The child inherits a copy of the file descriptor table, and the copied
// descriptor points at the *same* open file description -- so parent and
// child share one file offset.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

int main(void) {
    int fd = open("./q2-output.txt", O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        perror("open");
        exit(1);
    }
    printf("parent opened fd = %d\n", fd);
    fflush(stdout);                 // or the buffered line is duplicated into the child

    int rc = fork();
    if (rc < 0) {
        perror("fork");
        exit(1);
    }

    const char *who = (rc == 0) ? "child " : "parent";
    for (int i = 0; i < 5; i++) {
        char buf[64];
        int n = snprintf(buf, sizeof(buf), "%s (pid:%d) line %d\n", who, getpid(), i);
        write(fd, buf, n);              // same offset is shared, so no overwriting
    }

    if (rc == 0) {
        printf("child  (pid:%d) wrote through the inherited fd %d\n", getpid(), fd);
    } else {
        wait(NULL);
        printf("parent (pid:%d) done; see q2-output.txt\n", getpid());
    }
    close(fd);
    return 0;
}
