#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#define BUF_SIZE 128

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <src_file> <dst_file>\n", prog);
    fprintf(stderr, "Example: %s /path/to/source.txt /path/to/destination.txt\n", prog);
}


/// @brief Writes all data to a file descriptor
/// @param fd 
/// @param buf 
/// @param count 
/// @return 
static int write_all(int fd, const char *buf, size_t count)
{
    size_t total_written = 0;
    while(total_written < count)
    {
        ssize_t ret = write(fd,buf,count - total_written);
        if(ret < 0)
        {
            if(errno == EINTR)
            {
                continue; // Interrupted by signal, retry
            }
            perror("write");
            return -1;
        }
        total_written += ret;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    const char *src_file;
    const char *dst_file;
    int src_fd;
    int dst_fd;
    ssize_t ret;


    char buffer[BUF_SIZE] = {0};

    if(argc < 3)
    {
        usage(argv[0]);
        return 1;
    }

    src_file = argv[1];
    dst_file = argv[2];

    src_fd = open(src_file,O_RDONLY);
    if(src_fd < 0)
    {
        perror("open src_file");
        close(src_fd);
        return 1;
    }

    dst_fd = open(dst_file,O_WRONLY | O_CREAT | O_TRUNC,0644);
    if(dst_fd < 0)
    {
        perror("open dst_file");
        close(src_fd);
        return 1;
    }

    while(1)
    {
        ret = read(src_fd,buffer,sizeof(buffer));
        if(ret < 0)
        {
            if(errno == EINTR)
            {
                continue; // Interrupted by signal, retry
            }
            perror("read");
            close(src_fd);
            close(dst_fd);
            return 1;
        }

        if(ret == 0) break; // End of file
        if(write_all(dst_fd, buffer, ret) < 0)
        {
            perror("write");
            close(src_fd);
            close(dst_fd);
            return 1;
        }
    }

    close(src_fd);
    close(dst_fd);
    return 0;

}