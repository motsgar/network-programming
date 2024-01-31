#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define FIFO_TO_CONVERTER "/tmp/npfifo.1"
#define FIFO_FROM_CONVERTER "/tmp/npfifo.2"

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

int converter(int input, int output)
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
            return -1;
        }
    }
    // Check if EOF was actually reached or if an error occurred.
    if (charactersRead < 0)
    {
        perror("Failed to read from input");
        return -1;
    }
    return 0;
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char* argv[])
{
    // Create the FIFOs if they don't exist.
    if (mkfifo(FIFO_TO_CONVERTER, 0666) < 0)
    {
        if (errno != EEXIST)
        {
            perror("Failed to create input FIFO");
            return 1;
        }
    }
    if (mkfifo(FIFO_FROM_CONVERTER, 0666) < 0)
    {
        if (errno != EEXIST)
        {
            perror("Failed to create output FIFO");
            return 1;
        }
    }

    // Loop continuously to not have to restart the program after each "client" disconnect.
    while (1)
    {
        int receivefd, sendfd;

        // Open the FIFOs for reading and writing.
        printf("Opening input FIFO\n");
        if ((receivefd = open(FIFO_TO_CONVERTER, O_RDONLY)) < 0)
        {
            perror("Failed to open input FIFO");
            return 1;
        }
        printf("Opening output FIFO\n");
        if ((sendfd = open(FIFO_FROM_CONVERTER, O_WRONLY)) < 0)
        {
            perror("Failed to open output FIFO");
            return 1;
        }

        // Run the converter.
        if (converter(receivefd, sendfd) == 0)
            fprintf(stderr, "Received EOF from input FIFO\n");

        // Close the FIFOs.
        if (close(receivefd) < 0)
        {
            perror("Failed to close input FIFO");
            return 1;
        }
        if (close(sendfd) < 0)
        {
            perror("Failed to close output FIFO");
            return 1;
        }
    }

    // Unlink the FIFOs if the loop would have a exit condition.
    if (unlink(FIFO_TO_CONVERTER) < 0)
    {
        perror("Failed to unlink input FIFO");
        return 1;
    }
    if (unlink(FIFO_FROM_CONVERTER) < 0)
    {
        perror("Failed to unlink output FIFO");
        return 1;
    }
}