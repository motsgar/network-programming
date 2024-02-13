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

#define BUFFER_SIZE 100

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
    int listenSocketfd;
    if ((listenSocketfd = socket(PF_INET, SOCK_DGRAM, PF_UNSPEC)) < 0)
    {
        perror("Failed to create socket");
        return 1;
    }

    // Set the port and address to bind the socket to
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(serverPort);
    if (bind(listenSocketfd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("Failed to bind socket");
        return 1;
    }

    fprintf(stderr, "Started listening for UDP messages on port %d\n", serverPort);

    while (1)
    {
        int clientAddressSize = sizeof(clientAddress);

        char message[BUFFER_SIZE];
        int messageLength = recvfrom(listenSocketfd, message, BUFFER_SIZE, 0, (struct sockaddr*)&clientAddress, (socklen_t*)&clientAddressSize);
        if (messageLength < 0)
        {
            // idk if these are correct or even exhaustive, but these were in the example of tcp man page
            if (errno == EINTR || errno == ENETDOWN || errno == EPROTO || errno == ENOPROTOOPT || errno == EHOSTDOWN || errno == EHOSTUNREACH || errno == ENETUNREACH || errno == EOPNOTSUPP || errno == ENOENT)
                continue;
            else
            {
                perror("Failed to receive message from client");
                return 1;
            }
        }

        fprintf(stderr, "Received %d bytes from client\n", messageLength);

        // Send the message back to the client twice
        if (sendto(listenSocketfd, message, messageLength, 0, (struct sockaddr*)&clientAddress, clientAddressSize) < 0)
        {
            perror("Failed to send message to client");
            continue;
        }
        if (sendto(listenSocketfd, message, messageLength, 0, (struct sockaddr*)&clientAddress, clientAddressSize) < 0)
        {
            perror("Failed to send message to client");
            continue;
        }
    }

    return 0;
}