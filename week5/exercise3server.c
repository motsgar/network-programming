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

void forwarder(int input, int output)
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

    setsockopt(listenSocketfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

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

        // shutdown(clientSocketfd, SHUT_RD);
        // shutdown(clientSocketfd, SHUT_WR);
        // sleep(5);

        forwarder(clientSocketfd, STDOUT_FILENO);
        fprintf(stderr, "Received EOF from client (client disconnected)\n");
        if (close(clientSocketfd) < 0)
        {
            perror("Failed to close client socket after client disconnected");
            return 1;
        }
    }

    if (close(listenSocketfd) < 0)
    {
        perror("Failed to close listen socket");
        return 1;
    }

    return 0;
}