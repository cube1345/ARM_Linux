#ifndef __HELLO_CHAR_H
#define __HELLO_CHAR_H

#include <linux/ioctl.h>

// 魔数分类标识
#define HELLO_IOC_MAGIC 'h'

// _IO: 不传数据，只发命令
#define HELLO_IOC_CLEAR _IO(HELLO_IOC_MAGIC,0)
// _IOR: 驱动向用户发送数据,int是GET_LEN返回的数据类型
#define HELLO_IOC_GET_LEN _IOR(HELLO_IOC_MAGIC,1,int)

#endif
