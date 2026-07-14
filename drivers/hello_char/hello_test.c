#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include "hello_char.h"

#define DEVICE_PATH "/dev/hello_char"
#define BUFFER_SIZE 128

int main(int argc,char** argv)
{
    const char *msg = argc > 1 ? argv[1] : "hello from user app\n";
    char buffer[BUFFER_SIZE] = {0};
    int fd;
    ssize_t n;
    int len = 0;

    fd = open(DEVICE_PATH,O_RDWR);
    if (fd < 0)
    {
        perror("open");
        return 1;
    }
    
    n = read(fd,buffer,sizeof(buffer) - 1);
    
    if(n < 0) 
    {
        perror("read");
        close(fd);
        return 1;
    }
    printf("initial read: %s",buffer);

    if(ioctl(fd,HELLO_IOC_GET_LEN,&len) < 0)
    {
        perror("ioctl GET_LEN");
        close(fd);
        return 1;
    }
    printf("inital len: %d\n",len);


    n = write(fd,msg,strlen(msg));
    if(n < 0)
    {
        perror("write");
        close(fd);
        return 1;
    }
    if(lseek(fd,0,SEEK_SET) < 0)
    {
        perror("lseek");
        close(fd);
        return 1;
    }

    memset(buffer,0,sizeof(buffer));

    n = read(fd,buffer,sizeof(buffer) - 1);
    if(n < 0)
    {
        perror("read after write");
        close(fd);
        return 1;
    }
    printf("after write: %s",buffer);

    if(ioctl(fd,HELLO_IOC_CLEAR) < 0)
    {
        perror("ioctl CLEAR");
        close(fd);
        return 1;
    }
    if(lseek(fd,0,SEEK_SET) < 0)
    {
        perror("lseek after clear");
        close(fd);
        return 1;
    }
    memset(buffer,0,sizeof(buffer) - 1);

    n = read(fd,buffer,sizeof(buffer) - 1);
    if(n < 0)
    {
        perror("read after clear");
        close(fd);
        return 1;
    }
    printf("after clear bytes: %zd",n);

    close(fd);
    return 0;
}