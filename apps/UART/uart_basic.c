#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static void usage(const char* prog)
{
    printf("Usage %s <device> <baudrate> <send_string>\n",prog);
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

    // 读取串口配置
    if(tcgetattr(fd,&tio) < 0 )
    {
        perror("tcgetattr");
        return -1;
    }

    // 设置 raw mode    
    cfmakeraw(&tio);

    // 设置波特率
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);

    
    // 配置 raw mode
    tio.c_cflag |= (CLOCAL | CREAD); // 忽略 modem 控制线，接收使能
    tio.c_cflag &= ~CSIZE; // 清除数据位掩码
    tio.c_cflag |= CS8; // 设置数据位 8
    tio.c_cflag &= ~PARENB; // 无奇偶校验
    tio.c_cflag &= ~CSTOPB; // 1 个停止位
    tio.c_cflag &= ~CRTSCTS; // 无硬件流控制

    // read
    tio.c_cc[VMIN] = 1; // 最少读取 1 个字节
    tio.c_cc[VTIME] = 10; // 最多等待1s

    // 清空缓存区域
    tcflush(fd, TCIOFLUSH);

    // 使能配置
    if(tcsetattr(fd,TCSANOW,&tio) < 0)
    {
        perror("tcsetattr");
        return -1;
    }

    return 0;
}

int main(int argc, char* argv[])
{
    const char *device;
    const char *send_string;
    int baudrate;
    int fd;

    ssize_t ret;

    char recv_buf[256] = {0};

    if(argc < 4)
    {
        usage(argv[0]);
        return 1;
    }

    device = argv[1];
    baudrate = atoi(argv[2]);
    send_string = argv[3];

    // 打开串口设备
    fd = open(device,O_RDWR | O_NOCTTY );
    if(fd < 0)
    {
        close(fd);
        return 1;
    }
    if(uart_config(fd,baudrate) < 0)
    {
        close(fd);
        return 1;
    }

    // 发送数据
    ret = write(fd,send_string,strlen(send_string));
    if(ret < 0)
    {
        perror("write");
        close(fd);
        return 1;
    }

    printf("send %zd bytes: %s\n",ret,send_string);

    
    // 读取数据
    memset(recv_buf,0,sizeof(recv_buf));
    ret = read(fd,recv_buf,sizeof(recv_buf)-1);
    if(ret < 0)
    {
        perror("read");
        close(fd);
        return 1;
    }

    printf("recv %zd bytes: %s\n",ret,recv_buf);

    close(fd);
    return 0;
}