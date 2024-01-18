#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

int64_t getTimeSinceLastCall()
{
    static int64_t previous = 0;

    struct timeval tv;

    if (gettimeofday(&tv, NULL) < 0)
    {
        perror("Failed to get time of day");
        exit(1);
    }

    int64_t now = tv.tv_sec * 1000000 + tv.tv_usec;
    int64_t timeTaken = now - previous;
    previous = now;

    return timeTaken;
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char* argv[], char* environ[])
{
    getTimeSinceLastCall();

    pid_t pid = getpid();
    pid_t ppid = getppid();

    int64_t pidTakeTime = getTimeSinceLastCall();

    printf("PID: %d\n", pid);
    printf("PPID: %d\n", ppid);
    printf("Time to get PID and PPID: %ldus\n", pidTakeTime);

    printf("\nEnvironment variables:\n");

    getTimeSinceLastCall();

    size_t envPointer = 0;
    while (environ[envPointer] != NULL)
    {
        printf("%s\n", environ[envPointer]);
        envPointer++;
    }

    int64_t environmentPrintTime = getTimeSinceLastCall();

    printf(
        "\nTime to print environment variables: %ldus\n", environmentPrintTime
    );
}
