#include <syslog.h>

int main(__attribute__((unused)) int argc, __attribute__((unused)) char* argv[])
{
    syslog(LOG_INFO, "Hello, world!\n");
}