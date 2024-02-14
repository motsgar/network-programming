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
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void sigpipeHandler(__attribute__((unused)) int signum)
{
    char* message = "Recieved SIGPIPE from connected client indicating that it can't recieve data anymore. Closing the connection.\n";
    write(STDERR_FILENO, message, strlen(message));
    exit(0);
}

void sigchldHandler(__attribute__((unused)) int signum)
{
    int saved_errno = errno;

    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (WIFSIGNALED(status))
        {
            fprintf(stderr, "Child process %d suddenly exited with signal %d\n", pid, WTERMSIG(status));
        }
        else if (WIFEXITED(status))
        {
            if (WEXITSTATUS(status) != 0)
            {
                fprintf(stderr, "Child process %d exited with status %d\n", pid, WEXITSTATUS(status));
            }
        }
        else
        {
            fprintf(stderr, "Child process %d exited with unknown status\n", pid);
        }
    }

    errno = saved_errno;
}

void createSignalHandlers()
{
    struct sigaction signalAction;
    if (sigemptyset(&signalAction.sa_mask) < 0)
    {
        perror("Failed to set signal mask");
        exit(1);
    }
    signalAction.sa_handler = sigpipeHandler;
    signalAction.sa_flags = 0;

    // Apply the action to SIGPIPE. If it fails, print an error and exit.
    if (sigaction(SIGPIPE, &signalAction, NULL) < 0)
    {
        perror("Failed to set SIGPIPE handler");
        exit(1);
    }

    signalAction.sa_handler = sigchldHandler;
    if (sigaction(SIGCHLD, &signalAction, NULL) < 0)
    {
        perror("Failed to set SIGCHLD handler");
        exit(1);
    }
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

void echoServer(int input, int output)
{
    char buffer[1024];
    ssize_t bytesRead;
    while ((bytesRead = read(input, buffer, sizeof(buffer))) > 0)
    {
        if (loopedWrite(output, buffer, bytesRead) < 0)
        {
            perror("Failed to write to output");
            exit(1);
        }
    }
    if (bytesRead < 0)
    {
        perror("Failed to read from input");
        exit(1);
    }
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char* argv[])
{
    createSignalHandlers();

    int networkOrderPort;

    // If argument is given, parse it and use it as the port to listen on. Otherwise use the echo service port.

    if (argc == 2)
    {
        char* endPtr;
        size_t argumentLength = strlen(argv[1]);
        long parsedPort = strtol(argv[1], &endPtr, 10);
        // Check that the entire argument was parsed. This is to avoid eg. "5abc" being parsed as 5.
        // Also check that the parsed number is positive and at most 50.
        if (endPtr != argv[1] + argumentLength)
        {
            fprintf(stderr, "Argument must be a number\n");
            return 1;
        }
        else if (parsedPort < 1 || parsedPort > 65535)
        {
            fprintf(stderr, "Port must be a positive integer between 1 and 65535\n");
            return 1;
        }

        networkOrderPort = htons(parsedPort);
    }
    else if (argc == 1)
    {
        struct servent* protocolStruct;
        protocolStruct = getservbyname("echo", "tcp");
        if (protocolStruct == NULL)
        {
            fprintf(stderr, "Failed to get echo service port\n");
            return 1;
        }

        networkOrderPort = protocolStruct->s_port;
    }
    else
    {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        return 1;
    }

    // Create a socket
    struct sockaddr_in serverAddress, clientAddress;
    int listenSocketfd, clientSocketfd;
    if ((listenSocketfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Failed to create socket");
        return 1;
    }

    setsockopt(listenSocketfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    // Set the port and address to bind the socket to
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = networkOrderPort;
    if (bind(listenSocketfd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("Failed to bind socket");
        return 1;
    }

    if (listen(listenSocketfd, 10) < 0)
    {
        perror("Failed to listen on socket");
        return 1;
    }
    fprintf(stderr, "Echo server is listening on port %d\n", ntohs(networkOrderPort));

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

        // Print client address and port
        fprintf(stderr, "New client connected from %s:%d\n", inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));

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

            echoServer(clientSocketfd, clientSocketfd);
            fprintf(stderr, "Received EOF from client %s:%d (Client disconnected)\n", inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));
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