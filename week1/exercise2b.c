#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

int main(int argc, char* argv[])
{
    int file;

    // If a file is specified, open it. Otherwise, use stdin.
    if (argc > 1)
    {
        file = open(argv[1], O_RDONLY);
        if (file < 0)
        {
            perror("Failed to open file");
            return 1;
        }
    }
    else
    {
        file = STDIN_FILENO;
    }

    // Keep reading max 1000 characters to "data" until EOF is reached.
    // Data is read to "data + readBufferOffset" to allow contiunation of reading a line
    // if the previous read ended without a newline.
    char data[1000];
    ssize_t charactersRead;
    size_t readBufferOffset = 0;
    while ((charactersRead = read(file, data + readBufferOffset, sizeof(data) - readBufferOffset)) > 0)
    {
        charactersRead += readBufferOffset;

        // Keep writing lines to stdout until the read buffer is empty.
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

            // Write the line to stdout twice.
            ssize_t writeResult = loopedWrite(STDOUT_FILENO, data + charactersWritten, count);
            if (writeResult < 0)
            {
                perror("Failed to write to stdout");
                goto errorExit;
            }
            writeResult = loopedWrite(STDOUT_FILENO, data + charactersWritten, count);
            if (writeResult < 0)
            {
                perror("Failed to write to stdout");
                goto errorExit;
            }

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
            fprintf(stderr, "Line longer than max line length\n");
            return 1;
        }
    }
    // Check if EOF was actually reached or if an error occurred.
    if (charactersRead < 0)
    {
        perror("Failed to read file");
        return 1;
    }
    // If there are still characters in the read buffer and EOF was reached, the file ended without a newline.
    if (readBufferOffset > 0)
    {
        fprintf(stderr, "File ended without a newline\n");
        return 1;
    }

    // If the file is not stdin, close it.
    if (file != STDIN_FILENO)
    {
        if (close(file) < 0)
        {
            perror("Failed to close file");
            return 1;
        }
    }

    return 0;

    // If an error occurred, close the file if it is not stdin and return 1.
errorExit:
    if (file != STDIN_FILENO)
    {
        if (close(file) < 0)
        {
            perror("Failed to close file");
            return 1;
        }
    }
    return 1;
}
