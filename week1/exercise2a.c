#include <stdio.h>

int main(int argc, char* argv[])
{
    FILE* file;

    // If a file is specified, open it. Otherwise, use stdin.
    if (argc > 1)
    {
        file = fopen(argv[1], "r");
        if (file == NULL)
        {
            perror("Failed to open file");
            return 1;
        }
    }
    else
    {
        file = stdin;
    }

    // Keep reading lines to "line" until EOF is reached.
    char line[1000];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        // Write the line to stdout twice.
        if (fputs(line, stdout) == EOF)
        {
            perror("Failed to write to stdout");
            return 1;
        }
        if (fputs(line, stdout) == EOF)
        {
            perror("Failed to write to stdout");
            return 1;
        }
    }
    // Check if EOF was actually reached or if an error occurred.
    if (ferror(file))
    {
        perror("Failed to read file");
        return 1;
    }

    // If the file is not stdin, close it.
    if (file != stdin)
    {
        if (fclose(file) != 0)
        {
            perror("Failed to close file");
            return 1;
        }
    }
}
