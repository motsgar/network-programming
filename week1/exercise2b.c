#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

size_t charactersUntilNewline(char* string, size_t length)
{
    size_t count = 0;

    do
    {
        count++;

        if (count >= length)
            break;
    }
    while (string[count - 1] != '\n');

    return count;
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
    while ((charactersRead = read(file, data, sizeof(data))) >= 0)
    {
        if (charactersRead == 0)
        {
            break;
        }

        size_t charactersWritten = 0;
        while (charactersWritten < (size_t)charactersRead)
        {
            size_t count = charactersUntilNewline(data + charactersWritten, charactersRead - charactersWritten);
            ssize_t writeResult = write(STDOUT_FILENO, data + charactersWritten, count);
            if (writeResult < 0)
            {
                perror("Failed to write to stdout");
                return 1;
            }
            if (writeResult != count)
            {
                fprintf(stderr, "Failed to write all data to stdout\n");
                return 1;
            }
            writeResult = write(STDOUT_FILENO, data + charactersWritten, count);
            if (writeResult < 0)
            {
                perror("Failed to write to stdout");
                return 1;
            }
            if (writeResult != count)
            {
                fprintf(stderr, "Failed to write all data to stdout\n");
                return 1;
            }
            charactersWritten += count;
        }
    }
    if (charactersRead < 0)
    {
        perror("Failed to read file");
        return 1;
    }
}
