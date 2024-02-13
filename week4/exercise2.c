#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE 100

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

// Reads "length" bytes from "file" to "data" and returns the number of bytes read or -1 if an error occurred.
ssize_t loopedRead(int file, void* data, size_t length)
{
    ssize_t readResult;
    size_t charactersRead = 0;
    while ((
               readResult = read(file, data + charactersRead, length - charactersRead)
           ) >= 0)
    {
        charactersRead += readResult;
        if (charactersRead == length)
            break;
    }
    return readResult;
}

void echoProcess(int input, int output, int processTo, int processFrom)
{
    // Keep reading max BUFFER_SIZE characters to "data" until EOF is reached.
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

            if (!shouldSkipUntilNewline)
            {
                // Write the line to the server
                if (loopedWrite(processTo, data + charactersWritten, count) < 0)
                {
                    perror("Failed to write line to output");
                    exit(1);
                }

                // Read exactly the same amount of characters from the server
                char responseMessage[BUFFER_SIZE];

                ssize_t readResult;
                if ((readResult = loopedRead(processFrom, responseMessage, count)) < 0)
                {
                    perror("Failed to read from processFrom");
                    exit(1);
                }

                // Write the response to the output
                if (loopedWrite(output, responseMessage, readResult) < 0)
                {
                    perror("Failed to write to output");
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
}

struct addrinfo getHostIp(char* serverAddressString)
{
    struct addrinfo* result;
    struct addrinfo hints = {0};

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int error = getaddrinfo(serverAddressString, NULL, &hints, &result);
    if (error != 0)
    {
        fprintf(stderr, "Failed to get host ip address: %s\n", gai_strerror(error));
        exit(1);
    }

    return *result;
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char* argv[])
{
    createSignalHandler();

    char* serverAddressString;
    int socketfd;
    struct sockaddr_in serverAddress;

    // Read the server address and port from the command line arguments.
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <server ip address>\n", argv[0]);
        return 1;
    }
    serverAddressString = argv[1];

    // Get ip from host name and port from service name
    struct addrinfo hostIp = getHostIp(serverAddressString);
    struct servent* protocolStruct;
    protocolStruct = getservbyname("echo", "tcp");
    if (protocolStruct == NULL)
    {
        fprintf(stderr, "Failed to get echo service port\n");
        return 1;
    }

    fprintf(stderr, "Ip address: %s\n", inet_ntoa(((struct sockaddr_in*)hostIp.ai_addr)->sin_addr));
    fprintf(stderr, "Tcp echo service port: %d\n", ntohs(protocolStruct->s_port));

    // Create a stream socket for the connection
    if ((socketfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Failed to create socket");
        return 1;
    }

    // Initialize serverAddress struct with server IP address and port
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = ((struct sockaddr_in*)hostIp.ai_addr)->sin_addr.s_addr;
    serverAddress.sin_port = protocolStruct->s_port;

    fprintf(stderr, "Connecting to server...\n");

    // Connect the socket to the server
    if (connect(socketfd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("Failed to connect to server");
        return 1;
    }

    fprintf(stderr, "Connected to server\n");

    // Start client program that reads from standard input, sends to server and reads from server, writes to standard output
    echoProcess(STDIN_FILENO, STDOUT_FILENO, socketfd, socketfd);

    fprintf(stderr, "Received EOF from input\n");

    // Close the socket
    if (close(socketfd) < 0)
    {
        perror("Failed to close socket");
        return 1;
    }

    return 0;
}