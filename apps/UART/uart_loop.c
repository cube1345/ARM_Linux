#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

static void usage(const char* prog)
{
    printf("Usage %s <device> <baudrate>\n",prog);
    printf("Example %s /dev/ttyAMA0 115200 hello\n",prog);
}

static speed_t uart_b2s(int baudrate)
{
    switch (baudrate) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    default:
        return 0;
    }
}

static int uart_config(int fd,int baudrate)
{
    struct termios tio;
    speed_t speed;

    speed = uart_b2s(baudrate);
    if(!speed)
    {
        fprintf(stderr,"unspported baudrate: %d\n",baudrate);
        return -1;
    }

    if(tcgetattr(fd,&tio) < 0 )
    {
        perror("tcgetattr");
        return -1;
    }

    
    cfmakeraw(&tio);

    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);

    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;
    tio.c_cflag &= ~PARENB;
    tio.c_cflag &= ~CSTOPB;
    tio.c_cflag &= ~CRTSCTS;

    // VMIN = 1:至少收到1个字节数据才返回，VTIME = 0:不使用定时器
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;

    // 清空输入缓冲区
    tcflush(fd, TCIOFLUSH);

    if(tcsetattr(fd,TCSANOW,&tio) < 0)
    {
        perror("tcsetattr");
        return -1;
    }

    return 0;
}


int main(int argc,char** argv)
{
    const char* device;
    int baudrate;
    int fd;
    char buf[256];

    if(argc < 3)
    {
        usage(argv[0]);
        return -1;
    }
    device = argv[1];
    baudrate = atoi(argv[2]);

    fd = open(device,O_RDWR | O_NOCTTY);
    if(fd < 0)
    {
        perror("open");
        return -1;
    }

    if(uart_config(fd,baudrate) < 0)
    {
        perror("uart_config");
        return -1;
    }

    printf("uart_loop started, device: %s, baudrate: %d\n",device,baudrate);

    while(1)
    {
        ssize_t ret;

        memset(buf,0,sizeof(buf));

        ret = read(fd,buf,sizeof(buf));
        if(ret < 0)
        {
            if(errno == EINTR)
                continue;

            perror("read");
            break;
        }

        printf("recv %zd bytes: ",ret);

        for(int i = 0; i < ret; i++)
        {
            printf("%02X ",(unsigned char)buf[i]);
        }
        printf("\n");

        // 将接收到的数据中的buf个字节原样输出到标准输出
        fwrite(buf,1,ret,stdout);
        printf("\n");
        fflush(stdout);
    }

    close(fd);
    return 0;
}
