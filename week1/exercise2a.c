#include <stdio.h>

int main(int argc, char* argv[])
{
    FILE* file;

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

    char line[1000];
    while (fgets(line, sizeof(line), file) != NULL)
    {
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
    if (ferror(file))
    {
        perror("Failed to read file");
        return 1;
    }

    fclose(file);
}
