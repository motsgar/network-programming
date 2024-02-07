#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define FIFO_IN "/tmp/np_fifo_doubler.in"

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

// Creates a FIFO for receiving data.
void makeFifos()
{
    if (mkfifo(FIFO_IN, 0666) < 0)
    {
        if (errno != EEXIST)
        {
            perror("Failed to create input FIFO");
            exit(1);
        }
    }
}

// Unlinks the FIFOs to clean up after the program.
void unlinkFifos()
{
    if (unlink(FIFO_IN) < 0)
    {
        perror("Failed to unlink FIFO");
        exit(1);
    }
}

// Technically I could skip the overflow message handling part as the message length is known to be under 100 characters, but
// it was faster to just copy the code with ready made handling from the previous exercises. Also probably better to have it anyway just in case.
void dataEater(int input, int output)
{
    printf("Line doubler started\n");

    // Keep reading max 100 characters to "data" until EOF is reached.
    // Data is read to "data + readBufferOffset" to allow contiunation of reading a line
    // if the previous read ended without a newline.
    char data[100];
    ssize_t charactersRead;
    size_t readBufferOffset = 0;
    char shouldSkipUntilNewline = 0;
    while ((charactersRead = read(input, data + readBufferOffset, sizeof(data) - readBufferOffset)) > 0)
    {
        charactersRead += readBufferOffset;

        // Keep writing lines to output until the read buffer is empty.
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
                if (loopedWrite(output, data + charactersWritten, count) < 0 || loopedWrite(output, data + charactersWritten, count) < 0)
                {
                    perror("Doubler failed to write to output");
                    exit(1);
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
            fprintf(stderr, "Warning: Line doubler encountered a line longer than max line length\n");
            readBufferOffset = 0;
            shouldSkipUntilNewline = 1;
        }
    }
    // Check if EOF was actually reached or if an error occurred.
    if (charactersRead < 0)
    {
        perror("Doubler failed to read from input");
        exit(1);
    }
    // If there are still characters in the read buffer and EOF was reached, the file ended without a newline.
    if (readBufferOffset > 0)
    {
        fprintf(stderr, "Warning: Input ended without a newline\n");
    }
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char* argv[])
{
    makeFifos();

    int inputfd, outputfd = STDOUT_FILENO;

    fprintf(stderr, "Opening input FIFO\n");
    if ((inputfd = open(FIFO_IN, O_RDONLY)) < 0)
    {
        perror("Failed to open input FIFO");
        return 1;
    }

    dataEater(inputfd, outputfd);
    fprintf(stderr, "Received EOF from input\n");

    if (close(inputfd) < 0)
    {
        perror("Failed to close input FIFO");
        return 1;
    }

    unlinkFifos();

    return 0;
}