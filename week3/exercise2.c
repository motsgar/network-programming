#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define LOCKFILE "/tmp/np_lockfile"
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
/* default permissions for new files */

static struct flock lock_it, unlock_it;
static int lock_fd = -1;
/* fcntl() will fail if my_lock_init() not called */

void my_lock_init(char *pathname)
{
    // Hardcoded the lock file to be /tmp/np_lockfile
    lock_fd = open(pathname, O_CREAT | O_WRONLY, FILE_MODE);
    if (lock_fd < 0)
    {
        perror("lock file open failed");
        exit(1);
    }

    // Do not delete the file to ensure that the lock is also available to other processes

    lock_it.l_type = F_WRLCK;
    lock_it.l_whence = SEEK_SET;
    lock_it.l_start = 0;
    lock_it.l_len = 0;

    unlock_it.l_type = F_UNLCK;
    unlock_it.l_whence = SEEK_SET;
    unlock_it.l_start = 0;
    unlock_it.l_len = 0;
}

void my_lock_wait()
{
    int rc;

    while ((rc = fcntl(lock_fd, F_SETLKW, &lock_it)) < 0)
    {
        if (errno == EINTR)
            continue;
        else
        {
            perror("fcntl error for my_lock_wait");
            exit(1);
        }
    }
}

void my_lock_release()
{
    if (fcntl(lock_fd, F_SETLKW, &unlock_it) < 0)
    {
        perror("fcntl error for my_lock_release");
        exit(1);
    }
}
/* end my_lock_wait */
int main(int argc, char **argv)
{
    int fd, i, nloop, zero = 0;
    int *ptr;

    if (argc != 3)
    {
        perror("usage: incr2 <pathname> <#loops>");
        exit(1);
    }
    nloop = atoi(argv[2]);

    /* 4open file, initialize to 0 if empty, map into memory */
    if ((fd = open(argv[1], O_RDWR | O_CREAT, FILE_MODE)) < 0)
    {
        perror("open failed");
        exit(1);
    }
    // Check if file is empty. If it is, write 0 to it
    if (lseek(fd, 0, SEEK_END) == 0)
    {
        write(fd, &zero, sizeof(int));
    }

    ptr = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED)
    {
        perror("mmap failed");
        exit(1);
    }
    close(fd);

    /* 4create, initialize, and unlink file lock */
    my_lock_init(LOCKFILE);

    setbuf(stdout, NULL); /* stdout is unbuffered */

    for (i = 0; i < nloop; i++)
    {
        my_lock_wait();
        printf("Number: %d\n", (*ptr)++);
        my_lock_release();
    }
    exit(0);
}