#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define LOCKFILE "/tmp/np_lockfile_XXXXXX"
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
/* default permissions for new files */

volatile int childStarted = 0;

static struct flock lock_it, unlock_it;
static int lock_fd1 = -1, lock_fd2 = -1, lock_fd3 = -1;

void sigusrHandler(__attribute__((unused)) int signo)
{
    __atomic_store_n(&childStarted, 1, __ATOMIC_SEQ_CST);
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
    action.sa_handler = sigusrHandler;
    action.sa_flags = 0;

    // Apply the action to SIGPIPE. If it fails, print an error and exit.
    if (sigaction(SIGUSR1, &action, NULL) < 0)
    {
        perror("Failed to set SIGUSR1 handler");
        exit(1);
    }
}

void my_lock_init()
{
    char lock1_path[] = LOCKFILE;
    char lock2_path[] = LOCKFILE;
    char lock3_path[] = LOCKFILE;
    if ((lock_fd1 = mkstemp(lock1_path)) < 0 || (lock_fd2 = mkstemp(lock2_path)) < 0 || (lock_fd3 = mkstemp(lock3_path)) < 0)
    {
        perror("Mkstemp failed");
        exit(1);
    }

    // Unlink the generated files
    if (unlink(lock1_path) < 0 || unlink(lock2_path) < 0 || unlink(lock3_path) < 0)
    {
        perror("Unlink failed");
        exit(1);
    }

    lock_it.l_type = F_WRLCK;
    lock_it.l_whence = SEEK_SET;
    lock_it.l_start = 0;
    lock_it.l_len = 0;

    unlock_it.l_type = F_UNLCK;
    unlock_it.l_whence = SEEK_SET;
    unlock_it.l_start = 0;
    unlock_it.l_len = 0;
}

void my_lock_wait1()
{
    int rc;

    while ((rc = fcntl(lock_fd1, F_SETLKW, &lock_it)) < 0)
    {
        if (errno == EINTR)
            continue;
        else
        {
            perror("fcntl error for my_lock_wait1");
            exit(1);
        }
    }
}

void my_lock_wait2()
{
    int rc;

    while ((rc = fcntl(lock_fd2, F_SETLKW, &lock_it)) < 0)
    {
        if (errno == EINTR)
            continue;
        else
        {
            perror("fcntl error for my_lock_wait2");
            exit(1);
        }
    }
}

void my_lock_wait3()
{
    int rc;

    while ((rc = fcntl(lock_fd3, F_SETLKW, &lock_it)) < 0)
    {
        if (errno == EINTR)
            continue;
        else
        {
            perror("fcntl error for my_lock_wait3");
            exit(1);
        }
    }
}

void my_lock_release1()
{
    if (fcntl(lock_fd1, F_SETLKW, &unlock_it) < 0)
    {
        perror("fcntl error for my_lock_release1");
        exit(1);
    }
}

void my_lock_release2()
{
    if (fcntl(lock_fd2, F_SETLKW, &unlock_it) < 0)
    {
        perror("fcntl error for my_lock_release2");
        exit(1);
    }
}

void my_lock_release3()
{
    if (fcntl(lock_fd3, F_SETLKW, &unlock_it) < 0)
    {
        perror("fcntl error for my_lock_release3");
        exit(1);
    }
}

void processOperation(int *ptr, size_t mapSize, char character)
{
    int newVal = (*ptr)++;
    if (newVal + sizeof(int) >= mapSize)
    {
        fprintf(stderr, "Index to write character is out of mapped memory range. Needed to reset/delete file.\n");
        exit(1);
    }
    ((char *)ptr)[newVal + sizeof(int)] = character;
    printf("%c: %d\n", character, newVal);
}

int main(int argc, char **argv)
{
    int fd, i, nloop;
    int *ptr;
    size_t mapSize;

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <pathname> <#loops>\n", argv[0]);
        exit(1);
    }
    nloop = atoi(argv[2]);
    mapSize = sizeof(int) + nloop * 2;

    // Disable buffering for stdout to ensure that the output is printed immediately.
    setbuf(stdout, NULL);

    // Open the provided file and resize it to the required size.
    if ((fd = open(argv[1], O_RDWR | O_CREAT, FILE_MODE)) < 0)
    {
        perror("open failed");
        exit(1);
    }
    if (ftruncate(fd, 0) < 0 || ftruncate(fd, mapSize) < 0)
    {
        perror("Failed to reset file");
        exit(1);
    }

    // Map the file to memory.
    ptr = mmap(NULL, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED)
    {
        perror("Mmap failed");
        exit(1);
    }
    if (close(fd) < 0)
    {
        perror("Close failed");
        exit(1);
    }

    // Create 3 locks for the parent and child to use.
    my_lock_init();

    // Acquire locks for 1 and 2 for both parent and child to ensure that when the child starts, the parent has locks to block
    // the child from starting until the parent has released the locks.
    my_lock_wait1();
    my_lock_wait2();

    createSignalHandler();

    // Fork the child process.
    pid_t pid;
    if ((pid = fork()) < 0)
    {
        perror("Fork failed");
        exit(1);
    }
    if (pid == 0) // Child
    {
        // Signal the parent that the child has started meaning that the child has acquired the inherited locks.
        kill(getppid(), SIGUSR1);

        // Release the initial lock to allow the parent to start.
        my_lock_release2();

        for (i = 0; i < nloop; i++)
        {
            // Idea explained in exercise 4 pdf.
            my_lock_wait1();
            my_lock_release3();
            my_lock_wait2();
            processOperation(ptr, mapSize, 'B');
            my_lock_release1();
            my_lock_wait3();
            my_lock_release2();
        }

        my_lock_release3();

        exit(0);
    }
    // Parent

    // Wait for the child to start to ensure that the child has acquired the inherited locks.
    // If waiting would not be done, the parent would happily aquire all the three locks and would do multiple operations before the child would even start.
    while (!childStarted)
    {
        pause();
    }

    // Release the initial lock to allow the child to start.
    my_lock_release1();

    for (i = 0; i < nloop; i++)
    {
        // Idea explained in exercise 4 pdf.
        my_lock_wait2();
        my_lock_release1();
        my_lock_wait3();
        processOperation(ptr, mapSize, 'A');
        my_lock_release2();
        my_lock_wait1();
        my_lock_release3();
    }

    my_lock_release1();

    // Wait for the child to finish.
    if (waitpid(pid, NULL, 0) < 0)
    {
        perror("Waitpid failed");
        exit(1);
    }

    exit(0);
}