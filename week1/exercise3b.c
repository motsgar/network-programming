#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct sigaction action;
void sigintHandler(__attribute__((unused)) int signum)
{
    char* message = "SIGINT received\n";
    write(STDERR_FILENO, message, strlen(message));
    exit(0);
}

int main(int argc, char* argv[])
{
    if (sigemptyset(&action.sa_mask) < 0)
    {
        perror("Failed to set signal mask");
        return 1;
    }
    action.sa_handler = sigintHandler;
    action.sa_flags = 0;

    if (sigaction(SIGINT, &action, NULL) < 0)
    {
        perror("Failed to set SIGINT handler");
        return 1;
    }

    int file;

    if (argc > 1)
    {
        file = open(argv[1], O_RDONLY);
        if (file < 0)
        {
            perror("Failed to open file");
            return 1;
        }
    }
    else
    {
        file = STDIN_FILENO;
    }

    char data[1000];
    ssize_t charactersRead;
    while ((charactersRead = read(file, data, sizeof(data))) > 0)
    {
        ssize_t writeResult;
        size_t charactersWritten = 0;
        while ((writeResult = write(STDOUT_FILENO, data, charactersRead)) >= 0)
        {
            charactersWritten += writeResult;
            if (charactersWritten == (size_t)charactersRead)
                break;
        }
        if (writeResult < 0)
        {
            perror("Failed to write to stdout");
            goto errorExit;
        }
    }
    if (charactersRead < 0)
    {
        perror("Failed to read file");
        return 1;
    }
    if (close(file) < 0)
    {
        perror("Failed to close file");
        return 1;
    }

    return 0;

errorExit:
    if (file != STDIN_FILENO)
    {
        if (close(file) < 0)
        {
            perror("Failed to close file");
            return 1;
        }
    }
    return -1;
}
