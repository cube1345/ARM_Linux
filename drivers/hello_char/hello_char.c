#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/sched/signal.h>
#include <linux/poll.h>

#include "hello_char.h"

// 创建设备名
#define DEVICE_NAME "hello_char"
// 内核缓冲区大小
#define BUFFER_SIZE 128

// 保存主设备号和次设备号
static dev_t hello_dev;

// 这个是字符设备在内核的对象
static struct cdev hello_cdev;

// 创建/dev/hello_char的设备节点
static struct class *hello_class;

// 内核的缓冲区
static char hello_buffer[BUFFER_SIZE] = "hello from kernel!\n";

// 防止多个进程读取hello_buffer，mutex锁
static DEFINE_MUTEX(hello_lock);

static wait_queue_head_t read_queue;
static int data_ready;

/// @brief open() & close() 设备时调用该函数
/// @param inode 
/// @param file 
/// @return  
static int hello_open(struct inode * inode , struct file* file)
{
    pr_info("hello_char: device opened\n");
    return 0;
}

static int hello_release(struct inode * inode , struct file* file)
{
    pr_info("hello_char: device closed\n");
    return 0;
}

/// @brief 从hello_buffer的内容通过copy_to_user拷贝给用户态
/// @param file 
/// @param user_buf 
/// @param count 
/// @param ppos 文件偏移，表示读取的位置，读完则返回0
/// @return 
static ssize_t hello_read(struct file *file,char __user *user_buf,size_t count,loff_t* ppos)
{
        size_t len;
        size_t available;
        ssize_t ret;

        
        // 阻塞式等待
        // if(!*ppos)
        // {
        //     ret = wait_event_interruptible(read_queue,data_ready);
        //     if(ret) return ret;
        // }

        //非阻塞式等待
        if(!*ppos)
        {
            if(!data_ready)
            {
                // 没有数据的话立刻返回 -EAGAIN
                if(file -> f_flags & O_NONBLOCK) return -EAGAIN;
                ret = wait_event_interruptible(read_queue,data_ready);
                if(ret) return ret;
            }
            
        }

        // 当接受到write进程写入的数据后，数据空间只能由read进程掌有
        mutex_lock(&hello_lock);
        len = strnlen(hello_buffer,BUFFER_SIZE);
        if(*ppos >= len)
        {
            data_ready = 0;
            ret = 0;
            goto out;
        }

        available = len - *ppos;

        // 用户读取count个字节，但是不会超过可获得的长度。
        if(count > available) count = available;

        if(copy_to_user(user_buf,hello_buffer + *ppos, count)) 
        {
            ret = -EFAULT;
            goto out;
        }
        *ppos += count;
        ret = count;

        // 读取完全之后清状态
        if(*ppos >= len) 
        {
            data_ready = 0;
        }

out:    mutex_unlock(&hello_lock);
        return ret;
}

// 将用户态的数据拷贝进hello_buffer
static ssize_t hello_write(struct file *file, const char __user *user_buf,size_t count,loff_t *ppos)
{
    size_t len;

    if(!count) return 0;

    len = min(count,(size_t)BUFFER_SIZE - 1);

    mutex_lock(&hello_lock);

    if(copy_from_user(hello_buffer,user_buf,len))
    {
        mutex_unlock(&hello_lock);
        return -EFAULT;
    }
    hello_buffer[len] = '\0';
    data_ready = 1;
    mutex_unlock(&hello_lock);
    wake_up_interruptible(&read_queue);
    return len;
}

/// @brief 当前这个设备是否有数据可以读？
/// @param file 当前打开的设备文件
/// @param wait 传进来的等待表
/// @return 
static __poll_t hello_poll(struct file* file,poll_table* wait)
{
    __poll_t mask = 0;
    // 将当前进程登记到等待队列上
    poll_wait(file,&read_queue,wait);
    mutex_lock(&hello_lock);

    // 新旧版本的“有新数据可以读”
    if(data_ready) mask |= POLLIN | POLLRDNORM;
    mutex_unlock(&hello_lock);
    return mask;

}

static long hello_ioctl(struct file* file,unsigned int cmd,unsigned long arg)
{
    int len;
    int ret = 0;
    mutex_lock(&hello_lock);

    switch (cmd)
    {
    case HELLO_IOC_CLEAR:
        /* code */
        hello_buffer[0] = '\0';
        pr_info("hello_char: buffer cleared\n");
        data_ready = 0;
        break;
    case HELLO_IOC_GET_LEN:
        len = strnlen(hello_buffer, BUFFER_SIZE);
        if(copy_to_user((int __user *)arg,&len,sizeof(len))) ret = -EFAULT;
        break;
    default:
        ret = -ENOTTY;
        break;
    }
    mutex_unlock(&hello_lock);
    return ret;
}

// 把用户态的操作函数映射到驱动函数
static const struct file_operations hello_fops = {
    .owner = THIS_MODULE,
    .open = hello_open,
    .release = hello_release,
    .read = hello_read,
    .write = hello_write,
    .llseek = default_llseek,
    .unlocked_ioctl = hello_ioctl,
    .poll = hello_poll,
};

static int __init hello_char_init(void)
{
        int ret;
        struct device* device;
        init_waitqueue_head(&read_queue);
        data_ready = 0;

        // 设备号动态申请
        ret = alloc_chrdev_region(&hello_dev,0,1,DEVICE_NAME);
        if(ret) return ret;
        
        // 初始化字符设备对象
        cdev_init(&hello_cdev,&hello_fops);
        hello_cdev.owner = THIS_MODULE;

        // 把字符设备注册进内核
        ret = cdev_add(&hello_cdev,hello_dev,1);
        if(ret) goto unregister_chrdev; // 这步失败了，就把cdev删除

        // 创建class，用于生成/dev/hello_char
        hello_class = class_create(THIS_MODULE,DEVICE_NAME);
        if(IS_ERR(hello_class)) 
        {
            ret = PTR_ERR(hello_class);
            goto delete_cdev;
        }

        // 创建设备节点，会得到 /dev/hello_char
        device = device_create(hello_class,NULL,hello_dev,NULL,DEVICE_NAME);
        if(IS_ERR(device))
        {
            ret = PTR_ERR(device);
            goto destroy_class;
        }
        return 0;

// 如果中途遇到错误，则顺序向下执行，释放空间，删除设备，避免设备号泄露，数据残留等问题。
destroy_class:
        class_destroy(hello_class);
delete_cdev:
        cdev_del(&hello_cdev);
unregister_chrdev:
        unregister_chrdev_region(hello_dev,1);
        return ret;
}

static void __exit hello_char_exit(void)
{
    device_destroy(hello_class,hello_dev);
    class_destroy(hello_class);
    cdev_del(&hello_cdev);
    unregister_chrdev_region(hello_dev,1);
}



// 内核加载模块时跑哪个函数，卸载时跑哪个函数
module_init(hello_char_init);
module_exit(hello_char_exit);


// 声明许可证
MODULE_LICENSE("GPL");
MODULE_AUTHOR("cube");
MODULE_DESCRIPTION("A simple Hello World LKM!");

