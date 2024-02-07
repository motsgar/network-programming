#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define FIFO_IN "/tmp/np_fifo_converter.in"
#define FIFO_OUT "/tmp/np_fifo_doubler.in"

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
// Converts all lowercase characters in "string" of length "length" to uppercase.
void toUpper(char* string, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        // Conversion done by first checking if the character is in the range of lowercase characters.
        // If it is, the difference between the uppercase and lowercase character is subtracted from the character
        // as they are sequential in the ASCII table.
        if (string[i] >= 'a' && string[i] <= 'z')
            string[i] -= 'a' - 'A';
    }
}

// Creates the FIFOs for sending and receiving data
void makeFifos()
{
    // Create the FIFOs if they don't exist.
    if (mkfifo(FIFO_IN, 0666) < 0)
    {
        if (errno != EEXIST)
        {
            perror("Failed to create input FIFO");
            exit(1);
        }
    }
    if (mkfifo(FIFO_OUT, 0666) < 0)
    {
        if (errno != EEXIST)
        {
            perror("Failed to create output FIFO");
            exit(1);
        }
    }
}

// Unlinks the FIFOs to clean up after the program.
void unlinkFifos()
{
    // Unlink the FIFOs.
    if (unlink(FIFO_IN) < 0)
    {
        perror("Failed to unlink input FIFO");
        exit(1);
    }
    if (unlink(FIFO_OUT) < 0)
    {
        perror("Failed to unlink output FIFO");
        exit(1);
    }
}

// I optionally added a signal handler for SIGPIPE.
void sigpileHandler(__attribute__((unused)) int signum)
{
    unlinkFifos();

    char* message = "Recieved SIGPIPE from converter indicating that it can't recieve data anymore.\n";
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

void converter(int input, int output)
{
    printf("Converter started\n");

    // Read from input and write to output until EOF is reached.
    char message[100];
    ssize_t charactersRead;
    while ((charactersRead = read(input, message, sizeof(message))) > 0)
    {
        // Convert the message to uppercase and write it to the output.
        toUpper(message, charactersRead);
        if (loopedWrite(output, message, charactersRead) < 0)
        {
            perror("Failed to write to output");
            exit(1);
        }
    }
    // Check if EOF was actually reached or if an error occurred.
    if (charactersRead < 0)
    {
        perror("Failed to read from input");
        exit(1);
    }
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char* argv[])
{
    createSignalHandler();
    makeFifos();

    int inputfd, outputfd;

    // Open the FIFOs for reading and writing.
    printf("Opening input FIFO\n");
    if ((inputfd = open(FIFO_IN, O_RDONLY)) < 0)
    {
        perror("Failed to open input FIFO");
        return 1;
    }
    printf("Opening output FIFO\n");
    if ((outputfd = open(FIFO_OUT, O_WRONLY)) < 0)
    {
        perror("Failed to open output FIFO");
        return 1;
    }

    // Run the converter.
    converter(inputfd, outputfd);
    fprintf(stderr, "Received EOF from input FIFO\n");

    // Close the FIFOs.
    if (close(inputfd) < 0)
    {
        perror("Failed to close input FIFO");
        return 1;
    }
    if (close(outputfd) < 0)
    {
        perror("Failed to close output FIFO");
        return 1;
    }

    unlinkFifos();

    return 0;
}