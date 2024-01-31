#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

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

    // Read the file's info to fileStat.
    struct stat fileStat;
    fstat(file, &fileStat);

    printf("File type: ");
    // Check file type by masking the file mode with S_IFMT.
    switch (fileStat.st_mode & S_IFMT)
    {
        case S_IFBLK:
            printf("block device\n");
            break;
        case S_IFCHR:
            printf("character device\n");
            break;
        case S_IFDIR:
            printf("directory\n");
            break;
        case S_IFIFO:
            printf("FIFO/pipe\n");
            break;
        case S_IFLNK:
            printf("symlink\n");
            break;
        case S_IFREG:
            printf("regular file\n");
            break;
        case S_IFSOCK:
            printf("socket\n");
            break;
        default:
            printf("unknown?\n");
            break;
    }
    printf("User ID: %d\n", fileStat.st_uid);
    printf("File size: %ld bytes\n", fileStat.st_size);
}