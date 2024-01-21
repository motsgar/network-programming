#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

int main(int argc, char* argv[])
{
    int file;

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

    char data[1000];
    ssize_t charactersRead;
    size_t readBufferOffset = 0;
    while ((charactersRead = read(file, data + readBufferOffset, sizeof(data) - readBufferOffset)) > 0)
    {
        charactersRead += readBufferOffset;

        size_t charactersWritten = 0;
        while (charactersWritten < (size_t)charactersRead)
        {
            ssize_t count = charactersUntilNewline(data + charactersWritten, charactersRead - charactersWritten);
            if (count < 0)
                break;

            ssize_t writeResult = write(STDOUT_FILENO, data + charactersWritten, count);
            if (writeResult < 0)
            {
                perror("Failed to write to stdout");
                goto errorExit;
            }
            if (writeResult != count)
            {
                fprintf(stderr, "Failed to write all data to stdout\n");
                goto errorExit;
            }
            writeResult = write(STDOUT_FILENO, data + charactersWritten, count);
            if (writeResult < 0)
            {
                perror("Failed to write to stdout");
                goto errorExit;
            }
            if (writeResult != count)
            {
                fprintf(stderr, "Failed to write all data to stdout\n");
                goto errorExit;
            }
            charactersWritten += count;
        }

        if (charactersWritten < (size_t)charactersRead)
        {
            readBufferOffset = charactersRead - charactersWritten;
            memmove(data, data + charactersWritten, readBufferOffset);
        }
        else
        {
            readBufferOffset = 0;
        }

        if (readBufferOffset >= sizeof(data))
        {
            fprintf(stderr, "Line longer than max line length\n");
            return 1;
        }
    }
    if (charactersRead < 0)
    {
        perror("Failed to read file");
        return 1;
    }
    if (readBufferOffset > 0)
    {
        fprintf(stderr, "File ended without a newline\n");
        return 1;
    }
    if (close(file) < 0)
    {
        perror("Failed to close file");
        return 1;
    }

    return 0;

errorExit:
    if (file != STDIN_FILENO)
    {
        if (close(file) < 0)
        {
            perror("Failed to close file");
            return 1;
        }
    }
    return -1;
}
