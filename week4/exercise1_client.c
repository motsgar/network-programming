#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE 100

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

void lineProcesser(int input, int output, int processSocket, struct sockaddr* serverAddress, int serverAddressLength)
{
    // Forking to read from input and processSocket at the same time
    pid_t child_pid = fork();
    if (child_pid < 0)
    {
        perror("Fork failed");
        exit(1);
    }
    else if (child_pid == 0) // Child process
    {
        // Child process reads from processSocket and writes to output
        char responseMessage[BUFFER_SIZE];
        ssize_t readResult;
        while ((readResult = recvfrom(processSocket, responseMessage, BUFFER_SIZE, 0, NULL, NULL)) >= 0)
        {
            if (loopedWrite(output, responseMessage, readResult) < 0)
            {
                perror("Failed to write to output");
                exit(1);
            }
        }
        // Error occurred, exit with failure
        perror("Failed to read from processSocket");
        exit(1);
    }
    else // Parent process
    {
        // Keep reading max 100 characters to "data" until EOF is reached.
        // Data is read to "data + readBufferOffset" to allow continuation of reading a line
        // if the previous read ended without a newline.
        char data[BUFFER_SIZE];
        ssize_t charactersRead;
        size_t readBufferOffset = 0;
        char shouldSkipUntilNewline = 0;
        while ((charactersRead = read(input, data + readBufferOffset, BUFFER_SIZE - readBufferOffset)) > 0)
        {
            charactersRead += readBufferOffset;

            // Keep writing lines to processTo until the read buffer is empty or no newlines are found.
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

                // If the buffer was full previously, but now a newline was found, we should skip all the additional characters until the next (this) newline.
                if (!shouldSkipUntilNewline)
                {
                    if (sendto(processSocket, data + charactersWritten, count, 0, serverAddress, serverAddressLength) < 0)
                    {
                        perror("Failed to send data to processing server");
                        exit(1);
                    }
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
            if (readBufferOffset >= BUFFER_SIZE)
            {
                fprintf(stderr, "Warning: Encountered a line longer than max line length\n");
                readBufferOffset = 0;
                shouldSkipUntilNewline = 1;
            }
        }
        // Check if EOF was actually reached or if an error occurred.
        if (charactersRead < 0)
        {
            perror("Failed to read from input");
            exit(1);
        }
        // If there are still characters in the read buffer and EOF was reached, the file ended without a newline.
        if (readBufferOffset > 0)
        {
            fprintf(stderr, "Warning: Input ended without a newline\n");
        }

        // Send SIGTERM to the child process to indicate that it should exit.
        fprintf(stderr, "Received EOF from input. Waiting for response reader to exit\n");
        kill(child_pid, SIGTERM);

        // Wait for the child process to finish
        int status;
        if (waitpid(child_pid, &status, 0) < 0)
        {
            perror("Failed to wait for response reader process");
            exit(1);
        }

        // Check if the child process exited via the expected signal
        if (WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM)
        {
            fprintf(stderr, "Response reader process exited\n");
        }
        else
        {
            if (WIFSIGNALED(status))
                fprintf(stderr, "Response reader process exited with signal %d\n", WTERMSIG(status));
            else if (WIFEXITED(status))
                fprintf(stderr, "Response reader process exited with status %d\n", WEXITSTATUS(status));
            else
                fprintf(stderr, "Response reader process exited for unknown reason\n");
        }
    }
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char* argv[])
{
    int serverPort;
    char* serverAddressString;
    int socketfd;
    struct sockaddr_in serverAddress;

    // Read the server address and port from the command line arguments.
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <server ip address> <server port>\n", argv[0]);
        return 1;
    }
    serverAddressString = argv[1];
    serverPort = atoi(argv[2]);

    // Create a stream socket for the connection
    if ((socketfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Failed to create socket");
        return 1; // Exit with error if socket creation fails
    }

    // Initialize serverAddress struct with server IP address and port
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    if (inet_aton(serverAddressString, &serverAddress.sin_addr) == 0)
    {
        perror("Invalid server IP address");
        return 1;
    }
    serverAddress.sin_port = htons(serverPort);

    // Start client program that reads from standard input, sends to server and reads from server, writes to standard output
    lineProcesser(STDIN_FILENO, STDOUT_FILENO, socketfd, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

    // Close the socket
    if (close(socketfd) < 0)
    {
        perror("Failed to close socket");
        return 1;
    }

    return 0;
}