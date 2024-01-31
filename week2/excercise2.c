#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

// Executes the given path with the given argument.
void do_child(char *path, char *arg)
{
    char *argv[3];
    argv[0] = path;
    argv[1] = arg;
    argv[2] = NULL;

    execve(path, argv, environ);
}

int main(int argc, char *argv[])
{
    // Check that the correct amount of arguments were given.
    if (argc != 2 && argc != 3)
    {
        fprintf(stderr, "Usage: %s <command> [arg]\n", argv[0]);
        return 1;
    }

    // Read the command and argument from the arguments.
    char *command = argv[1];
    char *arg = NULL;
    if (argc == 3)
        arg = argv[2];

    pid_t forkResult;
    // Fork and run do_child in the child process.
    if ((forkResult = fork()) < 0)
    {
        perror("fork failure");
        return 1;
    }
    else if (forkResult == 0)
    {
        do_child(command, arg);
        // If execve returns, it failed.
        perror("execve failed");
        return 1;
    }
    // I interpreted the assignment as not requiring the loop printing as it would
    // also be kind of weird considering that this program seems more like a exit status printer.

    // Wait for the child process to exit and print its exit status.
    pid_t pid;
    int stat;
    pid = waitpid(-1, &stat, 0);
    if (pid < 0)
    {
        perror("waitpid failed");
        return 1;
    }
    printf("Child %d exited with status %d\n", pid, WEXITSTATUS(stat));
}
