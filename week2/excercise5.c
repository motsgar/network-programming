

void reader(int fd)
{
    char buf[1024];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
    {
        write(STDOUT_FILENO, buf, n);
    }
}

void doubler(int fd)
{
    char buf[1024];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
    {
        write(STDOUT_FILENO, buf, n);
        write(STDOUT_FILENO, buf, n);
    }
}

int main(int argc, int* argv[])
{
}