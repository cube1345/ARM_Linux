#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>


/* 寄存器物理地址 */
#define CCM_CCGR1_BASE				(0X020C406C)	
#define SW_MUX_GPIO1_IO03_BASE		(0X020E0068)
#define SW_PAD_GPIO1_IO03_BASE		(0X020E02F4)
#define GPIO1_DR_BASE				(0X0209C000)
#define GPIO1_GDIR_BASE				(0X0209C004)

/* newchrled设备结构体 */
struct newchrled_dev{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;		/* 类 		*/
	struct device *device;	/* 设备 	 */
	int major;				/* 主设备号	  */
	int minor;				/* 次设备号   */
};


struct newchrled_dev newcheled;

static void led_switch(u8 sta)
{
    u32 val = 0;
    if(sta == LEDON) {
        val = readl(GPIO1_DR);
        val &= ~(1 << 3);
        writel(val, GPIO1_DR);
    }
    else if(sta == LEDOFF) {
        val = readl(GPIO1_DR);
        val|= (1 << 3);
        writel(val, GPIO1_DR);
    }   
}

static int led_open(struct inode *inode, struct file *filp)
{
    //把当前打开的设备对应的驱动对象保存到这个文件描述符的私有区域中
    filp->private_data = &newcheled;
    return 0;
}

static ssize_t led_read(struct file *filp, char __user *buf,size_t cnt, loff_t *offt)
{
    return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int retvalue;
    unsigned char databuf[1];

    retvalue = copy_from_user(databuf, buf, sizeof(databuf));
    if (retvalue < 0)
        return -EFAULT;

    if (databuf[0] == LEDON)
        led_switch(LEDON);

    else if (databuf[0] == LEDOFF)
        led_switch(LEDOFF);
    else
        return -EINVAL;

    return sizeof(databuf);
}

static int led_release(struct inode *inode,struct file *filp)
{
    return 0;
}

static struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .read = led_read,
    .write = led_write,
    .release = led_release,
};
static int __init led_init(void)
{

    u32 val = 0;
    IMX6U_CCM_CCGR1 = ioremap(CCM_CCGR1_BASE, 4);
    SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
    SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
    GPIO1_DR = ioremap(GPIO1_DR_BASE, 4);
    GPIO1_GDIR = ioremap(GPIO1_GDIR_BASE, 4);

    if (!IMX6U_CCM_CCGR1 || !SW_MUX_GPIO1_IO03 || !SW_PAD_GPIO1_IO03 ||
        !GPIO1_DR || !GPIO1_GDIR) {
        retvalue = -ENOMEM;
        goto err_unmap;
    }

    val = readl(IMX6U_CCM_CCGR1);
    val &= ~(3 << 26); /* 清除以前的设置 */
    val |= (3 << 26); /* 设置新值 */
    writel(val, IMX6U_CCM_CCGR1);    
    writel(5,SW_MUX_GPIO1_IO03);
    writel(0X10B0,SW_PAD_GPIO1_IO03);

    /* 4、设置 GPIO1_IO03 为输出功能 */
    val = readl(GPIO1_GDIR);
    val &= ~(1 << 3); /* 清除以前的设置 */
    val |= (1 << 3); /* 设置为输出 */
    writel(val, GPIO1_GDIR);

    val = readl(GPIO1_DR);
    val |= (1 << 3);
    writel(val, GPIO1_DR);    

    // 创建主设备号
    if(newcheled.major)
    {
        newcheled.devid = MKDEV(newcheled.major, 0);
        register_chrdev_region(newcheled.devid, NEWCHRLED_CNT,NEWCHRLED_NAME);
    }
    else
    {
        alloc_chrdev_region(&newcheled.devid,0,NEWCHRLED_CNT,NEWCHRLED_NAME);
    }
    printk("newcheled major=%d,minor=%d\r\n",newchrled.major, newchrled.minor);	

    //初始化cdev
    newcheled.cdev.owner  = THIS_MODULE;
    cdev_init(&newcheled.cdev, &newchrled_fops);

    cdev_add(&newcheled.cdev,newcheled.devid,NEWCHRLED_CNT);

    newcheled.class = class_create(THIS_MODULE,NEWCHRLED_NAME);
    if(IS_ERR(newcheled.class)) return PTR_ERR(newcheled.class);

    newcheled.device = device_create(newcheled.class,NULL,newcheled.devid,NULL,NEWCHRLED_NAME);
    if(IS_ERR(newcheled.device)) return PTR_ERR(newcheled.device);

    return 0;

err_unmap:
    if (IMX6U_CCM_CCGR1) iounmap(IMX6U_CCM_CCGR1);
    if (SW_MUX_GPIO1_IO03) iounmap(SW_MUX_GPIO1_IO03);
    if (SW_PAD_GPIO1_IO03) iounmap(SW_PAD_GPIO1_IO03);
    if (GPIO1_DR) iounmap(GPIO1_DR);
    if (GPIO1_GDIR) iounmap(GPIO1_GDIR);
    return retvalue;
}

static void __exit led_exit(void)
{
    /* 取消映射 */
    iounmap(IMX6U_CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);
    /* 注销字符设备驱动 */
    unregister_chrdev(LED_MAJOR, LED_NAME);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("cube");