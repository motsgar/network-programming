#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// NOTE: I noticed after writing the code that the server should just print the data it receives and not send it back.
// I guess I just made the slightly more complex version of the exercise.

// I optionally added a signal handler for SIGPIPE.
void sigpileHandler(__attribute__((unused)) int signum)
{
    char* message = "Recieved SIGPIPE from connected client indicating that it can't recieve data anymore. Closing the connection.\n";
    write(STDERR_FILENO, message, strlen(message));
    exit(0);
}

void createSignalHandler()
{
    // Register additional signal handler.
    struct sigaction action;
    if (sigemptyset(&action.sa_mask) < 0)
    {
        perror("Failed to set signal mask");
        exit(1);
    }
    action.sa_handler = sigpileHandler;
    action.sa_flags = 0;

    // Apply the action to SIGPIPE. If it fails, print an error and exit.
    if (sigaction(SIGPIPE, &action, NULL) < 0)
    {
        perror("Failed to set SIGPIPE handler");
        exit(1);
    }
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

        // Keep writing lines to output until the read buffer is empty.
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

            sleep(1);

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
    createSignalHandler();

    int serverPort;

    // Read the server port from the command line arguments.
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <server port>\n", argv[0]);
        exit(1);
    }
    serverPort = atoi(argv[1]);

    // Create a socket
    struct sockaddr_in serverAddress, clientAddress;
    int listenSocketfd, clientSocketfd;
    if ((listenSocketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Failed to create socket");
        return 1;
    }

    // Set the port and address to bind the socket to
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);
    if (bind(listenSocketfd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("Failed to bind socket");
        return 1;
    }

    if (listen(listenSocketfd, 5) < 0)
    {
        perror("Failed to listen on socket");
        return 1;
    }
    fprintf(stderr, "Server listening on port %d\n", serverPort);

    while (1)
    {
        int clientAddressSize = sizeof(clientAddress);
        while ((clientSocketfd = accept(listenSocketfd, (struct sockaddr*)&clientAddress, (socklen_t*)&clientAddressSize)) < 0)
        {
            if (clientSocketfd < 0)
            {
                if (errno == EINTR || errno == ENETDOWN || errno == EPROTO || errno == ENOPROTOOPT || errno == EHOSTDOWN || errno == EHOSTUNREACH || errno == ENETUNREACH || errno == EOPNOTSUPP || errno == ENOENT)
                    continue;
                else
                {
                    perror("Failed to accept client connection");
                    return 1;
                }
            }
        }

        fprintf(stderr, "Accepted client connection\n");

        pid_t child_pid;
        if ((child_pid = fork()) < 0)
        {
            perror("Failed to fork for client connection");
            return 1;
        }
        else if (child_pid == 0)
        {
            // Child process
            close(listenSocketfd);

            lineDoubler(clientSocketfd, clientSocketfd);
            fprintf(stderr, "Received EOF from client (client disconnected)\n");
            if (close(clientSocketfd) < 0)
            {
                perror("Failed to close client socket after client disconnected");
                return 1;
            }
            exit(0);
        }
        else
        {
            // Parent process
            if (close(clientSocketfd) < 0)
            {
                perror("Main process failed to close client socket");
                return 1;
            }
        }
    }

    if (close(listenSocketfd) < 0)
    {
        perror("Failed to close listen socket");
        return 1;
    }

    return 0;
}