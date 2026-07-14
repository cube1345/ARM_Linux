#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/hello_char"
#define BUFFER_SIZE 128

int main(void)
{
    char buffer[BUFFER_SIZE] = {0};
    struct pollfd pfd;
    int fd;
    int ret;
    ssize_t n;

    fd = open(DEVICE_PATH,O_RDONLY | O_NONBLOCK);
    if(fd < 0)
    {
        perror("open");
        return 1;
    }

    n = read(fd,buffer,sizeof(buffer) - 1);
    if(n < 0 && errno == EAGAIN) printf("nonblock read no data now \n");
    else if(n < 0)
    {
        perror("read");
        close(fd);
        return 1;
    }

    pfd.fd = fd;
    pfd.events = POLLIN;
    printf("poll: waiting for data \n");
    ret = poll(&pfd,1,10000);
  	if (ret < 0) {
  		perror("poll");
  		close(fd);
  		return 1;
  	}

  	if (ret == 0) {
  		printf("poll: timeout\n");
  		close(fd);
  		return 0;
  	}

  	if (pfd.revents & POLLIN) {
  		memset(buffer, 0, sizeof(buffer));
  		n = read(fd, buffer, sizeof(buffer) - 1);
  		if (n < 0) {
  			perror("read after poll");
  			close(fd);
  			return 1;
  		}

  		printf("read after poll: %s", buffer);
  	}

  	close(fd);
  	return 0;

}