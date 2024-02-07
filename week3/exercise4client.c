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

// I optionally added a signal handler for SIGPIPE.
void sigpileHandler(__attribute__((unused)) int signum)
{
    char* message = "Recieved SIGPIPE from response reader indicating that it can't recieve data anymore.\n";
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

void dataGenerator(int input, int output, int processTo, int processFrom)
{
    fprintf(stderr, "Forking to read from input and processFrom at the same time\n");
    // Fork a child process
    pid_t child_pid = fork();
    if (child_pid < 0)
    {
        perror("Fork failed");
        exit(1);
    }
    else if (child_pid == 0) // Child process
    {
        // Child process reads from processFrom and writes to output
        char responseMessage[100];
        ssize_t readResult;
        while ((readResult = read(processFrom, responseMessage, sizeof(responseMessage))) > 0)
        {
            if (loopedWrite(output, responseMessage, readResult) < 0)
            {
                perror("Child process failed to write to output");
                exit(1);
            }
        }
        if (readResult < 0)
        {
            // Error occurred, exit with failure
            perror("Failed to read from processFrom");
            exit(1);
        }

        // EOF reached, exit with success
        fprintf(stderr, "Received EOF from processFrom\n");
        exit(0);
    }
    else // Parent process
    {
        // Keep reading max 100 characters to "data" until EOF is reached.
        // Data is read to "data + readBufferOffset" to allow continuation of reading a line
        // if the previous read ended without a newline.
        char data[100];
        ssize_t charactersRead;
        size_t readBufferOffset = 0;
        char shouldSkipUntilNewline = 0;
        while ((charactersRead = read(input, data + readBufferOffset, sizeof(data) - readBufferOffset)) > 0)
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

                if (!shouldSkipUntilNewline)
                {
                    if (loopedWrite(processTo, data + charactersWritten, count) < 0)
                    {
                        perror("Failed to write line to output");
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
            if (readBufferOffset >= sizeof(data))
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

        // Wait for the child process to finish
        int status;
        if (waitpid(child_pid, &status, 0) < 0)
        {
            perror("Failed to wait for response reader process");
            exit(1);
        }

        // Check if the child process exited normally
        if (WIFEXITED(status))
        {
            int exit_status = WEXITSTATUS(status);
            if (exit_status != 0)
            {
                fprintf(stderr, "Response reader exited with non-zero status: %d\n", exit_status);
                exit(1);
            }
        }
        else
        {
            fprintf(stderr, "Response reader did not exit normally\n");
            exit(1);
        }
    }
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char* argv[])
{
    createSignalHandler();

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
    if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Failed to create socket");
        return 1; // Exit with error if socket creation fails
    }

    // Initialize serverAddress struct with server IP address and port
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr(serverAddressString);
    serverAddress.sin_port = htons(serverPort);

    // Connect the socket to the server
    if (connect(socketfd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("Failed to connect to server");
        return 1;
    }

    // Start client program that reads from standard input, sends to server and reads from server, writes to standard output
    dataGenerator(STDIN_FILENO, STDOUT_FILENO, socketfd, socketfd);

    fprintf(stderr, "Received EOF from input\n");

    // Close the socket
    if (close(socketfd) < 0)
    {
        perror("Failed to close socket");
        return 1;
    }

    return 0;
}