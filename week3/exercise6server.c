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
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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

void dataEater(int input, int readAmount)
{
    char* buffer = malloc(readAmount);
    if (buffer == NULL)
    {
        perror("Failed to allocate buffer for reading");
        exit(1);
    }

    ssize_t bytesRead;
    size_t bytesReadTotal = 0;
    int firstRead = 1;
    while ((bytesRead = read(input, buffer, readAmount)) > 0)
    {
        bytesReadTotal += bytesRead;
        if (firstRead)
        {
            getTimeSinceLastCall();
            firstRead = 0;
        }
        // Do nothing with the data.
    }
    int64_t timeToReadData = getTimeSinceLastCall();
    if (bytesRead < 0)
    {
        perror("Failed to read from input");
        free(buffer);
        exit(1);
    }
    free(buffer);

    if (firstRead)
    {
        fprintf(stderr, "Warning: Client exited before sending any data\n");
        return;
    }

    // Print the time taken to eat the data and speed
    fprintf(stderr, "Time to read data: %ldus\n", timeToReadData);
    fprintf(stderr, "Speed: %fMB/s\n", (float)bytesReadTotal / (float)timeToReadData);
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char* argv[])
{
    createSignalHandler();

    int serverPort;

    // Read the server port from the command line arguments.
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <server port> <read amount per read call>\n", argv[0]);
        exit(1);
    }
    serverPort = atoi(argv[1]);
    int readAmount = atoi(argv[2]);

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

            fprintf(stderr, "Child process started eating data for client connection\n");
            dataEater(clientSocketfd, readAmount);
            fprintf(stderr, "Received EOF from client (client disconnected / sent all data)\n");
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