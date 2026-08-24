#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define ZERO_READ_SIZE 16

static void usage(const char *prog)
{
    printf("Usage:\n");
    printf("  %s null\n", prog);
    printf("  %s zero\n", prog);
    printf("\nExample:\n");
    printf("  %s null\n", prog);
    printf("  %s zero\n", prog);
}

static int test_dev_null(void)
{
    int fd;
    ssize_t ret;
    char buf[16];
    const char *text = "hello /dev/null\n";

    fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        perror("open /dev/null");
        return 1;
    }

    ret = write(fd, text, strlen(text));
    if (ret < 0) {
        perror("write /dev/null");
        close(fd);
        return 1;
    }

    printf("write /dev/null bytes: %ld\n", ret);

    ret = read(fd, buf, sizeof(buf));
    if (ret < 0) {
        perror("read /dev/null");
        close(fd);
        return 1;
    }

    printf("read /dev/null bytes : %ld\n", ret);
    printf("read 0 means EOF\n");

    close(fd);
    return 0;
}

static int test_dev_zero(void)
{
    int fd;
    ssize_t ret;
    unsigned char buf[ZERO_READ_SIZE];

    fd = open("/dev/zero", O_RDONLY);
    if (fd < 0) {
        perror("open /dev/zero");
        return 1;
    }

    ret = read(fd, buf, sizeof(buf));
    if (ret < 0) {
        perror("read /dev/zero");
        close(fd);
        return 1;
    }

    printf("read /dev/zero bytes: %ld\n", ret);
    printf("data:");

    for (ssize_t i = 0; i < ret; i++) {
        printf(" %02x", buf[i]);
    }

    printf("\n");

    close(fd);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "null") == 0) {
        return test_dev_null();
    }

    if (strcmp(argv[1], "zero") == 0) {
        return test_dev_zero();
    }

    usage(argv[0]);
    return 1;
}