#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

// Returns the time since the last call to this function in microseconds.
// Implemented using gettimeofday and saving the previous time in a static variable.
int64_t getTimeSinceLastCall()
{
    static int64_t previous = 0;

    struct timeval tv;

    if (gettimeofday(&tv, NULL) < 0)
    {
        perror("Failed to get time of day");
        exit(1);
    }

    // Calculate the time since the last call to this function in microseconds and save the current time.
    int64_t now = tv.tv_sec * 1000000 + tv.tv_usec;
    int64_t timeTaken = now - previous;
    previous = now;

    return timeTaken;
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char* argv[], char* environ[])
{
    getTimeSinceLastCall();

    // Get PIDs.
    pid_t pid = getpid();
    pid_t ppid = getppid();

    int64_t pidTakeTime = getTimeSinceLastCall();

    // Print PIDs and time taken to get them.
    printf("PID: %d\n", pid);
    printf("PPID: %d\n", ppid);
    printf("Time to get PID and PPID: %ldus\n", pidTakeTime);

    printf("\nEnvironment variables:\n");

    getTimeSinceLastCall();

    // Loop through the environment variables by printing each one until the next one is NULL.
    size_t envPointer = 0;
    while (environ[envPointer] != NULL)
    {
        printf("%s\n", environ[envPointer]);
        envPointer++;
    }

    int64_t environmentPrintTime = getTimeSinceLastCall();

    // Print time taken to print environment variables.
    printf(
        "\nTime to print environment variables: %ldus\n", environmentPrintTime
    );
}
