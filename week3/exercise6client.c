#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
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

// Returns the time since the last call to this function in microseconds.
// Implemented using gettimeofday and saving the previous time in a static variable.
int64_t getTimeSinceLastCall()
{
    static int64_t previous = 0;

    struct timeval tv;

    if (gettimeofday(&tv, NULL) < 0)
    {
        perror("Failed to get time of day");
        exit(1);
    }

    // Calculate the time since the last call to this function in microseconds and save the current time.
    int64_t now = tv.tv_sec * 1000000 + tv.tv_usec;
    int64_t timeTaken = now - previous;
    previous = now;

    return timeTaken;
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

// Generates random data and writes it to output in chunks of chunkSize until totalLength bytes have been written.
void dataGenerator(int output, int totalLength, int chunkSize)
{
    char* buffer = malloc(chunkSize);
    if (buffer == NULL)
    {
        perror("Failed to allocate buffer");
        exit(1);
    }
    ssize_t bytesWritten;
    while (totalLength > 0)
    {
        for (int i = 0; i < chunkSize; i++)
        {
            buffer[i] = (char)(rand() % 256);
        }
        bytesWritten = loopedWrite(output, buffer, chunkSize);
        if (bytesWritten < 0)
        {
            perror("Failed to write to output");
            free(buffer);
            exit(1);
        }
        totalLength -= bytesWritten;
    }
    free(buffer);
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char* argv[])
{
    createSignalHandler();

    int serverPort;
    char* serverAddressString;
    int socketfd;
    struct sockaddr_in serverAddress;

    // Read the server address and port from the command line arguments.
    if (argc != 5)
    {
        fprintf(stderr, "Usage: %s <server ip address> <server port> <total send amount> <chunk size>\n", argv[0]);
        return 1;
    }
    serverAddressString = argv[1];
    serverPort = atoi(argv[2]);
    int totalSendAmount = atoi(argv[3]);
    int chunkSize = atoi(argv[4]);

    if (totalSendAmount <= 0 || chunkSize <= 0)
    {
        fprintf(stderr, "Total send amount and chunk size must be positive\n");
        return 1;
    }
    if (chunkSize > totalSendAmount)
    {
        fprintf(stderr, "Chunk size must be less than or equal to total send amount\n");
        return 1;
    }
    if (totalSendAmount % chunkSize != 0)
    {
        fprintf(stderr, "Total send amount must be a multiple of chunk size\n");
        return 1;
    }

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

    printf("Connected to server, starting to send data\n");
    getTimeSinceLastCall();
    dataGenerator(socketfd, totalSendAmount, chunkSize);
    printf("Data sent to server, Sending fin packet and waiting for server to close the connection\n");

    // Send fin packet and wait for the server to close the connection
    shutdown(socketfd, SHUT_WR);

    char buffer[1];
    ssize_t bytesRead;
    while ((bytesRead = read(socketfd, buffer, sizeof(buffer))) > 0)
    {
        // Do nothing with the data.
    }
    int64_t timeTakenForTransfer = getTimeSinceLastCall();
    if (bytesRead < 0)
    {
        perror("Failed to wait for server to close connection");
        return 1;
    }
    printf("Server closed the connection\n");

    // Close the socket
    if (close(socketfd) < 0)
    {
        perror("Failed to close socket");
        return 1;
    }

    // Print time taken for transfer and transfer speed
    printf("Time taken for transfer: %ldus\n", timeTakenForTransfer);
    printf("Transfer speed: %.2fMB/s\n", (float)totalSendAmount / ((float)timeTakenForTransfer));

    return 0;
}