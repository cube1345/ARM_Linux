#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUF_SIZE 128
#define POLL_TIMEOUT_MS 5000

static int prepare_fifo(const char *path)
{
    struct stat st;

    if (mkfifo(path, 0666) == 0) {
        return 0;
    }

    if (errno != EEXIST) {
        perror("mkfifo");
        return -1;
    }

    if (stat(path, &st) < 0) {
        perror("stat");
        return -1;
    }

    if (!S_ISFIFO(st.st_mode)) {
        fprintf(stderr, "%s exists but is not a FIFO\n", path);
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    struct pollfd pfd;
    char buf[BUF_SIZE];
    int fd;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <fifo_path>\n", argv[0]);
        return 1;
    }

    if (prepare_fifo(argv[1]) < 0) {
        return 1;
    }

    /*
    * O_RDWR: 防止没有写端时 read() 直接返回 EOF
    * O_NONBLOCK: 防止 poll 返回后 read() 因特殊情况再次阻塞
    */
    fd = open(argv[1], O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    pfd.fd = fd;
    pfd.events = POLLIN;

    while (1) {
        int ret;

        printf("waiting for data...\n");

        ret = poll(&pfd, 1, POLL_TIMEOUT_MS);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("poll");
            close(fd);
            return 1;
        }

        if (ret == 0) {
            printf("poll timeout, no data\n");
            continue;
        }

        if (pfd.revents & POLLIN) {
            ssize_t nread;

            memset(buf, 0, sizeof(buf));

            nread = read(fd, buf, sizeof(buf) - 1);
            if (nread < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    printf("data not ready, try again\n");
                    continue;
                }

                perror("read");
                close(fd);
                return 1;
            }

            if (nread == 0) {
                printf("read EOF\n");
                continue;
            }

            printf("received %zd bytes: %s\n", nread, buf);
        }

        if (pfd.revents & POLLERR) {
            printf("poll error event\n");
        }

        if (pfd.revents & POLLHUP) {
            printf("poll hang up event\n");
        }
    }

    close(fd);
    return 0;
}
