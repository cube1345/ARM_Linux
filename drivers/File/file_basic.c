#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#define BUFFER_SIZE 128

static void usage(const char *prog)
{
    printf("Usage: %s <file_path> <text>\n",prog);
}

int main(int argc,char * argv[])
{
    const char *file_path;
    const char *text;
    int fd;
    ssize_t ret;
    char buf[BUFFER_SIZE] = {0};

    if(argc < 3)
    {
        usage(argv[0]);
        return 1;
    }

    file_path = argv[1];
    text = argv[2];

    // 0644：文件所有者可读写，组用户可读，其他用户可读
    // O_CREAT：如果文件不存在则创建
    // O_TRUNC：如果文件存在则清空文件内容
    fd = open(file_path,O_RDWR | O_CREAT | O_TRUNC,0644);
    if(fd < 0)
    {
        perror("open");
        return 1;
    }

    ret = write(fd,text,strlen(text));
    if(ret < 0)
    {
        perror("write");
        close(fd);
        return 1;
    }

    // 将文件偏移量移动到文件开头
    ret = lseek(fd,0,SEEK_SET);
    if(ret < 0)
    {
        perror("lseek");
        close(fd);
        return 1;
    }

    memset(buf,0,sizeof(buf));
    ret = read(fd,buf,sizeof(buf) - 1);
    if(ret < 0)
    {
        perror("read");
        close(fd);
        return 1;
    }
    buf[ret] = '\0';
    printf("Read from file: %s\n", buf);
    close(fd);
    return 0;
}