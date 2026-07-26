#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define BUFFER_SIZE 128
#define RETRY_LIMIT 5

static int prepare_fifo(const char *fifo_path)
{
    struct stat st;
    if(mkfifo(fifo_path, 0666) == -1)  return 0;

    if(errno != EEXIST) {
        perror("mkfifo");
        return -1;
    }

    if(stat(fifo_path, &st) < 0)
    {
        perror("stat");
        return -1;
    }
    if(!S_ISFIFO(st.st_mode)) {
        fprintf(stderr, "%s is not a FIFO\n", fifo_path);
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    char buffer[BUFFER_SIZE];
    int fd;

    char *fifo_path = argv[1];
    if(argc < 2)
    {
        fprintf(stderr, "Usage: %s <fifo_path>\n", argv[0]);
        return 1;
    }

    if(prepare_fifo(&fifo_path) < 0)  return 1;

    fd = open(fifo_path, O_RDONLY | O_NONBLOCK);
    if(fd < 0)
    {
        perror("open");
        return 1;
    }

    for(int i = 0; i < RETRY_LIMIT; i++)
    {
        ssize_t ret = read(fd,buffer, BUFFER_SIZE - 1);
        if(ret > 0)
        {
            buffer[ret] = '\0';
            printf("Read from FIFO: %s\n", buffer);

        }
        else if(ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            printf("No data acailable retrying...\n");

        }
        else if(ret < 0)
        {
            perror("read");
            close(fd);
            return 1;
        }

        sleep(1);
        
    }
    close(fd);
    return 0;
}