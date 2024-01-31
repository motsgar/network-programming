#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define FIFO_TO_CONVERTER "/tmp/npfifo.1"
#define FIFO_FROM_CONVERTER "/tmp/npfifo.2"

// I optionally added a signal handler for SIGPIPE.
void sigpileHandler(__attribute__((unused)) int signum)
{
    char* message = "Recieved SIGPIPE from converter indicating that it can't recieve data anymore.\n";
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

int main(__attribute__((unused)) int argc, __attribute__((unused)) char* argv[])
{
    // Register additional signal handler.
    struct sigaction action;
    if (sigemptyset(&action.sa_mask) < 0)
    {
        perror("Failed to set signal mask");
        return 1;
    }
    action.sa_handler = sigpileHandler;
    action.sa_flags = 0;

    // Apply the action to SIGPIPE. If it fails, print an error and exit.
    if (sigaction(SIGPIPE, &action, NULL) < 0)
    {
        perror("Failed to set SIGPIPE handler");
        return 1;
    }

    int receivefd, sendfd;

    // Open the FIFOs for sending and recieving.
    fprintf(stderr, "Opening send FIFO\n");
    if ((sendfd = open(FIFO_TO_CONVERTER, O_WRONLY)) < 0)
    {
        perror("Failed to open send FIFO");
        return 1;
    }
    fprintf(stderr, "Opening recieve FIFO\n");
    if ((receivefd = open(FIFO_FROM_CONVERTER, O_RDONLY)) < 0)
    {
        perror("Failed to open receive FIFO");
        return 1;
    }

    // Loop that reads from stdin and writes to the converter. After writing, it reads from the converter
    // the same amount of bytes that it wrote and writes them to stdout.
    char requestMessage[100];
    ssize_t charactersRead;
    while ((charactersRead = read(STDIN_FILENO, requestMessage, sizeof(requestMessage))) > 0)
    {
        if (loopedWrite(sendfd, requestMessage, charactersRead) < 0)
        {
            perror("Failed to write to send FIFO");
            return 1;
        }

        // Read exactly charactersRead bytes from input and write them to stdout.
        // The protocol guarantees that the reader will write exactly charactersRead bytes to the input FIFO.
        char responseMessage[100];
        ssize_t charactersReadFromResponse = 0;
        while (charactersReadFromResponse < charactersRead)
        {
            // Read from the receive FIFO to response buffer with offset of how many bytes have already been read.
            ssize_t readResult = read(receivefd, responseMessage + charactersReadFromResponse, charactersRead - charactersReadFromResponse);
            if (readResult == 0)
            {
                perror("Reached EOF from receive FIFO before reading all bytes of response");
                return 1;
            }
            if (readResult < 0)
            {
                perror("Failed to read from receive FIFO");
                return 1;
            }
            if (loopedWrite(STDOUT_FILENO, responseMessage, readResult) < 0)
            {
                perror("Failed to write to stdout");
                return 1;
            }

            // Update the number of bytes read from the response FIFO.
            charactersReadFromResponse += readResult;
        }
    }
    // Check if EOF was actually reached or if an error occurred.
    if (charactersRead < 0)
    {
        perror("Failed to read from stdin");
        return 1;
    }

    fprintf(stderr, "Stdin reached EOF\n");

    // Close the FIFOs.
    if (close(sendfd) < 0)
    {
        perror("Failed to close send FIFO");
        return 1;
    }
    if (close(receivefd) < 0)
    {
        perror("Failed to close receive FIFO");
        return 1;
    }

    return 0;
}