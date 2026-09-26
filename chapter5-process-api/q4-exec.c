// Chapter 5, Question 4
// Every exec() variant, one per child process.
//   l / v : arguments given as a list vs. a vector (argv array)
//   p     : search $PATH for the program name
//   e     : pass an explicit environment instead of inheriting environ

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

extern char **environ;

static void run(const char *label, int which) {
    int rc = fork();
    if (rc < 0) {
        perror("fork");
        exit(1);
    }
    if (rc == 0) {
        printf("---- %s ----\n", label);
        fflush(stdout);

        char *argv[] = {"ls", "-1", NULL};
        char *envp[] = {"PATH=/bin:/usr/bin", "MY_VAR=ostep", NULL};

        switch (which) {
        case 0: execl("/bin/ls", "ls", "-1", (char *) NULL);            break;
        case 1: execle("/bin/ls", "ls", "-1", (char *) NULL, envp);     break;
        case 2: execlp("ls", "ls", "-1", (char *) NULL);                break;
        case 3: execv("/bin/ls", argv);                                 break;
        case 4: execvp("ls", argv);                                     break;
        case 5: execvpe("ls", argv, envp);                              break;
        }
        perror("exec");                 // only reached if exec failed
        exit(1);
    }
    wait(NULL);
}

int main(void) {
    const char *names[] = {"execl", "execle", "execlp", "execv", "execvp", "execvpe"};
    for (int i = 0; i < 6; i++)
        run(names[i], i);
    return 0;
}
