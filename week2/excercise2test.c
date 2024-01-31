#include <unistd.h>

int main()
{
    if (pause() < 0)
        return 1;
    return 0;
}
