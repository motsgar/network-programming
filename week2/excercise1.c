#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

// Writes "length" bytes from "data" to "file" and returns the number of bytes written or -1 if an error occurred.
// Implemented because write does not guarantee to write all bytes if the output file is eg. full or a signal causes the write to be interrupted.
ssize_t loopedWrite(int file, void* data, size_t length)
{
    ssize_t writeResult;
    size_t charactersWritten = 0;
    while ((writeResult = write(file, data + charactersWritten, length - charactersWritten)) >= 0)
    {
        charactersWritten += writeResult;
        if (charactersWritten == length)
            break;
    }
    return writeResult;
}

int main(int argc, char* argv[])
{
    int processesToCreate = 5;

    if (argc > 1)
    {
        char* endPtr;
        size_t argumentLength = strlen(argv[1]);
        long parsedProcessesToCreate = strtol(argv[1], &endPtr, 10);
        if (endPtr != argv[1] + argumentLength)
        {
            fprintf(stderr, "Argument must be a number between 1 and 50\n");
            return 1;
        }
        else if (parsedProcessesToCreate < 1)
        {
            fprintf(stderr, "Amount of processes must be a positive integer\n");
            return 1;
        }
        else if (parsedProcessesToCreate > 50)
        {
            fprintf(stderr, "Amount of processes must be at most 50\n");
            return 1;
        }
        processesToCreate = parsedProcessesToCreate;
    }

    pid_t forkResult;
    int forkId;
    for (forkId = 0; forkId < processesToCreate - 1; forkId++)
    {
        if ((forkResult = fork()) < 0)
        {
            perror("Failed to fork");
            return 1;
        }
        else if (forkResult == 0)
        {
            break;
        }
    }
    char* message;
    if (forkResult == 0)
    {
        message = "";
    }
    else
    {
        message = " (Original process)";
    }

    for (int i = 1; i <= 5; i++)
    {
        char buffer[50];
        // snprintf used to avoid (in this case) very theoretical buffer overflow.
        int charactersWritten = snprintf(buffer, sizeof(buffer), "ID: %-2d i = %d%s\n", forkId, i, message);
        if (charactersWritten < 0)
        {
            perror("Failed to format string");
            return 1;
        }
        else if ((size_t)charactersWritten >= sizeof(buffer))
        {
            fprintf(stderr, "Buffer overflow\n");
            return 1;
        }

        if (loopedWrite(STDOUT_FILENO, buffer, strlen(buffer)) < 0)
        {
            perror("Failed to write");
            return 1;
        }
    }

    exit(0);
}