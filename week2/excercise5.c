#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// I optionally added a signal handler for SIGPIPE.
void sigpileHandler(__attribute__((unused)) int signum)
{
    char* message = "Reader recieved SIGPIPE from doubler indicating that it can't recieve data anymore.\n";
    write(STDERR_FILENO, message, strlen(message));
    exit(0);
}

// Inclusively returns the number of characters until the next newline character.
// Returns -1 if no newline character is found in "length" bytes.
// Example: "abc\ndef\n" returns 4.
ssize_t charactersUntilNewline(char* string, size_t length)
{
    size_t count = 0;

    while (count < length && string[count] != '\n')
    {
        count++;
    }
    count++;

    return (count > length) ? -1 : (ssize_t)count;
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

int reader(int input, int output)
{
    // Read from input and write to output until EOF is reached.
    char data[100];
    ssize_t charactersRead;
    while ((charactersRead = read(input, data, sizeof(data))) > 0)
    {
        if (loopedWrite(output, data, charactersRead) < 0)
        {
            perror("Reader failed to write to output");
            return -1;
        }
    }
    // Check if EOF was actually reached or if an error occurred.
    if (charactersRead < 0)
    {
        perror("Reader failed to read from input");
        return -1;
    }
    return 0;
}

void lineDoubler(int input, int output)
{
    // Keep reading max 100 characters to "data" until EOF is reached.
    // Data is read to "data + readBufferOffset" to allow contiunation of reading a line
    // if the previous read ended without a newline.
    char data[100];
    ssize_t charactersRead;
    size_t readBufferOffset = 0;
    char shouldSkipUntilNewline = 0;
    while ((charactersRead = read(input, data + readBufferOffset, sizeof(data) - readBufferOffset)) > 0)
    {
        charactersRead += readBufferOffset;

        // Keep writing lines to stdout until the read buffer is empty.
        // If there is no newline in the read buffer, the start of the line is copied to the start of the buffer
        // and readBufferOffset is set to the number of characters copied for the next read to continue reading the line.
        size_t charactersWritten = 0;
        while (charactersWritten < (size_t)charactersRead)
        {
            // Find the number of characters until the next newline.
            // If no newline is found, break out of the loop to read more data.
            ssize_t count = charactersUntilNewline(data + charactersWritten, charactersRead - charactersWritten);
            if (count < 0)
                break;

            if (!shouldSkipUntilNewline)
                if (loopedWrite(output, data + charactersWritten, count) < 0 || loopedWrite(output, data + charactersWritten, count) < 0)
                {
                    perror("Doubler failed to write to output");
                    exit(1);
                }

            // As we got to the end of the line, we can stop skipping characters until the next newline.
            shouldSkipUntilNewline = 0;

            // Update the number of characters written.
            charactersWritten += count;
        }

        // If there were less characters written than read (because there was no newline), copy the remaining characters to the start
        // of the buffer and set readBufferOffset to the number of characters copied.
        // Otherwise, set readBufferOffset to 0 to indicate that the buffer is empty.
        if (charactersWritten < (size_t)charactersRead)
        {
            readBufferOffset = charactersRead - charactersWritten;
            memmove(data, data + charactersWritten, readBufferOffset);
        }
        else
        {
            readBufferOffset = 0;
        }

        // If the read buffer is full and there is still no newline, the line is too long to fit in the buffer.
        if (readBufferOffset >= sizeof(data))
        {
            fprintf(stderr, "Warning: Line doubler encountered a line longer than max line length\n");
            readBufferOffset = 0;
            shouldSkipUntilNewline = 1;
        }
    }
    // Check if EOF was actually reached or if an error occurred.
    if (charactersRead < 0)
    {
        perror("Doubler failed to read from input");
        exit(1);
    }
    // If there are still characters in the read buffer and EOF was reached, the file ended without a newline.
    if (readBufferOffset > 0)
    {
        fprintf(stderr, "Warning: Input ended without a newline\n");
    }
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

    // Create a pipe for the reader and doubler to communicate.
    int pipefd[2];
    if (pipe(pipefd) < 0)
    {
        perror("Failed to create pipe");
        return 1;
    }

    pid_t forkResult;
    if ((forkResult = fork()) < 0)
    {
        perror("Failed to fork");
        return 1;
    }
    // The child process is specifically chosen to be the doubler as it will notice if the reader exits
    // and can then exit as well when EOF is reached. If the doubler exits, the reader will not notice
    // until it tries to write to the pipe and receives SIGPIPE.
    // Having the reader as the original process makes sure the terminal doesn't show the prompt before the child process exits.
    else if (forkResult == 0)
    {
        // Close the write end of the pipe as doubler will read from the pipe and write to stdout.
        if (close(pipefd[1]) < 0)
        {
            perror("Failed to close write end of pipe");
            return 1;
        }

        lineDoubler(pipefd[0], STDOUT_FILENO);
        fprintf(stderr, "Line doubler received EOF from pipe\n");

        return 0;
    }

    // Close the read end of the pipe as reader will read stdin and write to the pipe.
    if (close(pipefd[0]) < 0)
    {
        perror("Failed to close read end of pipe");
        return 1;
    }

    if (reader(STDIN_FILENO, pipefd[1]) == 0)
        fprintf(stderr, "Reader received EOF from stdin\n");

    // Close the write end of the pipe to signal EOF to the doubler.
    if (close(pipefd[1]) < 0)
    {
        perror("Failed to close write end of pipe");
        return 1;
    }

    // I also decided to add a wait here to make sure the child process exits before the parent process
    // so that the terminal doesn't show the prompt before the child process exits.
    pid_t pid;
    int stat;
    pid = waitpid(-1, &stat, 0);
    if (pid < 0)
    {
        perror("Failed to wait for child process");
        return 1;
    }
}