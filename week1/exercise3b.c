#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Signal handler for SIGINT. Writes a message to stderr and exits.
void sigintHandler(__attribute__((unused)) int signum)
{
    char* message = "SIGINT received\n";
    write(STDERR_FILENO, message, strlen(message));
    exit(0);
}

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
    // Creating signal action. Initialize sa_mask to empty set to allow other signals to interrupt the handler.
    struct sigaction action;
    if (sigemptyset(&action.sa_mask) < 0)
    {
        perror("Failed to set signal mask");
        return 1;
    }
    // Set the handler function and empty flags.
    action.sa_handler = sigintHandler;
    action.sa_flags = 0;

    // Apply the action to SIGINT. If it fails, print an error and exit.
    if (sigaction(SIGINT, &action, NULL) < 0)
    {
        perror("Failed to set SIGINT handler");
        return 1;
    }

    int file;

    // If a file is specified, open it. Otherwise, use stdin. (I don't think this was in the exercise, but it added it because why
    // not and it makes testing blocking read easy.)
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

    // Keep reading max 1000 characters to "data" until EOF is reached.
    // Each read is written fully to stdout immediately.
    char data[1000];
    ssize_t charactersRead;
    while ((charactersRead = read(file, data, sizeof(data))) > 0)
    {
        if (loopedWrite(STDOUT_FILENO, data, charactersRead) < 0)
        {
            perror("Failed to write to stdout");
            goto errorExit;
        }
    }
    // Check if EOF was actually reached or if an error occurred.
    if (charactersRead < 0)
    {
        perror("Failed to read file");
        return 1;
    }

    // If the file is not stdin, close it.
    if (file != STDIN_FILENO)
    {
        if (close(file) < 0)
        {
            perror("Failed to close file");
            return 1;
        }
    }

    return 0;

    // If an error occurred, close the file if it is not stdin and return 1.
errorExit:
    if (file != STDIN_FILENO)
    {
        if (close(file) < 0)
        {
            perror("Failed to close file");
            return 1;
        }
    }
    return 1;
}
