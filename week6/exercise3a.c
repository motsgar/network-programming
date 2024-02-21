#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

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

    // Create a datagram socket for the connection
    if ((socketfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Failed to create socket");
        return 1; // Exit with error if socket creation fails
    }

    // Enabling the don't fragment bit causes the max length to be 1472
    // int val = IP_PMTUDISC_DO;
    // setsockopt(socketfd, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));

    // Initialize serverAddress struct with server IP address and port
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    if (inet_aton(serverAddressString, &serverAddress.sin_addr) == 0)
    {
        perror("Invalid server IP address");
        return 1;
    }
    serverAddress.sin_port = htons(serverPort);

    socklen_t serverAddressLength = sizeof(serverAddress);

    size_t messageLength = 0;
    while (1)
    {
        void* message = malloc(messageLength);
        if (message == NULL)
        {
            perror("Failed to allocate memory for message");
            return 1;
        }
        fprintf(stderr, "Sending message of length %zu\n", messageLength);

        if (sendto(socketfd, message, messageLength, 0, (struct sockaddr*)&serverAddress, serverAddressLength) < 0)
        {
            perror("Failed to send data to processing server");
            exit(1);
        }

        free(message);

        // usleep(2000);

        messageLength++;
    }

    // Close the socket
    if (close(socketfd) < 0)
    {
        perror("Failed to close socket");
        return 1;
    }

    return 0;
}